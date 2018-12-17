#include <gtest/gtest.h>
#include <log4cplus/configurator.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_map>
#include "balancer/consul_resolver.h"
#include <log4cplus/loggingmacros.h>

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testBalancer, caseConsulResolver) {
    log4cplus::PropertyConfigurator config(LOG4CPLUS_C_STR_TO_TSTRING("log.ini"));
    config.configure();
    log4cplus::Logger logger = log4cplus::Logger::getInstance("info");

    auto client = std::make_shared<ConsulClient>("http://sg-consul.mobvista.com:8500");
    int status = -1;
    json11::Json kv;
    std::string err;
    std::string lastIndex;
    std::tie(status, kv, err) = client->GetKV("rs/zone_cpu.json", 10, lastIndex);
    LOG4CPLUS_INFO(logger, "rs/zone_cpu.json" << kv.dump());
}

}
