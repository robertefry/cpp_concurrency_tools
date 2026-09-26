
#ifndef CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH
#define CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH

#include "cts/channel.hh"

#include <deque>
#include <mutex>
#include <limits>

template <typename T>
struct ReferenceChannel {

    std::deque<T> buffer_ {};
    mutable std::mutex mutex_ {};

    explicit ReferenceChannel() = default;

    ReferenceChannel(ReferenceChannel&& other) noexcept
        : buffer_{other.buffer_}
    {}

    ReferenceChannel& operator=(ReferenceChannel&& other) noexcept {
        std::swap(buffer_, other.buffer_);
    }

    [[nodiscard]] auto send_available() const -> size_t { return SIZE_MAX; }

    void send(T const& value) { send_emplace(value); }
    void send(T&& value) { send_emplace(std::move(value)); }

    template <typename... Args>
    void send_emplace(Args&&... args) {
        auto const lock = std::scoped_lock{mutex_};
        buffer_.emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] auto recv_available() const -> size_t {
        auto const lock = std::scoped_lock{mutex_};
        return buffer_.size();
    }

    [[nodiscard]] auto recv() {
        auto const lock = std::scoped_lock{mutex_};
        auto const value = buffer_.front();
        buffer_.pop_front();
        return value;
    }

    void discard_next() {
        auto const lock = std::scoped_lock{mutex_};
        buffer_.pop_front();
    }

    void discard_all() {
        auto const lock = std::scoped_lock{mutex_};
        buffer_.clear();
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH */
