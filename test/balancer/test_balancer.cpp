#include <gtest/gtest.h>
#include <log4cplus/configurator.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_map>
#include "balancer/consul_resolver.h"

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, caseConsulResolver) {
    log4cplus::PropertyConfigurator config(LOG4CPLUS_C_STR_TO_TSTRING("log.ini"));
    config.configure();
    log4cplus::Logger logger = log4cplus::Logger::getInstance("info");

    auto resolver = std::make_shared<ConsulResolver>(
        "http://sg-consul.mobvista.com:8500", "rs",
        "rs/cpu_threshold.json", "rs/zone_cpu.json", "rs/instance_factor.json", 15, 10);
//    resolver->SetLogger(&logger);
//    int code;
//    std::string err;
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

TEST(testBalancer, caseConcurrency) {
    log4cplus::PropertyConfigurator config(LOG4CPLUS_C_STR_TO_TSTRING("log.ini"));
    config.configure();
    log4cplus::Logger logger = log4cplus::Logger::getInstance("info");
    auto resolver = std::make_shared<ConsulResolver>(
        "http://127.0.0.1:8500", "rs",
        "rs/cpu_threshold.json", "rs/zone_cpu.json", "rs/instance_factor.json", 1, 1);
    resolver->SetLogger(&logger);
    int code;
    std::string err;
    std::tie(code, err) = resolver->Start();
    if (code!=0) {
        std::cout << code << err << std::endl;
    }

    auto threadNum = 20;
    std::vector<std::thread *> vt;
    std::vector<std::unordered_map<std::string, int>> counters(threadNum);
    auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < threadNum; i++) {
        vt.emplace_back(new std::thread(
            [&](int idx) {
                while (true) {
                    if (std::chrono::steady_clock::now() - now > std::chrono::seconds(20)) {
                        break;
                    }
                    auto address = resolver->DiscoverNode()->Address();
                    if (counters[idx].count(address) <= 0) {
                        counters[idx][address] = 0;
                    }
                    counters[idx][address]++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            },
            i));
    }
    for (auto &t : vt) {
        if (t->joinable()) {
            t->join();
        }
    }
    std::unordered_map<std::string, int> counter;
    int total = 0;
    for (const auto &c : counters) {
        for (const auto &kv : c) {
            if (counter.count(kv.first) <= 0) {
                counter[kv.first] = 0;
            }
            counter[kv.first] += kv.second;
            total += kv.second;
        }
    }
    for (const auto &kv : counter) {
        std::cout << kv.first << " => " << kv.second*1.0/total << std::endl;
    }
    resolver->Stop();
}
}
