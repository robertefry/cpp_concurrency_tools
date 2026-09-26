
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include "channel.hh"

#include <cassert>
#include <atomic>
#include <memory>
#include <new>
#include <utility>

namespace cts {

    namespace spsc {

        namespace detail {

            class IndexPolicyModulo {
                size_t capacity_;
            public:
                explicit IndexPolicyModulo(size_t capacity) noexcept
                    : capacity_{capacity}
                {
                    assert(capacity_ != 0 && "capacity cannot be zero");
                }
                [[nodiscard]] auto index(size_t index) const noexcept {
                    return index % capacity_;
                }
                [[nodiscard]] auto next(size_t index) const noexcept {
                    if (index + 1 != 0) [[likely]] { return index + 1; }
                    else return (index % capacity_) + 1;
                }
            };

            class IndexPolicyMasking {
                size_t index_mask_;
            public:
                explicit IndexPolicyMasking(size_t capacity) noexcept
                    : index_mask_{capacity - 1}
                {
                    assert(capacity != 0 && "capacity cannot be zero");
                    assert((capacity & (capacity - 1)) == 0 && "capacity must be a power-of-two");
                }
                [[nodiscard]] auto index(size_t index) const noexcept { return index & index_mask_; }
                [[nodiscard]] auto next(size_t index) const noexcept { return index + 1; }
            };

        } // namespace detail

        template <typename T
            , typename Allocator = std::allocator<T>
            , typename IndexPolicy = detail::IndexPolicyModulo
        > class RingChannel {

            friend class cts::ChannelTx<T,RingChannel<T,Allocator,IndexPolicy>>;
            friend class cts::ChannelRx<T,RingChannel<T,Allocator,IndexPolicy>>;

            using alloc_traits = std::allocator_traits<Allocator>;

            T* buffer_;
            size_t capacity_;

            [[no_unique_address]] Allocator alloc_;
            [[no_unique_address]] IndexPolicy index_policy_;

            static constexpr size_t cache_line = std::hardware_destructive_interference_size;
            alignas(cache_line) std::atomic_uint64_t tx_count_;
            alignas(cache_line) std::atomic_uint64_t rx_count_;

            struct Connection {
                RingChannel<T,Allocator,IndexPolicy> channel;
                std::atomic_flag tx_connected;
                std::atomic_flag rx_connected;

                template <typename... Args> explicit Connection(Args&&... args)
                    : channel {std::forward<Args>(args)...}
                {
                    tx_connected.clear();
                    rx_connected.clear();
                }
            };

        public:

            ~RingChannel() {
                if (buffer_ != nullptr) {
                    discard_all();
                    alloc_traits::deallocate(alloc_, buffer_, capacity());
                }
            }

            RingChannel(RingChannel&& other) noexcept
                : buffer_{std::exchange(other.buffer_, nullptr)}
                , capacity_{std::exchange(other.capacity_, 0)}
                , alloc_{std::move(other.alloc_)}
                , index_policy_{std::move(other.index_policy_)}
                , tx_count_{std::atomic_exchange_explicit(&other.tx_count_, 0, std::memory_order_relaxed)}
                , rx_count_{std::atomic_exchange_explicit(&other.rx_count_, 0, std::memory_order_relaxed)}
            {}

            RingChannel& operator=(RingChannel&& other) noexcept {
                constexpr auto atomic_swap_relaxed = [](std::atomic<T>& a, std::atomic<T>& b) {
                    auto const a_val = a->load(std::memory_order_relaxed);
                    auto const b_val = b->load(std::memory_order_relaxed);
                    a->store(b_val, std::memory_order_relaxed);
                    b->store(a_val, std::memory_order_relaxed);
                };
                std::swap(buffer_, other.buffer_);
                std::swap(capacity_, other.capacity_);
                std::swap(alloc_, other.alloc_);
                std::swap(index_policy_, other.index_policy_);
                atomic_swap_relaxed(rx_count_, other.rx_count_);
                atomic_swap_relaxed(tx_count_, other.tx_count_);
                return *this;
            }

            explicit RingChannel(
                size_t capacity
                , Allocator const& allocator = Allocator{}
            )
                : buffer_{nullptr}
                , capacity_{capacity}
                , alloc_{allocator}
                , index_policy_{capacity}
                , tx_count_{0}
                , rx_count_{0}
            {
                buffer_ = alloc_traits::allocate(alloc_, capacity);
            }

            [[nodiscard]] auto into_endpoints() && {
                using Channel = std::remove_cvref_t<decltype(*this)>;
                auto const connection = std::make_shared<typename Channel::Connection>(std::move(*this));
                return std::tuple{
                    ChannelTx<T,Channel>{connection}, ChannelRx<T,Channel>{connection}
                };
            }

            [[nodiscard]] auto capacity() const noexcept -> size_t {
                return capacity_;
            }

            [[nodiscard]] auto size() const noexcept -> size_t {
                auto const tx_count = tx_count_.load(std::memory_order_acquire);
                auto const rx_count = rx_count_.load(std::memory_order_acquire);
                return tx_count - rx_count;
            }

            [[nodiscard]] auto is_empty() const noexcept { return size() == 0; }
            [[nodiscard]] auto is_full() const noexcept { return size() == capacity(); }

            void send(T const& value) { send_emplace(value); }
            void send(T&& value) { send_emplace(std::move(value)); }

            template <typename... Args>
            void send_emplace(Args&&... args) {
                assert(not is_full());
                auto tx_count = tx_count_.load(std::memory_order_relaxed);

                alloc_traits::construct(alloc_, buffer_ + index_policy_.index(tx_count),
                    std::forward<Args>(args)...
                );
                tx_count_.store(index_policy_.next(tx_count), std::memory_order_release);
            }

            [[nodiscard]] auto recv() -> T {
                assert(not is_empty());
                auto rx_count = rx_count_.load(std::memory_order_relaxed);

                auto value = std::move(buffer_[index_policy_.index(rx_count)]);
                alloc_traits::destroy(alloc_, buffer_ + index_policy_.index(rx_count));

                rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
                return value;
            }

            void discard_next() {
                assert(not is_empty());
                auto const rx_count = rx_count_.load(std::memory_order_relaxed);
                alloc_traits::destroy(alloc_, buffer_ + index_policy_.index(rx_count));
                rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
            }

            void discard_all() {
                auto tx_count = tx_count_.load(std::memory_order_acquire);
                auto rx_count = rx_count_.load(std::memory_order_relaxed);

                while (tx_count != rx_count) {
                    alloc_traits::destroy(alloc_, buffer_ + index_policy_.index(rx_count));
                    rx_count += 1;
                }
                rx_count_.store(rx_count, std::memory_order_release);
            }

        };

