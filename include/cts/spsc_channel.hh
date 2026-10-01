
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include <cassert>
#include <memory>
#include <atomic>
#include <new>

namespace cts::spsc {

    namespace detail {

        template <typename T
            , typename Allocator = std::allocator<T>
        > class BufferPolicyDynamic {

            using alloc_traits = std::allocator_traits<Allocator>;
            static_assert(std::is_same_v<Allocator,std::decay_t<Allocator>>);
            [[no_unique_address]] Allocator alloc_;

            size_t capacity_;
            T* buffer_;

        public:

            ~BufferPolicyDynamic() {
                if (buffer_ != nullptr) {
                    alloc_traits::deallocate(alloc_, buffer_, capacity_);
                }
            }

            BufferPolicyDynamic(BufferPolicyDynamic&& other) noexcept
                : alloc_{std::move(other.alloc_)}
                , capacity_{std::exchange(other.capacity_, 0)}
                , buffer_{std::exchange(other.buffer_, nullptr)}
            {}

            BufferPolicyDynamic& operator=(BufferPolicyDynamic&& other) noexcept {
                std::swap(alloc_, other.alloc_);
                std::swap(capacity_, other.capacity_);
                std::swap(buffer_, other.buffer_);
            }

            explicit BufferPolicyDynamic(
                size_t capacity
                , Allocator allocator = Allocator{}
            )
                : alloc_{std::move(allocator)}
                , capacity_{capacity}
                , buffer_{alloc_traits::allocate(alloc_, capacity_)}
            {}

            [[nodiscard]] constexpr auto capacity() const { return capacity_; }

            template <typename... Args>
            void construct_at(size_t index, Args&&... args) {
                alloc_traits::construct(alloc_, buffer_ + index, std::forward<Args>(args)...);
            }

            [[nodiscard]] auto at(size_t index) -> T* {
                return buffer_ + index;
            }

            void destroy_at(size_t index) {
                alloc_traits::destroy(alloc_, buffer_ + index);
            }

        };

        template <typename T, size_t N
            , typename Allocator = std::allocator<T>
        > class BufferPolicyStatic {

            using alloc_traits = std::allocator_traits<Allocator>;
            static_assert(std::is_same_v<Allocator,std::decay_t<Allocator>>);
            [[no_unique_address]] Allocator alloc_;

            alignas(T) std::byte buffer_[N*sizeof(T)];

        public:

            explicit BufferPolicyStatic(
                Allocator allocator = Allocator{}
            )
                : alloc_{std::move(allocator)}
            {}

            [[nodiscard]] constexpr auto capacity() const { return N; }

            template <typename... Args>
            void construct_at(size_t index, Args&&... args) {
                auto const element = reinterpret_cast<T*>(&buffer_) + index;
                alloc_traits::construct(alloc_, element, std::forward<Args>(args)...);
            }

            [[nodiscard]] auto at(size_t index) -> T* {
                return reinterpret_cast<T*>(&buffer_) + index;
            }

            void destroy_at(size_t index) {
                alloc_traits::destroy(alloc_, reinterpret_cast<T*>(&buffer_) + index);
            }

        };

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

        template <typename T
            , typename BufferPolicy = BufferPolicyDynamic<T>
            , typename IndexPolicy = IndexPolicyModulo
        > class RingChannel {

            static_assert(std::is_same_v<BufferPolicy,std::decay_t<BufferPolicy>>);
            [[no_unique_address]] BufferPolicy buffer_;

            static_assert(std::is_same_v<IndexPolicy,std::decay_t<IndexPolicy>>);
            [[no_unique_address]] IndexPolicy index_policy_;

            static constexpr size_t cache_line = std::hardware_destructive_interference_size;
            alignas(cache_line) std::atomic_uint64_t tx_count_;
            alignas(cache_line) std::atomic_uint64_t rx_count_;

        public:

            ~RingChannel() {
                this->discard_all();
            }

            RingChannel(RingChannel&& other) noexcept
                : buffer_{std::move(other.buffer_)}
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
                std::swap(index_policy_, other.index_policy_);
                atomic_swap_relaxed(rx_count_, other.rx_count_);
                atomic_swap_relaxed(tx_count_, other.tx_count_);
                return *this;
            }

