#include "balancer/consul_resolver.h"
#include <log4cplus/loggingmacros.h>
#include <algorithm>
#include <chrono>
#include <json11.hpp>
#include <random>
#include "util/util.h"

namespace kit {

ConsulResolver::ConsulResolver(
    const std::string& address,
    const std::string& service,
    const std::string& cpuThresholdKey,
    const std::string& zoneCPUKey,
    const std::string& machineFactorKey,
    int                intervalS,
    int                timeoutS) : client(address) {
    this->address          = address;
    this->service          = service;
    this->cpuThresholdKey  = cpuThresholdKey,
    this->zoneCPUKey       = zoneCPUKey,
    this->machineFactorKey = machineFactorKey,
    this->intervalS        = intervalS;
    this->timeoutS         = timeoutS;
    this->cpuThreshold     = 0;
    this->done             = false;
    this->lastIndex        = "0";
    this->zone             = Zone();
    this->serviceUpdater   = nullptr;
    this->logger           = nullptr;
}

std::tuple<int, std::string> ConsulResolver::Start() {
    std::string err;
    int         code;
    std::tie(code, err) = this->_updateAll();
    if (code != 0) {
        return std::make_tuple(code, err);
    }

    this->serviceUpdater = new std::thread([&]() {
        while (!this->done) {
            this->_updateAll();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    if (logger != nullptr) {
        LOG4CPLUS_INFO(*(this->logger), "consul resolver start " << this->to_json().dump());
    }

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::Stop() {
    this->done = true;

    for (const auto& t : {this->serviceUpdater}) {
        if (t != nullptr) {
            if (t->joinable()) {
                t->join();
            }
            delete t;
        }
    }
    this->serviceUpdater = nullptr;

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateAll() {
    int         code;
    std::string err;
    std::tie(code, err) = this->_updateCPUThreshold();
    if (code != 0 && this->logger != nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update CPU threshold failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->_updateZoneCPUMap();
    if (code != 0 && this->logger != nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update zoneCPUMap failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->_updateMachineFactorMap();
    if (code != 0 && this->logger != nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update machineFactorMap failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->_updateServiceZone();
    if (code != 0 && this->logger != nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update serviceZone failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateZoneCPUMap() {
    int          status = -1;
    json11::Json kv;
    std::string  err;
    std::tie(status, kv, err) = this->client.GetKV(this->zoneCPUKey, this->timeoutS, this->lastIndex);
    if (status != 0) {
        return std::make_tuple(status, err);
    }

    auto zoneCPUMap = std::unordered_map<std::string, int>();
    for (const auto& item : kv.object_items()) {
        zoneCPUMap[item.first] = item.second.int_value();
    }

    this->zoneCPUMap = zoneCPUMap;

    LOG4CPLUS_INFO(*(this->logger), "update zoneCPUMap: [" << json11::Json(this->zoneCPUMap).dump() << "]");
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateMachineFactorMap() {
    int          status = -1;
    json11::Json kv;
    std::string  err;
    std::tie(status, kv, err) = this->client.GetKV(this->machineFactorKey, this->timeoutS, this->lastIndex);
    if (status != 0) {
        return std::make_tuple(status, err);
    }

    auto machineFactorMap = std::unordered_map<std::string, int>();
    for (const auto& item : kv.object_items()) {
        machineFactorMap[item.first] = item.second.int_value();
    }

    this->machineFactorMap = machineFactorMap;

    LOG4CPLUS_INFO(*(this->logger), "update machineFactorMap: [" << json11::Json(this->machineFactorMap).dump() << "]");
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateCPUThreshold() {
    int          status = -1;
    json11::Json kv;
    std::string  err;
    std::tie(status, kv, err) = this->client.GetKV(this->cpuThresholdKey, this->timeoutS, this->lastIndex);
    if (status != 0) {
        return std::make_tuple(status, err);
    }
    if (!kv["cpuThreshold"].is_null()) {
        this->cpuThreshold = kv["cpuThreshold"].int_value();
    }

    LOG4CPLUS_INFO(*(this->logger), "update cpuThreshold: [" << this->cpuThreshold << "]");
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateServiceZone() {
    int                                       status = -1;
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string                               err;
    std::tie(status, nodes, err) = this->client.GetService(this->service, this->intervalS, this->lastIndex);
    if (status != 0) {
        return std::make_tuple(status, err);
    }

    std::unordered_map<std::string, std::shared_ptr<ServiceZone>> serviceZoneMap;
    for (auto& node : nodes) {
        if (this->machineFactorMap.count(node->machine) == 0) {
            node->balanceFactor = 100;
        } else {
            node->balanceFactor = this->machineFactorMap[node->machine];
        }
        if (serviceZoneMap.count(node->zone) == 0) {
            serviceZoneMap[node->zone]       = std::make_shared<ServiceZone>();
            serviceZoneMap[node->zone]->zone = node->zone;
            if (this->zoneCPUMap.count(node->zone) == 0) {
                serviceZoneMap[node->zone]->cpu = 100;
            } else {
                serviceZoneMap[node->zone]->cpu = this->zoneCPUMap[node->zone];
            }
            serviceZoneMap[node->zone]->zoneFactor     = 0;
            serviceZoneMap[node->zone]->zoneWeight     = 0;
            serviceZoneMap[node->zone]->idleZoneFactor = 0;
            serviceZoneMap[node->zone]->factorSum      = 0;
        }
        serviceZoneMap[node->zone]->nodes.emplace_back(node);
        serviceZoneMap[node->zone]->factors.emplace_back(node->balanceFactor);
        serviceZoneMap[node->zone]->weights.emplace_back(0);
        serviceZoneMap[node->zone]->factorSum += node->balanceFactor;
    }

    auto serviceZones = std::make_shared<std::vector<std::shared_ptr<ServiceZone>>>();
    auto localZone    = std::make_shared<ServiceZone>();
    for (auto& item : serviceZoneMap) {
        serviceZones->emplace_back(item.second);
        if (item.second->zone == this->zone) {
            localZone = item.second;
        }
    }

    int allFactorSum  = 0;
    int currFactorSum = 0;
    int zoneFactorSum = 0;
    for (auto& sz : *serviceZones) {
        allFactorSum += sz->factorSum;
        currFactorSum += sz->factorSum * sz->cpu;
    }
    int avgCPU = currFactorSum / allFactorSum;
    if (localZone->nodes.empty() || (avgCPU < localZone->cpu && localZone->cpu > this->cpuThreshold)) {
        int needCrossAZFactor = 0;
        if (localZone->nodes.empty()) {
            needCrossAZFactor = currFactorSum;
        } else {
            needCrossAZFactor = (localZone->cpu - avgCPU) * localZone->factorSum;
        }
        for (auto& sz : *serviceZones) {
            if (sz->cpu < avgCPU) {
                sz->idleZoneFactor = sz->factorSum * (avgCPU - sz->cpu);
                zoneFactorSum += sz->idleZoneFactor;
            }
        }
        for (auto& sz : *serviceZones) {
            if (sz->zone != this->zone) {
                sz->zoneFactor = needCrossAZFactor * sz->idleZoneFactor / zoneFactorSum;
            }
        }
    }
    if (!localZone->nodes.empty()) {
        localZone->zoneFactor = avgCPU * localZone->factorSum;
        zoneFactorSum += localZone->zoneFactor;
    } else {
        zoneFactorSum = currFactorSum;
    }

    if (logger != nullptr) {
        LOG4CPLUS_INFO(*(this->logger), "avgCPU: [" << avgCPU << "] zoneFactorSum: [" << zoneFactorSum << "] zone: [" << this->zone << "]");
        for (auto& item : *serviceZones) {
            LOG4CPLUS_INFO(*(this->logger), "update zone: [" << item->zone << "], serviceZone: [" << item->to_json().dump() << "]");
        }
    }

    this->serviceUpdaterMutex.lock();
    this->serviceZones  = serviceZones;
    this->localZone     = localZone;
    this->zoneFactorSum = zoneFactorSum;
    this->serviceUpdaterMutex.unlock();

    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::DiscoverNode() {
    this->serviceUpdaterMutex.lock_shared();
    auto serviceZones  = this->serviceZones;
    auto localZone     = this->localZone;
    auto zoneFactorSum = this->zoneFactorSum;
    this->serviceUpdaterMutex.unlock_shared();

    std::lock_guard<std::mutex> lock_guard(this->discoverMutex);
    int                         max         = 0;
    auto                        serviceZone = localZone;
    for (int i = 0; i < serviceZones->size(); i++) {
        (*serviceZones)[i]->zoneWeight += (*serviceZones)[i]->zoneFactor;
        if (max < (*serviceZones)[i]->zoneWeight) {
            max         = (*serviceZones)[i]->zoneWeight;
            serviceZone = (*serviceZones)[i];
        }
    }
    serviceZone->zoneWeight -= zoneFactorSum;

    int idx = 0;
    max     = 0;
    for (int i = 0; i < serviceZone->factors.size(); i++) {
        serviceZone->weights[i] += serviceZone->factors[i];
        if (max < serviceZone->weights[i]) {
            max = serviceZone->weights[i];
            idx = i;
        }
    }
    serviceZone->weights[idx] -= serviceZone->factorSum;

    return serviceZone->nodes[idx];
}
}
