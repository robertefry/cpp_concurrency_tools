
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

        friend auto channel_bounded<T,typename Channel::allocator_type>(
            size_t capacity,
            Channel::allocator_type const& allocator
        ) -> std::tuple<
            Sender<T,Channel>,
            Receiver<T,Channel>
        >;

    public:
        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;

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

        friend auto channel_bounded<T,typename Channel::allocator_type>(
            size_t capacity,
            Channel::allocator_type const& allocator
        ) -> std::tuple<
            Sender<T,Channel>,
            Receiver<T,Channel>
        >;

    public:
        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;

        [[nodiscard]] auto size() const { return _channel->size(); }
        [[nodiscard]] auto is_empty() const { return _channel->is_empty(); }

        [[nodiscard]] auto recv() { return _channel->recv(); }

        void discard_next() { _channel->discard_next(); }
        void discard_all() { _channel->discard_all(); }

    };

    namespace impl {

        template <typename T, typename Allocator>
        class ArrayChannel {

            Allocator _alloc;

            T*     _buffer;
            size_t _buffer_mask;

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
                , _buffer {std::exchange(other._buffer, nullptr)}
                , _buffer_mask {std::exchange(other._buffer_mask, 0)}
                , _head {std::atomic_exchange_explicit(&other._head, 0, std::memory_order_relaxed)}
                , _tail {std::atomic_exchange_explicit(&other._tail, 0, std::memory_order_relaxed)}
            {}

            ArrayChannel& operator=(ArrayChannel&& other) noexcept
            {
                if (this == &other) { return *this; }

                discard_all();
                alloc_traits::deallocate(_alloc, _buffer, capacity());

                _alloc = std::move(other._alloc); // TODO: proper threading of this allocator
                _buffer = std::exchange(other._buffer, nullptr);
                _buffer_mask = std::exchange(other._buffer_mask, 0);
                _head = std::atomic_exchange_explicit(&other._head, 0, std::memory_order_relaxed);
                _tail = std::atomic_exchange_explicit(&other._tail, 0, std::memory_order_relaxed);

                return *this;
            }

            ArrayChannel(
                size_t capacity,
                allocator_type const& allocator = allocator_type{}
            )
                : _alloc {allocator}
                , _buffer {alloc_traits::allocate(_alloc, capacity)}
                , _buffer_mask {capacity-1}
                , _head {0}
                , _tail {0}
            {
                assert(capacity > 0);
                assert((capacity & (capacity - 1)) == 0);
            }

            [[nodiscard]] auto capacity() const { return _buffer_mask + 1; }

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

                alloc_traits::construct(_alloc, _buffer + (head & _buffer_mask), std::forward<Args>(args)...);
                _head.store(head + 1, std::memory_order_release);
            }

            [[nodiscard]] auto recv() -> T {
                assert(not is_empty());
                auto tail = _tail.load(std::memory_order_acquire);

                auto value = std::move(_buffer[tail & _buffer_mask]);

                alloc_traits::destroy(_alloc, _buffer + (tail & _buffer_mask));
                _tail.store(tail + 1, std::memory_order_release);

                return value;
            }

            void discard_next() {
                assert(not is_empty());
                auto tail = _tail.load(std::memory_order_acquire);

                alloc_traits::destroy(_alloc, _buffer + (tail & _buffer_mask));
                _tail.store(tail + 1, std::memory_order_release);
            }

            void discard_all() {
                auto head = _head.load(std::memory_order_acquire);
                auto tail = _tail.load(std::memory_order_acquire);

                while (tail != head) {
                    alloc_traits::destroy(_alloc, _buffer + (tail & _buffer_mask));
                    tail += 1;
                }
                _tail.store(tail, std::memory_order_release);
            }

        };

    } // namespace impl

    template <typename T, typename Allocator>
    inline auto channel_bounded(
        size_t capacity,
        Allocator const& allocator
    ) -> std::tuple<
        Sender<T, impl::ArrayChannel<T,Allocator>>,
        Receiver<T, impl::ArrayChannel<T,Allocator>>
    > {
        auto channel = std::make_shared<impl::ArrayChannel<T,Allocator>>(
            capacity, allocator
        );
        return std::tuple{
            Sender<T,impl::ArrayChannel<T,Allocator>>{channel},
            Receiver<T,impl::ArrayChannel<T,Allocator>>{channel},
        };
    }

} // namespace cts::spsc

#endif /* CTS_SPSC_CHANNEL_HH */
