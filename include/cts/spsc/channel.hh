
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include "channel.fwd"

#include <cassert>
#include <atomic>

namespace cts::spsc {

    template <typename T, typename Channel>
    class Sender {

        std::shared_ptr<Channel> _channel;

        Sender(std::shared_ptr<Channel> channel)
            : _channel{std::move(channel)}
        {}

        friend Channel;

    public:
        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;

        [[nodiscard]] auto capacity() const { return _channel->capacity(); }
        [[nodiscard]] auto size() const { return _channel->size(); }
        [[nodiscard]] auto is_full() const { return _channel->is_full(); }

        void send(T const& value) { _channel->send(value); }
        void send(T&& value) { _channel->send(std::move(value)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            _channel->send_emplace(std::forward<Args>(args)...);
        }

    };

    template <typename T, typename Channel>
    class Receiver {

        std::shared_ptr<Channel> _channel;

        Receiver(std::shared_ptr<Channel> channel)
            : _channel{std::move(channel)}
        {}

        friend Channel;

    public:
        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;

        [[nodiscard]] auto capacity() const { return _channel->capacity(); }
        [[nodiscard]] auto size() const { return _channel->size(); }
        [[nodiscard]] auto is_empty() const { return _channel->is_empty(); }

        [[nodiscard]] auto recv() { return _channel->recv(); }

        void discard_next() { _channel->discard_next(); }
        void discard_all() { _channel->discard_all(); }

    };

    namespace impl {

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

        template <typename T, typename IndexPolicy, typename Allocator>
        class ArrayChannel {

            Allocator _alloc;
            IndexPolicy _indexer;

            T*     _buffer;

            std::atomic_size_t _head;
            std::atomic_size_t _tail;

        public:
            using allocator_type = Allocator;
            using alloc_traits = std::allocator_traits<allocator_type>;

            ~ArrayChannel() {
                discard_all();
                alloc_traits::deallocate(_alloc, _buffer, capacity());
            }

            ArrayChannel(ArrayChannel&& other) noexcept
                : _alloc {std::move(other._alloc)}
                , _indexer {std::move(other._indexer)}
                , _buffer {std::exchange(other._buffer, nullptr)}
                , _head {std::atomic_exchange_explicit(&other._head, 0, std::memory_order_relaxed)}
                , _tail {std::atomic_exchange_explicit(&other._tail, 0, std::memory_order_relaxed)}
            {}

            ArrayChannel& operator=(ArrayChannel&& other) noexcept
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

            ArrayChannel(
                size_t capacity,
                allocator_type const& allocator = allocator_type{}
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
                using Channel = ArrayChannel<T,IndexPolicy,Allocator>;

                auto channel = std::make_shared<Channel>(capacity, allocator);
                return std::tuple{ Sender<T,Channel>{channel}, Receiver<T,Channel>{channel} };
            }

            [[nodiscard]] auto capacity() const { return _indexer.capacity(); }

            [[nodiscard]] auto size() const -> size_t {
                auto head = _head.load(std::memory_order_acquire);
                auto tail = _tail.load(std::memory_order_acquire);
                return head - tail;
            }

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

    } // namespace impl

    template <typename T, typename Allocator = std::allocator<T>>
    inline auto channel_bounded_fast(
        size_t capacity,
        Allocator const& allocator = Allocator{}
    ) {
        using Channel = impl::ArrayChannel<T,impl::IndexPolicyMasking,Allocator>;
        return Channel::make_endpoints(capacity, allocator);
    }

    template <typename T, typename Allocator = std::allocator<T>>
    inline auto channel_bounded(
        size_t capacity,
        Allocator const& allocator = Allocator{}
    ) {
        using Channel = impl::ArrayChannel<T,impl::IndexPolicyModulo,Allocator>;
        return Channel::make_endpoints(capacity, allocator);
    }

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
