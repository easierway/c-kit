#include <gtest/gtest.h>
#include <exception>
#include <iostream>
#include <unordered_map>
#include "balancer/consul_resolver.h"

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, case1) {
    try {
        auto balancer = std::make_shared<ConsulResolver>("http://127.0.0.1:8500", "hatlonly-test-service", "my-service", 1, 0);
        if (!balancer) {
            std::cout << "construct r falied." << std::endl;
        }
        int         code;
        std::string err;
        std::tie(code, err) = balancer->Start();
        if (code != 0) {
            std::cout << code << err << std::endl;
        }
        int                                  N = 1000;
        std::unordered_map<std::string, int> counter;
        for (int i = 0; i < N; i++) {
            auto address = balancer->DiscoverNode()->Address();
            if (counter.count(address) <= 0) {
                counter[address] = 0;
            }
            counter[address]++;
        }
        for (const auto& kv : counter) {
            std::cout << kv.first << " => " << kv.second * 1.0 / N << std::endl;
        }
        balancer->Stop();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
}
