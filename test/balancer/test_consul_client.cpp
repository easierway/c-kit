#include <gtest/gtest.h>
#include <log4cplus/configurator.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_map>
#include "balancer/consul_resolver.h"
#include <log4cplus/loggingmacros.h>

int main(int argc, char *argv[]) {
    log4cplus::initialize();
    log4cplus::BasicConfigurator config;
    config.configure();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testConsulClient, caseGetKV) {
    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");

    auto client = std::make_shared<ConsulClient>("http://sg-consul.mobvista.com:8500");
    int status = -1;
    json11::Json kv;
    std::string err;
    std::string lastIndex;

    // zone cpu
    std::tie(status, kv, err) = client->GetKV("clb/rs/zone_cpu.json", 10, lastIndex);
    GTEST_ASSERT_EQ(0, status);
    GTEST_ASSERT_EQ("", err);
    LOG4CPLUS_DEBUG(logger, "kv/clb/rs/zone_cpu.json: [" << kv.dump() << "]");

    // instance factor
    std::tie(status, kv, err) = client->GetKV("clb/rs/instance_factor.json", 10, lastIndex);
    GTEST_ASSERT_EQ(0, status);
    GTEST_ASSERT_EQ("", err);
    LOG4CPLUS_DEBUG(logger, "kv/clb/rs/instance_factor.json: [" << kv.dump() << "]");

}

TEST(testConsulClient, caseGetService) {
    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");

    auto client = std::make_shared<ConsulClient>("http://sg-consul.mobvista.com:8500");
    int status = -1;
    json11::Json kv;
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string err;
    std::string lastIndex;

    // rs
    std::tie(status, nodes, err) = client->GetService("rs", 10, lastIndex);
    GTEST_ASSERT_EQ(0, status);
    GTEST_ASSERT_EQ("", err);
    LOG4CPLUS_DEBUG(logger, "health/service/rs: [" << nodes.size() << "]");
    for (const auto& node: nodes) {
//        LOG4CPLUS_DEBUG(logger, "health/service/rs, node: [" << node->to_json().dump() << "]");
    }
}

}
