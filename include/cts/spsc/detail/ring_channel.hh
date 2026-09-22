
#ifndef CTS_SPSC_DETAIL_RING_CHANNEL_HH
#define CTS_SPSC_DETAIL_RING_CHANNEL_HH

#include "cts/spsc/channel.fwd"

#include <cstddef>
#include <cassert>
#include <atomic>
#include <memory>

namespace cts::spsc::detail {

    template <typename T, typename IndexPolicy, typename Allocator = std::allocator<T>>
    class RingChannel {

        using alloc_traits = std::allocator_traits<Allocator>;

        [[no_unique_address]] Allocator _alloc;
        [[no_unique_address]] IndexPolicy _indexer;

        T* _buffer;

        std::atomic_size_t _head;
        std::atomic_size_t _tail;

    public:

        ~RingChannel() {
            discard_all();
            alloc_traits::deallocate(_alloc, _buffer, capacity());
        }

        RingChannel(RingChannel&& other) noexcept
            : _alloc {std::move(other._alloc)}
            , _indexer {std::move(other._indexer)}
            , _buffer {std::exchange(other._buffer, nullptr)}
            , _head {std::atomic_exchange_explicit(&other._head, 0, std::memory_order_relaxed)}
            , _tail {std::atomic_exchange_explicit(&other._tail, 0, std::memory_order_relaxed)}
        {}

        RingChannel& operator=(RingChannel&& other) noexcept
        {
            if (this == &other) { return *this; }

            discard_all();
            alloc_traits::deallocate(_alloc, _buffer, capacity());

            _alloc = std::move(other._alloc); // TODO: proper threading of this allocator
            _indexer = std::move(other._indexer);
            _buffer = std::exchange(other._buffer, nullptr);
            _head = std::atomic_exchange_explicit(&other._head, 0, std::memory_order_relaxed);
            _tail = std::atomic_exchange_explicit(&other._tail, 0, std::memory_order_relaxed);

            return *this;
        }

        explicit RingChannel(
            size_t capacity,
            Allocator const& allocator = Allocator{}
        )
            : _alloc {allocator}
            , _indexer {capacity}
            , _buffer {alloc_traits::allocate(_alloc, capacity)}
            , _head {0}
            , _tail {0}
        {}

        [[nodiscard]] static auto make_endpoints(
            size_t capacity,
            Allocator const& allocator = Allocator{}
        ) {
            using Channel = RingChannel<T,IndexPolicy,Allocator>;
            auto channel = std::make_shared<Channel>(capacity, allocator);
            return std::tuple{ Sender<T,Channel>{channel}, Receiver<T,Channel>{channel} };
        }

        [[nodiscard]] auto capacity() const { return _indexer.capacity(); }

        [[nodiscard]] auto size() const -> size_t {
            auto head = _head.load(std::memory_order_acquire);
            auto tail = _tail.load(std::memory_order_acquire);
            return head - tail;
        }

        // TODO: investigate possible optimisations by endpoint caching
        [[nodiscard]] bool is_empty() const { return size() == 0; }
        [[nodiscard]] bool is_full() const { return size() == capacity(); }

        void send(T const& value) { send_emplace(value); }
        void send(T&& value) { send_emplace(std::move(value)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            assert(not is_full());
            auto head = _head.load(std::memory_order_acquire);

            alloc_traits::construct(_alloc, _buffer + _indexer.index(head), std::forward<Args>(args)...);
            _head.store(head + 1, std::memory_order_release);
        }

        [[nodiscard]] auto recv() -> T {
            assert(not is_empty());
            auto tail = _tail.load(std::memory_order_acquire);

            auto value = std::move(_buffer[_indexer.index(tail)]);

            alloc_traits::destroy(_alloc, _buffer + _indexer.index(tail));
            _tail.store(tail + 1, std::memory_order_release);

            return value;
        }

        void discard_next() {
            assert(not is_empty());
            auto tail = _tail.load(std::memory_order_acquire);

            alloc_traits::destroy(_alloc, _buffer + _indexer.index(tail));
            _tail.store(tail + 1, std::memory_order_release);
        }

        void discard_all() {
            auto head = _head.load(std::memory_order_acquire);
            auto tail = _tail.load(std::memory_order_acquire);

            while (tail != head) {
                alloc_traits::destroy(_alloc, _buffer + _indexer.index(tail));
                tail += 1;
            }
            _tail.store(tail, std::memory_order_release);
        }

    };

    class IndexPolicyMasking {
        size_t _index_mask;
    public:
        inline explicit IndexPolicyMasking(size_t capacity)
            : _index_mask {capacity - 1}
        {
            assert(capacity > 0);
            assert((capacity & (capacity - 1)) == 0);
        }
        [[nodiscard]] inline auto capacity() const { return _index_mask + 1; }
        [[nodiscard]] inline auto index(size_t i) const { return i & _index_mask; }
    };

    class IndexPolicyModulo {
        size_t _capacity;
    public:
        inline explicit IndexPolicyModulo(size_t capacity)
            : _capacity {capacity}
        {
            assert(capacity > 0);
        }
        [[nodiscard]] inline auto capacity() const { return _capacity; }
        [[nodiscard]] inline auto index(size_t i) const { return i % _capacity; }
    };

} // namespace cts::spsc::detail

#endif /* CTS_SPSC_DETAIL_RING_CHANNEL_HH */
