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
        this->resolver.SetLogger(logger);
        this->logger = logger;
    }
    // TODO: this method should not be public, but test needed now
    void SetZone(const std::string& zone) {
        this->resolver.SetZone(zone);
    }

    std::tuple<int, std::string> Start();
    std::tuple<int, std::string> Stop();
    std::shared_ptr<ServiceNode> SelectedNode();
    std::string getLocalZone();
};

}
