
#ifndef CTS_TESTS_HELPER_STAT_LOGS_HH
#define CTS_TESTS_HELPER_STAT_LOGS_HH

#include <unordered_map>
#include <string>

#include <catch2/catch_all.hpp>

struct StatLogs : Catch::EventListenerBase
{
    using EventListenerBase::EventListenerBase;

    using StatsLog = std::unordered_map<std::string,Catch::BenchmarkStats<>>;
    static inline StatsLog _stats_log {};

    void benchmarkEnded(Catch::BenchmarkStats<> const& s) override {
        _stats_log[s.info.name] = s;
    }

public:

    static auto get(std::string benchmark) {
        return _stats_log.at(benchmark);
    }

};

CATCH_REGISTER_LISTENER(StatLogs);

#endif /* CTS_TESTS_HELPER_STAT_LOGS_HH */
