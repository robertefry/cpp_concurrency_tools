
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include <cstddef>
#include <cassert>
#include <type_traits>
#include <memory>
#include <atomic>
#include <new>

namespace cts::spsc {

    template <typename T
        , typename Allocator = std::allocator<T>
    > class RingChannel {

        using alloc_traits = std::allocator_traits<Allocator>;
        static_assert(std::is_same_v<typename alloc_traits::value_type,T>, "incompatible allocator");
        [[no_unique_address]] Allocator alloc_;

        size_t capacity_ = 0;
        T* buffer_ = nullptr;

        alignas(std::hardware_destructive_interference_size)
        std::atomic_size_t tx_head_ = 0;
        size_t mutable rx_head_cache_ = 0;

        alignas(std::hardware_destructive_interference_size)
        std::atomic_size_t rx_head_ = 0;
        size_t mutable tx_head_cache_ = 0;

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

        [[nodiscard]] size_t index(size_t head) const {
            return head % capacity_;
        }

        [[nodiscard]] size_t next_head(size_t head) const {
            if (head+1 > head || head+1 == 0) [[likely]] { return head + 1; }
            else { return index(head) + 1; }
        }

    };

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