            explicit RingChannel(
                BufferPolicy buffer
            )
                : buffer_{std::move(buffer)}
                , index_policy_{buffer_.capacity()}
                , tx_count_{0}
                , rx_count_{0}
            {}

            [[nodiscard]] auto send_available() const noexcept -> size_t {
                auto const tx_count = tx_count_.load(std::memory_order_relaxed);
                auto const rx_count = rx_count_.load(std::memory_order_acquire);
                return buffer_.capacity() - (tx_count - rx_count);
            }

            void send(T const& msg) { send_emplace(msg); }
            void send(T&& msg) { send_emplace(std::move(msg)); }

            template <typename... Args>
            void send_emplace(Args&&... args) {
                assert(send_available() != 0 && "expected a free slot in the channel");
                auto tx_count = tx_count_.load(std::memory_order_relaxed);

                buffer_.construct_at(index_policy_.index(tx_count), std::forward<Args>(args)...);
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

                auto msg = std::move(*buffer_.at(index_policy_.index(rx_count)));
                buffer_.destroy_at(index_policy_.index(rx_count));

                rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
                return msg;
            }

            void discard_next() {
                assert(recv_available() != 0 && "expected a message in the channel");
                auto rx_count = rx_count_.load(std::memory_order_relaxed);

                buffer_.destroy_at(index_policy_.index(rx_count));
                rx_count_.store(index_policy_.next(rx_count), std::memory_order_release);
            }

            void discard_all() {
                auto const tx_count = tx_count_.load(std::memory_order_acquire);
                auto rx_count = rx_count_.load(std::memory_order_relaxed);

                while (tx_count != rx_count) {
                    buffer_.destroy_at(index_policy_.index(rx_count));
                    rx_count += 1;
                }
                rx_count_.store(rx_count, std::memory_order_release);
            }

        };

    } // namespace detail

    template <typename T
        , typename Allocator = std::allocator<T>
    > [[nodiscard]] auto channel_bounded_fast(
        size_t capacity
        , Allocator&& allocator = Allocator{}
    ) {
        using Buffer = detail::BufferPolicyDynamic<T,std::decay_t<Allocator>>;
        using Channel = detail::RingChannel<T,Buffer,detail::IndexPolicyMasking>;
        return Channel{Buffer{capacity, std::forward<Allocator>(allocator)}};
    }

    template <typename T
        , size_t N
        , typename Allocator = std::allocator<T>
    > [[nodiscard]] auto channel_bounded_fast(
        Allocator&& allocator = Allocator{}
    ) {
        using Buffer = detail::BufferPolicyStatic<T,N,std::decay_t<Allocator>>;
        using Channel = detail::RingChannel<T,Buffer,detail::IndexPolicyMasking>;
        return Channel{Buffer{std::forward<Allocator>(allocator)}};
    }

    template <typename T
        , typename Allocator = std::allocator<T>
    > [[nodiscard]] auto channel_bounded(
        size_t capacity
        , Allocator&& allocator = Allocator{}
    ) {
        using Buffer = detail::BufferPolicyDynamic<T,std::decay_t<Allocator>>;
        using Channel = detail::RingChannel<T,Buffer,detail::IndexPolicyModulo>;
        return Channel{Buffer{capacity, std::forward<Allocator>(allocator)}};
    }

    template <typename T
        , size_t N
        , typename Allocator = std::allocator<T>
    > [[nodiscard]] auto channel_bounded(
        Allocator&& allocator = Allocator{}
    ) {
        using Buffer = detail::BufferPolicyStatic<T,N,std::decay_t<Allocator>>;
        using Channel = detail::RingChannel<T,Buffer,detail::IndexPolicyModulo>;
        return Channel{Buffer{std::forward<Allocator>(allocator)}};
    }

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
