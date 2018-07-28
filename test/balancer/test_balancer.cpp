#include <gtest/gtest.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_map>
#include "balancer/consul_resolver.h"

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, caseConsulResolver) {
    auto              resolver = std::make_shared<ConsulResolver>("http://127.0.0.1:8500", "hatlonly-test-service", "my-service", 1, 0);
    log4cplus::Logger logger;
    resolver->SetLogger(&logger);
    int         code;
    std::string err;
    std::tie(code, err) = resolver->Start();
    if (code != 0) {
        std::cout << code << err << std::endl;
    }
    int                                  N = 1000;
    std::unordered_map<std::string, int> counter;
    for (int i = 0; i < N; i++) {
        auto address = resolver->DiscoverNode()->Address();
        if (counter.count(address) <= 0) {
            counter[address] = 0;
        }
        counter[address]++;
    }
    for (const auto& kv : counter) {
        std::cout << kv.first << " => " << kv.second * 1.0 / N << std::endl;
    }
    resolver->Stop();
}

TEST(testBalancer, caseConcurrency) {
    auto              resolver = std::make_shared<ConsulResolver>("http://127.0.0.1:8500", "hatlonly-test-service", "my-service", 1, 0);
    log4cplus::Logger logger;
    resolver->SetLogger(&logger);
    int         code;
    std::string err;
    std::tie(code, err) = resolver->Start();
    if (code != 0) {
        std::cout << code << err << std::endl;
    }

    auto                                              threadNum = 100;
    std::vector<std::thread*>                         vt;
    std::vector<std::unordered_map<std::string, int>> counters(threadNum);
    auto                                              now = std::chrono::steady_clock::now();
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
                }
            },
            i));
    }
    for (auto& t : vt) {
        if (t->joinable()) {
            t->join();
        }
    }
    std::unordered_map<std::string, int> counter;
    int                                  total = 0;
    for (const auto& c : counters) {
        for (const auto& kv : c) {
            if (counter.count(kv.first) <= 0) {
                counter[kv.first] = 0;
            }
            counter[kv.first] += kv.second;
            total += kv.second;
        }
    }
    for (const auto& kv : counter) {
        std::cout << kv.first << " => " << kv.second * 1.0 / total << std::endl;
    }
    resolver->Stop();
}
}
