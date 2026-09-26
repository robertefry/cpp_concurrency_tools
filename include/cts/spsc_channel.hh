
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include <cassert>
#include <memory>
#include <atomic>
#include <new>

namespace cts::spsc {

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

        using alloc_traits = std::allocator_traits<Allocator>;

        T* buffer_;
        size_t capacity_;

        [[no_unique_address]] Allocator alloc_;
        [[no_unique_address]] IndexPolicy index_policy_;

        static constexpr size_t cache_line = std::hardware_destructive_interference_size;
        alignas(cache_line) std::atomic_uint64_t tx_count_;
        alignas(cache_line) std::atomic_uint64_t rx_count_;

    public:

        ~RingChannel() {
            if (buffer_ != nullptr) {
                discard_all();
                alloc_traits::deallocate(alloc_, buffer_, capacity_);
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

        [[nodiscard]] auto send_available() const noexcept -> size_t {
            auto const tx_count = tx_count_.load(std::memory_order_relaxed);
            auto const rx_count = rx_count_.load(std::memory_order_acquire);
            return capacity_ - (tx_count - rx_count);
        }

        void send(T const& msg) { send_emplace(msg); }
        void send(T&& msg) { send_emplace(std::move(msg)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            assert(send_available() != 0 && "expected a free slot in the channel");
            auto tx_count = tx_count_.load(std::memory_order_relaxed);

            alloc_traits::construct(alloc_, buffer_ + index_policy_.index(tx_count),
                std::forward<Args>(args)...
            );
            tx_count_.store(index_policy_.next(tx_count), std::memory_order_release);
        }

        [[nodiscard]] auto recv_available() const noexcept -> size_t {
            auto const tx_count = tx_count_.load(std::memory_order_acquire);
            auto const rx_count = rx_count_.load(std::memory_order_relaxed);
            return tx_count - rx_count;
        }

        [[nodiscard]] auto recv() -> T {
            assert(recv_available() != 0 && "expected a message in the channel");
            auto rx_count = rx_count_.load(std::memory_order_relaxed);

            auto msg = std::move(buffer_[index_policy_.index(rx_count)]);
            alloc_traits::destroy(alloc_, buffer_ + index_policy_.index(rx_count));

            rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
            return msg;
        }

        void discard_next() {
            assert(recv_available() != 0 && "expected a message in the channel");
            auto rx_count = rx_count_.load(std::memory_order_relaxed);

            alloc_traits::destroy(alloc_, buffer_ + index_policy_.index(rx_count));
            rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
        }

        void discard_all() {
            auto const tx_count = tx_count_.load(std::memory_order_acquire);
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

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
