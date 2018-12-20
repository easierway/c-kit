#include <chrono>
#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include <unordered_map>

#include "balancer/consul_resolver.h"

int main(int argc, char *argv[]) {
    log4cplus::initialize();
    log4cplus::BasicConfigurator config;
    config.configure();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testResolver, caseUpdate) {
    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");
    auto resolver = std::make_shared<ConsulResolver>(
        "http://sg-consul.mobvista.com:8500",
        "rs",
        "clb/rs/cpu_threshold.json",
        "clb/rs/zone_cpu.json",
        "clb/rs/instance_factor.json",
        "clb/rs/onlinelab_factor.json",
        15,
        10);
    resolver->SetLogger(&logger);
    int code;
    std::string err;

    std::tie(code, err) = resolver->updateZoneCPUMap();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);

    std::tie(code, err) = resolver->updateCPUThreshold();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);

    std::tie(code, err) = resolver->updateOnlinelabFactor();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);

    std::tie(code, err) = resolver->updateInstanceFactorMap();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);

    resolver->SetZone("ap-southeast-1a");
    std::tie(code, err) = resolver->updateServiceZone();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);

    std::tie(code, err) = resolver->updateCandidatePool();
    GTEST_ASSERT_EQ(0, code);
    GTEST_ASSERT_EQ("", err);
    LOG4CPLUS_DEBUG(logger, "resolver: [" << resolver->to_json().dump() << "]");

    auto snode1 = resolver->SelectedNode();
    LOG4CPLUS_DEBUG(logger, "resolver, select node [" << snode1->to_json().dump() << "]");

//    std::tie(code, err) = resolver->Start();
//    if (code!=0) {
//        std::cout << code << err << std::endl;
//    }
//    int N = 1000;
//    std::unordered_map<std::string, int> counter;
//    for (int i = 0; i < N; i++) {
//        auto address = resolver->DiscoverNode()->Address();
//        if (counter.count(address) <= 0) {
//            counter[address] = 0;
//        }
//        counter[address]++;
//    }
//    for (const auto &kv : counter) {
//        std::cout << kv.first << " => " << kv.second*1.0/N << std::endl;
//    }
//    resolver->Stop();
}

TEST(testResolver, caseConcurrency) {
//    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");
//    auto resolver = std::make_shared<ConsulResolver>(
//        "http://sg-consul.mobvista.com:8500", "rs",
//        "clb/rs/cpu_threshold.json", "clb/rs/zone_cpu.json", "clb/rs/instance_factor.json", 15, 10);
//    resolver->SetLogger(&logger);
//
//
//
//    int code;
//    std::string err;


//    std::tie(code, err) = resolver->Start();
//    if (code!=0) {
//        std::cout << code << err << std::endl;
//    }
//
//    auto threadNum = 20;
//    std::vector<std::thread *> vt;
//    std::vector<std::unordered_map<std::string, int>> counters(threadNum);
//    auto now = std::chrono::steady_clock::now();
//    for (int i = 0; i < threadNum; i++) {
//        vt.emplace_back(new std::thread(
//            [&](int idx) {
//                while (true) {
//                    if (std::chrono::steady_clock::now() - now > std::chrono::seconds(20)) {
//                        break;
//                    }
//                    auto address = resolver->DiscoverNode()->Address();
//                    if (counters[idx].count(address) <= 0) {
//                        counters[idx][address] = 0;
//                    }
//                    counters[idx][address]++;
//                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
//                }
//            },
//            i));
//    }
//    for (auto &t : vt) {
//        if (t->joinable()) {
//            t->join();
//        }
//    }
//    std::unordered_map<std::string, int> counter;
//    int total = 0;
//    for (const auto &c : counters) {
//        for (const auto &kv : c) {
//            if (counter.count(kv.first) <= 0) {
//                counter[kv.first] = 0;
//            }
//            counter[kv.first] += kv.second;
//            total += kv.second;
//        }
//    }
//    for (const auto &kv : counter) {
//        std::cout << kv.first << " => " << kv.second*1.0/total << std::endl;
//    }
//    resolver->Stop();
}
}
