
#ifndef CTS_TESTS_HELPER_CHANNEL_THRASHER_HH
#define CTS_TESTS_HELPER_CHANNEL_THRASHER_HH

#include <chrono>
#include <thread>
#include <future>
#include <latch>
#include <atomic>

class ChannelThrasher {

    template <typename Tx, typename Rx>
    class Runner;

public:

    size_t message_count = 0;
    std::chrono::nanoseconds producer_work_time {0};
    std::chrono::nanoseconds consumer_work_time {0};

    template <typename Tx, typename Rx>
    [[nodiscard]] auto setup(Tx&& tx, Rx&& rx) const -> Runner<Tx,Rx>;

};

template <typename Tx, typename Rx>
class ChannelThrasher::Runner {

    ChannelThrasher config_;
    Tx tx_;
    Rx rx_;

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

    Runner(ChannelThrasher config, Tx tx, Rx rx)
        : config_{std::move(config)}
        , tx_{std::move(tx)}
        , rx_{std::move(rx)}
    {
        started_.clear();

        producer_ = std::jthread{[this](std::stop_token token){ this->producer_task(token); }};
        consumer_ = std::jthread{[this](std::stop_token token){ this->consumer_task(token); }};
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

    void producer_task(std::stop_token token) {
        sync_.arrive_and_wait();

        for (size_t counter = 0; counter < config_.message_count;) {
            if (token.stop_requested()) { return; }
            if (not tx_.is_full()) { tx_.send(counter++); }
            std::this_thread::sleep_for(config_.producer_work_time);
            std::this_thread::yield();
        }
        done_.count_down();
    }

    void consumer_task(std::stop_token token) {
        sync_.arrive_and_wait();

        for (size_t counter = 0; counter < config_.message_count;) {
            if (token.stop_requested()) { return; }
            if (not rx_.is_empty() && rx_.recv() != counter++) { return; }
            std::this_thread::sleep_for(config_.consumer_work_time);
            std::this_thread::yield();
        }
        success_.store(true, std::memory_order_release);
        done_.count_down();
    }

};

template <typename Tx, typename Rx>
auto ChannelThrasher::setup(Tx&& tx, Rx&& rx) const -> Runner<Tx,Rx> {
    return Runner{*this, std::forward<Tx>(tx), std::forward<Rx>(rx)};
};

#endif /* CTS_TESTS_HELPER_CHANNEL_THRASHER_HH */
