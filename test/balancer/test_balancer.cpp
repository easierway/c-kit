#include <chrono>
#include <exception>
#include <gtest/gtest.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>

#include "balancer/balancer.h"
#include "util/constant.h"

int main(int argc, char *argv[]) {
    log4cplus::initialize();
    log4cplus::BasicConfigurator config;
    config.configure();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, caseService) {
    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");

    auto balancer = std::make_shared<Balancer>(
        "http://sg-consul.mobvista.com:8500",
        "rs",
        "clb/rs/cpu_threshold.json",
        "clb/rs/zone_cpu.json",
        "clb/rs/instance_factor.json",
        "clb/rs/onlinelab_factor.json",
        10,
        10);
    balancer->SetLogger(&logger);
    balancer->SetZone("ap-southeast-1a");

    int code;
    std::string err;
    std::tie(code, err) = balancer->Start();
    GTEST_ASSERT_EQ(STATUSCODE::SUCCESS, code);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    for (auto i = 0; i < 100; i++) {
        LOG4CPLUS_DEBUG(logger,
                        "balancer, select node [" << balancer->SelectedNode()->to_jsonBalanceFactor().dump() << "]");
    }
    balancer->Stop();
}

}
