
#ifndef CTS_TESTS_HELPER_CHANNEL_THRASHER_HH
#define CTS_TESTS_HELPER_CHANNEL_THRASHER_HH

#include "cts/spsc/channel.hh"

#include <thread>
#include <latch>
#include <atomic>

template <typename T, typename Channel>
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

    explicit ChannelThrasher(
        size_t max_count,
        cts::spsc::Sender<T,Channel> sender,
        cts::spsc::Receiver<T,Channel> receiver
    ){
        _started.clear();

        _producer = std::jthread{[this, max_count, sender = std::move(sender)](std::stop_token token) mutable {
            this->producer_task(token, max_count, std::move(sender));
        }};

        _consumer = std::jthread{[this, max_count, receiver = std::move(receiver)](std::stop_token token) mutable {
            this->consumer_task(token, max_count, std::move(receiver));
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

    void producer_task(std::stop_token token, size_t max_count, cts::spsc::Sender<T,Channel>&& sender)
    {
        _sync.arrive_and_wait();

        for (size_t counter = 0; counter < max_count;) {
            if (token.stop_requested()) { return; }
            if (not sender.is_full()) { sender.send(counter++); }
            std::this_thread::yield();
        }
        _done.count_down();
    }

    void consumer_task(std::stop_token token, size_t max_count, cts::spsc::Receiver<T,Channel>&& receiver)
    {
        _sync.arrive_and_wait();

        for (size_t counter = 0; counter < max_count;) {
            if (token.stop_requested()) { return; }
            if (not receiver.is_empty() && receiver.recv() != counter++) { return; }
            std::this_thread::yield();
        }
        _success.store(true, std::memory_order_release);
        _done.count_down();
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_THRASHER_HH */
