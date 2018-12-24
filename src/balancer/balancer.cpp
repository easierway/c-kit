#include <log4cplus/loggingmacros.h>

#include "balancer/balancer.h"
#include "util/constant.h"

namespace kit {

Balancer::Balancer(
    const std::string &address,
    const std::string &service,
    const std::string &cpuThresholdKey,
    const std::string &zoneCPUKey,
    const std::string &instanceFactorKey,
    const std::string &onlinelabFactorKey,
    int timeoutS,
    int intervalS
) : resolver(address, service, cpuThresholdKey, zoneCPUKey, instanceFactorKey, onlinelabFactorKey, timeoutS),
    intervalS(intervalS) {
    this->done = false;
    this->serviceUpdater = nullptr;
    this->logger = nullptr;
}

std::tuple<int, std::string> Balancer::Start() {
    std::string err;
    int code;
    std::tie(code, err) = this->resolver.updateAll();
    if (code!=STATUSCODE::SUCCESS) {
        return std::make_tuple(code, err);
    }

    this->serviceUpdater = new std::thread([&]() {
        while (!this->done) {
            LOG4CPLUS_DEBUG(*(this->logger), "update consul metrics start");
            this->resolver.updateAll();
            LOG4CPLUS_INFO(*(this->logger),
                           "update consul metrics finish, resolver" << this->resolver.to_json().dump());
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    if (logger!=nullptr) {
        LOG4CPLUS_INFO(*(this->logger), "consul resolver start ");
    }

    return std::make_tuple(STATUSCODE::SUCCESS, "");
}

std::tuple<int, std::string> Balancer::Stop() {
    this->done = true;

    for (const auto &t : {this->serviceUpdater}) {
        if (t!=nullptr) {
            if (t->joinable()) {
                t->join();
            }
            delete t;
        }
    }
    this->serviceUpdater = nullptr;

    return std::make_tuple(STATUSCODE::SUCCESS, "");
}

std::shared_ptr<ServiceNode> Balancer::SelectedNode() {
    return this->resolver.SelectedNode();
}

}