        template <typename T
            , typename Allocator = std::allocator<T>
        > [[nodiscard]] auto channel_bounded_fast(
            size_t capacity
            , Allocator const& allocator = Allocator{}
        ) {
            using Channel = RingChannel<T,Allocator,detail::IndexPolicyMasking>;
            return Channel{capacity, allocator};
        }

        template <typename T
            , typename Allocator = std::allocator<T>
        > [[nodiscard]] auto channel_bounded(
            size_t capacity
            , Allocator const& allocator = Allocator{}
        ) {
            using Channel = RingChannel<T,Allocator,detail::IndexPolicyModulo>;
            return Channel{capacity, allocator};
        }

    } // namespace spsc

    template <typename T, typename Allocator, typename IndexPolicy>
    class ChannelTx<T,spsc::RingChannel<T,Allocator,IndexPolicy>> {

        using Channel = spsc::RingChannel<T,Allocator,IndexPolicy>;
        friend Channel;

        std::shared_ptr<typename Channel::Connection> connection_;

        explicit ChannelTx(std::shared_ptr<typename Channel::Connection> connection)
            : connection_{std::move(connection)}
        {
            assert(static_cast<bool>(connection_) && "expected a non-null connection");
            connection_->tx_connected.test_and_set();
        }

    public:
        ~ChannelTx() {
            release();
        }

        ChannelTx(ChannelTx&& other) noexcept
            : connection_{std::exchange(other.connection_, nullptr)}
        {}

        ChannelTx& operator=(ChannelTx&& other) noexcept {
            std::swap(*this, other);
            return *this;
        };

        friend void swap(ChannelTx& a, ChannelTx& b) noexcept {
            std::swap(a.connection_, b.connection_);
        }

        void release() noexcept {
            if (connection_) { connection_->tx_connected.clear(); }
            connection_.reset();
        }

        [[nodiscard]] bool disconnected() const {
            return not connection_->rx_connected.test();
        }

        [[nodiscard]] auto size() const noexcept {
            auto const tx_count = connection_->channel.tx_count_.load(std::memory_order_relaxed);
            auto const rx_count = connection_->channel.rx_count_.load(std::memory_order_acquire);
            return tx_count - rx_count;
        }

        [[nodiscard]] auto capacity() const noexcept { return connection_->channel.capacity(); }
        [[nodiscard]] auto is_empty() const noexcept { return size() == 0; }
        [[nodiscard]] auto is_full() const noexcept { return size() == capacity(); }

        void send(T const& value) { connection_->channel.send(value); }
        void send(T&& value) { connection_->channel.send(std::move(value)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            connection_->channel.send_emplace(std::forward<Args>(args)...);
        }

    };

    template <typename T, typename Allocator, typename IndexPolicy>
    class ChannelRx<T,spsc::RingChannel<T,Allocator,IndexPolicy>> {

        using Channel = spsc::RingChannel<T,Allocator,IndexPolicy>;
        friend Channel;

        std::shared_ptr<typename Channel::Connection> connection_;

        explicit ChannelRx(std::shared_ptr<typename Channel::Connection> connection)
            : connection_{std::move(connection)}
        {
            assert(static_cast<bool>(connection_) && "expected a non-null connection");
            connection_->rx_connected.test_and_set();
        }

    public:
        ~ChannelRx() {
            release();
        }

        ChannelRx(ChannelRx&& other) noexcept
            : connection_{std::exchange(other.connection_, nullptr)}
        {}

        ChannelRx& operator=(ChannelRx&& other) noexcept {
            std::swap(*this, other);
            return *this;
        }

        friend void swap(ChannelRx& a, ChannelRx& b) noexcept {
            std::swap(a.connection_, b.connection_);
        }

        void release() noexcept {
            if (connection_) { connection_->rx_connected.clear(); }
            connection_ = nullptr;
        }

        [[nodiscard]] bool disconnected() const {
            return not connection_->tx_connected.test();
        }

        [[nodiscard]] auto size() const noexcept {
            auto const tx_count = connection_->channel.tx_count_.load(std::memory_order_acquire);
            auto const rx_count = connection_->channel.rx_count_.load(std::memory_order_relaxed);
            return tx_count - rx_count;
        }

        [[nodiscard]] auto capacity() const noexcept { return connection_->channel.capacity(); }
        [[nodiscard]] auto is_empty() const noexcept { return size() == 0; }
        [[nodiscard]] auto is_full() const noexcept { return size() == capacity(); }

        [[nodiscard]] auto recv() { return connection_->channel.recv(); }

        void discard_next() { connection_->channel.discard_next(); }
        void discard_all() { connection_->channel.discard_all(); }

    };

} // namespace cts

#endif /* CTS_SPSC_CHANNEL_HH */
