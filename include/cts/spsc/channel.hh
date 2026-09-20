
#ifndef CTS_SPSC_CHANNEL_HH
#define CTS_SPSC_CHANNEL_HH

#include <memory>

namespace cts::spsc {

    template <typename T, typename Channel>
    class Sender {

        std::shared_ptr<Channel> _channel;

    public:
        using value_type = T;

        ~Sender() = default;

        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;

        explicit Sender(std::shared_ptr<Channel> channel)
            : _channel{std::move(channel)}
        {}

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

    public:
        using value_type = T;

        ~Receiver() = default;

        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;

        explicit Receiver(std::shared_ptr<Channel> channel)
            : _channel{std::move(channel)}
        {}

        [[nodiscard]] auto capacity() const { return _channel->capacity(); }
        [[nodiscard]] auto size() const { return _channel->size(); }
        [[nodiscard]] auto is_empty() const { return _channel->is_empty(); }

        [[nodiscard]] auto recv() { return _channel->recv(); }

        void discard_next() { _channel->discard_next(); }
        void discard_all() { _channel->discard_all(); }

    };

} // namespace cts::spsc

#include "array_channel.hh"
// #include "deque_channel.hh"

#endif /* CTS_SPSC_CHANNEL_HH */
