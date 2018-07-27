#include <gtest/gtest.h>
#include <exception>
#include <iostream>
#include "balancer/consul_balancer.h"

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, case1) {
    try {
        auto r = std::make_shared<ConsulResolver>("http://127.0.0.1:8500", "hatlonly-test-service", "my-service", 1, 0);
        if (!r) {
            std::cout << "construct r falied." << std::endl;
        }
        r->DiscoverNode();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
}
