
#ifndef CTS_TESTS_HELPER_INSTANCE_COUNTER_HH
#define CTS_TESTS_HELPER_INSTANCE_COUNTER_HH

#include <memory>

class InstanceCounter {

    std::shared_ptr<char> use_counter_ = std::make_shared<char>();

public:

    class Copyable {
        friend InstanceCounter;
        std::shared_ptr<char> use_counter_;
        explicit Copyable(std::shared_ptr<char> use_counter) : use_counter_{use_counter} {}
    public:
        Copyable(Copyable const&) = default;
        Copyable& operator=(Copyable const&) = default;
    };

    class Moveable {
        friend InstanceCounter;
        std::shared_ptr<char> use_counter_;
        explicit Moveable(std::shared_ptr<char> use_counter) : use_counter_{use_counter} {}
    public:
        Moveable(Moveable&&) = default;
        Moveable& operator=(Moveable&&) = default;
    };

    [[nodiscard]] auto use_count() const { return use_counter_.use_count() - 1; }

    [[nodiscard]] auto make_copyable() { return Copyable{use_counter_}; }
    [[nodiscard]] auto make_moveable() { return Moveable{use_counter_}; }

};

#endif /* CTS_TESTS_HELPER_INSTANCE_COUNTER_HH */
