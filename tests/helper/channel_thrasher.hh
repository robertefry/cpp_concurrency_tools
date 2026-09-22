
#ifndef CTS_TESTS_HELPER_CHANNEL_THRASHER_HH
#define CTS_TESTS_HELPER_CHANNEL_THRASHER_HH

#include <thread>
#include <latch>
#include <atomic>

template <typename Tx, typename Rx>
class ChannelThrasher {

    std::latch _sync {3}; // producer, consumer, this
    std::latch _done {2}; // producer, consumer
    std::atomic_flag _started {};

    std::atomic_bool _success = false;

    std::jthread _producer {};
    std::jthread _consumer {};

public:
    ~ChannelThrasher() {
        _producer.request_stop();
        _consumer.request_stop();
        if (not _started.test_and_set()) { _sync.count_down(); }
    }

    ChannelThrasher(ChannelThrasher const&) = delete;
    ChannelThrasher& operator=(ChannelThrasher const&) = delete;

    explicit ChannelThrasher(size_t message_count, Tx tx, Rx rx)
    {
        _started.clear();

        _producer = std::jthread{[this, message_count, tx = std::move(tx)](std::stop_token token) mutable {
            this->producer_task(token, message_count, std::move(tx));
        }};

        _consumer = std::jthread{[this, message_count, rx = std::move(rx)](std::stop_token token) mutable {
            this->consumer_task(token, message_count, std::move(rx));
        }};
    }

    auto run() && -> bool {
        if (not _started.test_and_set()) {
            _sync.arrive_and_wait();
            _done.wait();
        }
        return _success.load(std::memory_order_acquire);
    }

private:

    void producer_task(std::stop_token token, size_t message_count, Tx&& tx)
    {
        _sync.arrive_and_wait();

        for (size_t counter = 0; counter < message_count;) {
            if (token.stop_requested()) { return; }
            if (not tx.is_full()) { tx.send(counter++); }
            std::this_thread::yield();
        }
        _done.count_down();
    }

    void consumer_task(std::stop_token token, size_t message_count, Rx&& rx)
    {
        _sync.arrive_and_wait();

        for (size_t counter = 0; counter < message_count;) {
            if (token.stop_requested()) { return; }
            if (not rx.is_empty() && rx.recv() != counter++) { return; }
            std::this_thread::yield();
        }
        _success.store(true, std::memory_order_release);
        _done.count_down();
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_THRASHER_HH */
