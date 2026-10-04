
#ifndef CTS_TESTS_HELPER_CHANNEL_THRASHER_HH
#define CTS_TESTS_HELPER_CHANNEL_THRASHER_HH

#include <chrono>
#include <thread>
#include <future>
#include <latch>
#include <atomic>

class ChannelThrasher {

    template <typename Channel>
    class Runner;

public:

    size_t message_count = 0;
    std::chrono::nanoseconds producer_work_time {0};
    std::chrono::nanoseconds consumer_work_time {0};

    template <typename Channel>
    [[nodiscard]] auto setup(Channel* channel) const -> Runner<Channel>;

};

template <typename Channel>
class ChannelThrasher::Runner {

    ChannelThrasher config_;
    Channel* channel_;

    std::latch sync_{3};
    std::latch done_{2};
    std::atomic_flag started_{};

    std::atomic<bool> success_ = false;

    std::jthread producer_{};
    std::jthread consumer_{};

public:

    ~Runner() {
        producer_.request_stop();
        consumer_.request_stop();
        if (not started_.test_and_set()) { sync_.count_down(); }
    }

    Runner(Runner&&) = delete;
    Runner& operator=(Runner&&) = delete;

    Runner(ChannelThrasher config, Channel* channel)
        : config_{std::move(config)}
        , channel_{channel}
    {
        assert(channel != nullptr && "cannot construct over a nullptr channel");
        started_.clear();

        producer_ = std::jthread{[this](std::stop_token token){
            sync_.arrive_and_wait();
            this->producer_task(token);
            done_.count_down();
        }};
        consumer_ = std::jthread{[this](std::stop_token token){
            sync_.arrive_and_wait();
            this->consumer_task(token);
            done_.count_down();
        }};
    }

    void run_blocking() {
        if (not started_.test_and_set()) {
            sync_.arrive_and_wait();
            done_.wait();
        }
    }

    auto success() const {
        return success_.load(std::memory_order_acquire);
    }

private:

    static void spin_for(std::chrono::nanoseconds duration) {
        auto const wake_time = std::chrono::steady_clock::now() + duration;
        while (wake_time > std::chrono::steady_clock::now());
    }

    void producer_task(std::stop_token token) {
        for (size_t counter = 0; counter < config_.message_count;) {
            if (token.stop_requested()) { return; }
            if (channel_->send_available()) { channel_->send(counter++); }
            spin_for(config_.producer_work_time);
            std::this_thread::yield();
        }
    }

    void consumer_task(std::stop_token token) {
        for (size_t counter = 0; counter < config_.message_count;) {
            if (token.stop_requested()) { return; }
            if (channel_->recv_available() && channel_->recv() != counter++) { return; }
            spin_for(config_.consumer_work_time);
            std::this_thread::yield();
        }
        success_.store(true, std::memory_order_release);
    }

};

template <typename Channel>
auto ChannelThrasher::setup(Channel* channel) const -> Runner<Channel> {
    return Runner{*this, channel};
};

#endif /* CTS_TESTS_HELPER_CHANNEL_THRASHER_HH */
