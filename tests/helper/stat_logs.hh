
#ifndef CTS_TESTS_HELPER_STAT_LOGS_HH
#define CTS_TESTS_HELPER_STAT_LOGS_HH

#include <unordered_map>
#include <string>

#include <catch2/catch_all.hpp>

class BenchmarkStats : public Catch::EventListenerBase {

    using Logs = std::unordered_map<std::string,Catch::BenchmarkStats<>>;
    static inline Logs _logs {};

public:
    using EventListenerBase::EventListenerBase;

    void benchmarkEnded(Catch::BenchmarkStats<> const& s) override {
        _logs[s.info.name] = s;
    }

    static auto has(std::string benchmark) {
        return _logs.contains(benchmark);
    }

    static auto get(std::string benchmark) {
        return _logs.at(benchmark);
    }

};

CATCH_REGISTER_LISTENER(BenchmarkStats);

#endif /* CTS_TESTS_HELPER_STAT_LOGS_HH */
