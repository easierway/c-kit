#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include "balancer/balancer.h"

int main(int argc, char** argv) {
    log4cplus::initialize();
    log4cplus::BasicConfigurator config;
    config.configure();
    log4cplus::Logger logger = log4cplus::Logger::getInstance("test");


    auto balancer = std::make_shared<kit::Balancer>(
        "http://fk-consul.mobvista.com:8500",
        "unknown",
        "rs",
        "clb/rs/cpu_threshold.json",
        "clb/rs/zone_cpu.json",
        "clb/rs/instance_factor.json",
        "clb/rs/onlinelab_factor.json",
        10,
        15);
    balancer->SetLogger(&logger);
//    balancer->SetZone("ap-southeast-1a");
    balancer->SetZone("eu-central-1b");

    int code;
    std::string err;
    std::tie(code, err) = balancer->Start();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    for (auto i = 0; i < 100000; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        balancer->SelectedNode()->to_jsonBalanceFactor().dump();
    }


    balancer->Stop();
    return EXIT_SUCCESS;
}
