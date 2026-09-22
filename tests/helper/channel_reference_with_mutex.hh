
#ifndef CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH
#define CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH

#include "cts/channel.hh"

#include <deque>
#include <mutex>
#include <limits>

template <typename T> struct ReferenceTx;
template <typename T> struct ReferenceRx;

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

    [[nodiscard]] auto into_endpoints() && {
        using Channel = std::remove_cvref_t<decltype(*this)>;
        auto channel = std::make_shared<Channel>(std::move(*this));
        return std::tuple{
            ReferenceTx{ .channel_ = channel }, ReferenceRx{ .channel_ = channel }
        };
    }

    [[nodiscard]] auto size() const {
        auto const lock = std::scoped_lock{mutex_};
        return buffer_.size();
    }

    [[nodiscard]] auto is_full() const {
        auto const lock = std::scoped_lock{mutex_};
        return false;
    }

    [[nodiscard]] auto is_empty() const {
        auto const lock = std::scoped_lock{mutex_};
        return buffer_.empty();
    }

};

template <typename T>
struct ReferenceTx {

    std::shared_ptr<ReferenceChannel<T>> channel_;

    [[nodiscard]] auto size() const noexcept { return channel_->size(); }
    [[nodiscard]] auto is_empty() const noexcept { return channel_->is_empty(); }
    [[nodiscard]] auto is_full() const noexcept { return channel_->is_full(); }

    void send(T const& value) { send_emplace(value); }
    void send(T&& value) { send_emplace(std::move(value)); }

    template <typename... Args>
    void send_emplace(Args&&... args) {
        auto const lock = std::scoped_lock{channel_->mutex_};
        channel_->buffer_.emplace_back(std::forward<Args>(args)...);
    }

};

template <typename T>
struct ReferenceRx {

    std::shared_ptr<ReferenceChannel<T>> channel_;

    [[nodiscard]] auto size() const noexcept { return channel_->size(); }
    [[nodiscard]] auto is_empty() const noexcept { return channel_->is_empty(); }
    [[nodiscard]] auto is_full() const noexcept { return channel_->is_full(); }

    [[nodiscard]] auto recv() {
        auto const lock = std::scoped_lock{channel_->mutex_};
        auto const value = channel_->buffer_.front();
        channel_->buffer_.pop_front();
        return value;
    }

    void discard_next() {
        auto const lock = std::scoped_lock{channel_->mutex_};
        channel_->buffer_.pop_front();
    }

    void discard_all() {
        auto const lock = std::scoped_lock{channel_->mutex_};
        channel_->buffer_.clear();
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_REFERENCE_HH */
