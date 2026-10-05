
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include <cstddef>
#include <cassert>
#include <type_traits>
#include <memory>
#include <atomic>
#include <new>

namespace cts::spsc {

    template <typename T, typename Allocator = std::allocator<T>> class RingChannelTx;
    template <typename T, typename Allocator = std::allocator<T>> class RingChannelRx;

    template <typename T
        , typename Allocator = std::allocator<T>
    > class RingChannel {

        friend class RingChannelTx<T,Allocator>;
        friend class RingChannelRx<T,Allocator>;

        using alloc_traits = std::allocator_traits<Allocator>;
        static_assert(std::is_same_v<typename alloc_traits::value_type,T>, "incompatible allocator");
        [[no_unique_address]] Allocator alloc_;

        // TODO: To be replace with a deleter.
        bool owned_by_endpoints_ = false;
        std::atomic_uint8_t endpoint_count_ = 0;

        size_t capacity_ = 0;
        T* buffer_ = nullptr;

        alignas(std::hardware_destructive_interference_size)
        std::atomic_size_t tx_head_ = 0;
        size_t mutable rx_head_cache_ = 0;
        std::atomic_flag has_rx_ = false;

        alignas(std::hardware_destructive_interference_size)
        std::atomic_size_t rx_head_ = 0;
        size_t mutable tx_head_cache_ = 0;
        std::atomic_flag has_tx_ = false;

    public:

        ~RingChannel() {
            recv_refresh();
            discard_all();
            alloc_traits::deallocate(alloc_, buffer_, capacity_);
        }

        RingChannel(RingChannel const&) = delete;
        RingChannel& operator=(RingChannel const&) = delete;

        explicit RingChannel(size_t capacity
            , Allocator allocator = Allocator{}
        )
            : alloc_{std::move(allocator)}
            , capacity_{capacity}
            , buffer_{alloc_traits::allocate(alloc_, capacity_)}
        {}

        [[nodiscard]] static auto make_endpoints(size_t capacity
            , Allocator const& allocator = Allocator{}
        ) {
            auto* channel = new RingChannel<T,Allocator>{capacity, allocator};
            channel->owned_by_endpoints_ = true;
            return std::tuple{ channel->get_send_endpoint(), channel->get_recv_endpoint() };
        }

        [[nodiscard]] auto get_send_endpoint() noexcept {
            return RingChannelTx<T,Allocator>{this};
        }

        void send_refresh() const noexcept {
            rx_head_cache_ = rx_head_.load(std::memory_order_acquire);
        }

        [[nodiscard]] size_t send_available() const noexcept {
            size_t const tx_head = tx_head_.load(std::memory_order_relaxed);
            if (capacity_ - (tx_head - rx_head_cache_) == 0) { send_refresh(); }
            return capacity_ - (tx_head - rx_head_cache_);
        }

        void send(T const& msg) { send_emplace(msg); }
        void send(T&& msg) { send_emplace(std::move(msg)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            assert(send_available() != 0 && "expected an available slot in the channel");
            size_t tx_head = tx_head_.load(std::memory_order_relaxed);

            alloc_traits::construct(alloc_, buffer_ + index(tx_head), std::forward<Args>(args)...);
            tx_head_.store(next_head(tx_head), std::memory_order_release);
        }

        [[nodiscard]] auto get_recv_endpoint() noexcept {
            return RingChannelRx<T,Allocator>{this};
        }

        void recv_refresh() const noexcept {
            tx_head_cache_ = tx_head_.load(std::memory_order_acquire);
        }

        [[nodiscard]] size_t recv_available() const noexcept {
            size_t const rx_head = rx_head_.load(std::memory_order_relaxed);
            if (tx_head_cache_ - rx_head == 0) { recv_refresh(); }
            return tx_head_cache_ - rx_head;
        }

        [[nodiscard]] auto recv() -> T {
            assert(recv_available() != 0 && "expected a message in the channel");
            size_t rx_head = rx_head_.load(std::memory_order_relaxed);

            auto msg = std::move(buffer_[index(rx_head)]);
            alloc_traits::destroy(alloc_, buffer_ + index(rx_head));

            rx_head_.store(next_head(rx_head), std::memory_order_release);
            return msg;
        }

        void discard_next() {
            assert(recv_available() != 0 && "expected a message in the channel");
            size_t rx_head = rx_head_.load(std::memory_order_relaxed);

            alloc_traits::destroy(alloc_, buffer_ + index(rx_head));
            rx_head_.store(next_head(rx_head), std::memory_order_release);
        }

        void discard_all() {
            size_t rx_head = rx_head_.load(std::memory_order_relaxed);

            while (tx_head_cache_ != rx_head) {
                alloc_traits::destroy(alloc_, buffer_ + index(rx_head));
                rx_head = next_head(rx_head);
            }
            rx_head_.store(rx_head, std::memory_order_release);
        }

    private:

        explicit RingChannel(size_t capacity, T* buffer
            , Allocator allocator = Allocator{}
        )
            : alloc_{std::move(allocator)}
            , capacity_{capacity}
            , buffer_{buffer}
        {}

        [[nodiscard]] size_t index(size_t head) const noexcept {
            return head % capacity_;
        }

        [[nodiscard]] size_t next_head(size_t head) const noexcept {
            if (head+1 > head || head+1 == 0) [[likely]] { return head + 1; }
            else { return index(head) + 1; }
        }

    };

    template <typename T, typename Allocator>
    class RingChannelTx {

        friend class RingChannel<T,Allocator>;

        RingChannel<T,Allocator>* channel_ = nullptr;

        explicit RingChannelTx(RingChannel<T,Allocator>* channel) noexcept
            : channel_{channel}
        {
            if (channel_ != nullptr) {
                channel_->endpoint_count_.fetch_add(1, std::memory_order_relaxed);
                bool duplicate = channel_->has_tx_.test_and_set(std::memory_order_acquire);
                assert(not duplicate && "duplicate endpoint creation");
            }
        }

    public:

        ~RingChannelTx() {
            detach();
        }

        RingChannelTx(RingChannelTx&& other) noexcept {
            swap(*this, other);
        }

        RingChannelTx& operator=(RingChannelTx&& other) noexcept {
            swap(*this, other); return *this;
        }

        friend void swap(RingChannelTx& a, RingChannelTx& b) noexcept {
            using std::swap;
            swap(a.channel_, b.channel_);
        }

        explicit RingChannelTx() noexcept = default;

        void detach() noexcept {
            if (channel_ != nullptr) {
                channel_->has_tx_.clear(std::memory_order_release);

                auto const count = channel_->endpoint_count_.fetch_sub(1, std::memory_order_acq_rel);
                if (count == 1 && channel_->owned_by_endpoints_) {
                    delete channel_; // TODO: Use the deleter (when implemented).
                }
            }
            channel_ = nullptr;
        }

        [[nodiscard]] bool is_detached() const noexcept {
            return channel_ == nullptr;
        }

        [[nodiscard]] bool is_connected() const noexcept {
            return channel_ != nullptr && channel_->has_rx_.test(std::memory_order_relaxed);
        }

        void refresh() const noexcept {
            assert(channel_ != nullptr && "cannot use a detached channel");
            channel_->send_refresh();
        }

        [[nodiscard]] size_t available() const noexcept {
            assert(channel_ != nullptr && "cannot use a detached channel");
            return channel_->send_available();
        }

        void send(T const& msg) { send_emplace(msg); }
        void send(T&& msg) { send_emplace(std::move(msg)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            assert(channel_ != nullptr && "cannot use a detached channel");
            channel_->send_emplace(std::forward<Args>(args)...);
        }

    };

    template <typename T, typename Allocator>
    class RingChannelRx {

        friend class RingChannel<T,Allocator>;

        RingChannel<T,Allocator>* channel_ = nullptr;

        explicit RingChannelRx(RingChannel<T,Allocator>* channel) noexcept
            : channel_{channel}
        {
            if (channel_ != nullptr) {
                channel_->endpoint_count_.fetch_add(1, std::memory_order_relaxed);
                bool duplicate = channel_->has_rx_.test_and_set(std::memory_order_acquire);
                assert(not duplicate && "duplicate endpoint creation");
            }
        }

    public:

        ~RingChannelRx() {
            detach();
        }

        RingChannelRx(RingChannelRx&& other) noexcept {
            swap(*this, other);
        }

        RingChannelRx& operator=(RingChannelRx&& other) noexcept {
            swap(*this, other); return *this;
        }

        friend void swap(RingChannelRx& a, RingChannelRx& b) noexcept {
            using std::swap;
            swap(a.channel_, b.channel_);
        }

        explicit RingChannelRx() noexcept = default;

        void detach() noexcept {
            if (channel_ != nullptr) {
                channel_->has_rx_.clear(std::memory_order_release);

                auto const count = channel_->endpoint_count_.fetch_sub(1, std::memory_order_acq_rel);
                if (count == 1 && channel_->owned_by_endpoints_) {
                    delete channel_; // TODO: Use the deleter (when implemented).
                }
            }
            channel_ = nullptr;
        }

        [[nodiscard]] bool is_detached() const noexcept {
            return channel_ == nullptr;
        }

        [[nodiscard]] bool is_connected() const noexcept {
            return channel_ != nullptr && channel_->has_tx_.test(std::memory_order_relaxed);
        }

        void refresh() const noexcept {
            assert(channel_ != nullptr && "cannot use a detached channel");
            channel_->recv_refresh();
        }

        [[nodiscard]] size_t available() const noexcept {
            assert(channel_ != nullptr && "cannot use a detached channel");
            return channel_->recv_available();
        }

        [[nodiscard]] auto recv() -> T {
            assert(channel_ != nullptr && "cannot use a detached channel");
            return channel_->recv();
        }

        void discard_next() {
            assert(channel_ != nullptr && "cannot use a detached channel");
            channel_->discard_next();
        }

        void discard_all() {
            assert(channel_ != nullptr && "cannot use a detached channel");
            channel_->discard_all();
        }

    };

    template <typename T
        , typename Allocator = std::allocator<T>
    > [[nodiscard]] auto channel_bounded(size_t capacity
        , Allocator const& allocator = Allocator{}
    ) {
        // TODO: Construct the channel and buffer contiguously.
        return RingChannel<T,Allocator>::make_endpoints(capacity, allocator);
    }

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
