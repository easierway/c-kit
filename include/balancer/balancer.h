#pragma once

#include <log4cplus/logger.h>
#include <thread>

#include "consul_resolver.h"

namespace kit {

class Balancer {
    ConsulResolver resolver;
    int intervalS;
    bool done;
    std::thread *serviceUpdater;
    log4cplus::Logger *logger;

public:
    Balancer(const std::string &address,
             const std::string &service,
             const std::string &cpuThresholdKey = "clb/rs/cpu_threshold.json",
             const std::string &zoneCPUKey = "clb/rs/zone_cpu.json",
             const std::string &instanceFactorKey = "clb/rs/instance_factor.json",
             const std::string &onlinelabFactorKey = "clb/rs/onlinelab_factor.json",
             int timeoutS = 5,
             int intervalS = 60);

    void SetLogger(log4cplus::Logger *logger) {
        this->logger = logger;
    }
    std::tuple<int, std::string> Start();
    std::tuple<int, std::string> Stop();
    std::shared_ptr<ServiceNode> SelectedNode();
};

}
