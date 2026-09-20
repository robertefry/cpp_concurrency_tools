
#ifndef CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH
#define CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH

#include "cts/spsc/channel.hh"

#include <deque>
#include <mutex>
#include <limits>

template <typename T>
class ChannelReference {

    std::deque<T> _buffer {};
    mutable std::mutex _mutex {};

public:

    [[nodiscard]] static auto make_endpoints()
    {
        auto channel = std::make_shared<ChannelReference<T>>();
        return std::tuple{
            cts::spsc::Sender<T,ChannelReference<T>>{channel},
            cts::spsc::Receiver<T,ChannelReference<T>>{channel}
        };
    }

    [[nodiscard]] auto capacity() const { return std::numeric_limits<size_t>::max(); }

    [[nodiscard]] auto size() const -> size_t {
        std::scoped_lock lock {_mutex};
        return _buffer.size();
    }

    [[nodiscard]] bool is_empty() const { return size() == 0; }
    [[nodiscard]] bool is_full() const { return size() == capacity(); }

    void send(T const& value) { send_emplace(value); }
    void send(T&& value) { send_emplace(std::move(value)); }

    template <typename... Args>
    void send_emplace(Args&&... args) {
        std::scoped_lock lock {_mutex};
        _buffer.emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] auto recv() -> T {
        assert(not is_empty());
        std::scoped_lock lock {_mutex};
        auto value = std::move(_buffer.front());
        _buffer.pop_front();
        return value;
    }

    void discard_next() {
        assert(not is_empty());
        std::scoped_lock lock {_mutex};
        _buffer.pop_front();
    }

    void discard_all() {
        std::scoped_lock lock {_mutex};
        _buffer.clear();
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH */
