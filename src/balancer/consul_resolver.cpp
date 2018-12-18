#include "balancer/consul_resolver.h"
#include <log4cplus/loggingmacros.h>
#include <algorithm>
#include <chrono>
#include <json11.hpp>
#include <random>
#include "util/util.h"
#include "util/constant.h"

namespace kit {

ConsulResolver::ConsulResolver(
    const std::string &address,
    const std::string &service,
    const std::string &cpuThresholdKey,
    const std::string &zoneCPUKey,
    const std::string &instanceFactorKey,
    const std::string &onlinelabFactorKey,
    int intervalS,
    int timeoutS) : client(address) {
    this->address = address;
    this->service = service;
    this->cpuThresholdKey = cpuThresholdKey,
    this->zoneCPUKey = zoneCPUKey,
    this->instanceFactorKey = instanceFactorKey,
    this->onlinelabFactorKey = onlinelabFactorKey,
    this->intervalS = intervalS;
    this->timeoutS = timeoutS;
    this->cpuThreshold = 0;
    this->lastIndex = "0";
    this->zone = Zone();
    this->logger = nullptr;
}

std::tuple<int, std::string> ConsulResolver::updateAll() {
    int code;
    std::string err;
    std::tie(code, err) = this->updateCPUThreshold();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update CPU threshold failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->updateZoneCPUMap();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update zoneCPUMap failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->updateOnlinelabFactor();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update onlinelabFactor failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->updateInstanceFactorMap();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger),
                       "update instanceFactorMap failed. code: [" << code << "], err: [" << err << "]");
    }
    std::tie(code, err) = this->updateServiceZone();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update serviceZone failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateZoneCPUMap() {
    int status = -1;
    json11::Json kv;
    std::string err;
    std::tie(status, kv, err) = this->client.GetKV(this->zoneCPUKey, this->timeoutS, this->lastIndex);
    if (status!=0) {
        return std::make_tuple(status, err);
    }

    if (kv["data"].is_null()) {
        return std::make_tuple(STATUSCODE::ERROR_CONSUL_VALUE, "no data key, please check zone cpu map");
    }

    auto zoneCPUMap = std::unordered_map<std::string, double>();
    for (const auto &item : kv["data"].array_items()) {
        for (const auto &zone: item.object_items()) {
            zoneCPUMap[zone.first] = zone.second.number_value();
        }
    }

    this->zoneCPUMap = zoneCPUMap;

    LOG4CPLUS_INFO(*(this->logger), "update zoneCPUMap: [" << json11::Json(this->zoneCPUMap).dump() << "]");
    return std::make_tuple(STATUSCODE::SUCCESS, "");
}

std::tuple<int, std::string> ConsulResolver::updateInstanceFactorMap() {
    int status = -1;
    json11::Json kv;
    std::string err;
    std::tie(status, kv, err) = this->client.GetKV(this->instanceFactorKey, this->timeoutS, this->lastIndex);
    if (status!=STATUSCODE::SUCCESS) {
        return std::make_tuple(status, err);
    }
    if (kv["data"].is_null()) {
        return std::make_tuple(STATUSCODE::ERROR_CONSUL_VALUE, "no data key, please check instance factor");
    }
    auto instanceFactorMap = std::unordered_map<std::string, double>();
    // TODO: enlarge the content
    for (const auto &item : kv["data"].array_items()) {
        instanceFactorMap[item["instanceid"].string_value()] = item["CPUUtilization"].number_value();
    }

    this->instanceFactorMap = instanceFactorMap;

    LOG4CPLUS_INFO(*(this->logger),
                   "update instanceFactorMap: [" << json11::Json(this->instanceFactorMap).dump() << "]");
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateCPUThreshold() {
    int status = -1;
    json11::Json kv;
    std::string err;
    std::tie(status, kv, err) = this->client.GetKV(this->cpuThresholdKey, this->timeoutS, this->lastIndex);
    if (status!=0) {
        LOG4CPLUS_INFO(*(this->logger), "update cpuThreshold: [" << this->cpuThreshold << "]");
        return std::make_tuple(status, err);
    }
    if (!kv["cpuThreshold"].is_null()) {
        this->cpuThreshold = kv["cpuThreshold"].int_value();
    }

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateOnlinelabFactor() {
    int status = -1;
    json11::Json kv;
    std::string err;
    std::tie(status, kv, err) = this->client.GetKV(this->onlinelabFactorKey, this->timeoutS, this->lastIndex);
    if (status!=0) {
        LOG4CPLUS_ERROR(*(this->logger), "update OnlinelabFactor [" << this->onlinelabFactorKey << "] failed. " << err);
        return std::make_tuple(status, err);
    }
    if (!kv["rateThreshold"].is_null()) {
        this->rateThreshold = kv["rateThreshold"].number_value();
    }
    if (!kv["learningRate"].is_null()) {
        this->learningRate = kv["learningRate"].number_value();
    }

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateServiceZone() {
    int status = -1;
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string err;
    std::tie(status, nodes, err) = this->client.GetService(this->service, this->intervalS, this->lastIndex);
    if (status!=0) {
        return std::make_tuple(status, err);
    }

    std::unordered_map<std::string, std::shared_ptr<ServiceZone>> serviceZoneMap;
    for (auto &node : nodes) {
        if (this->instanceFactorMap.count(node->instanceid)==0) {
            // TODO: default 100?
            node->workload = 50;
        } else {
            // TODO: enlarge the map content
            node->workload = this->instanceFactorMap[node->instanceid];
        }
        if (serviceZoneMap.count(node->zone)==0) {
            serviceZoneMap[node->zone] = std::make_shared<ServiceZone>();
            serviceZoneMap[node->zone]->zone = node->zone;
            if (this->zoneCPUMap.count(node->zone)==0) {
                // TODO: default 100?
                serviceZoneMap[node->zone]->workload = 50;
            } else {
                serviceZoneMap[node->zone]->workload = this->zoneCPUMap[node->zone];
            }
        }
        serviceZoneMap[node->zone]->nodes.emplace_back(node);
    }

    auto serviceZones = std::make_shared<std::vector<std::shared_ptr<ServiceZone>>>();
    auto localZone = std::make_shared<ServiceZone>();
    for (auto &item : serviceZoneMap) {
        serviceZones->emplace_back(item.second);
        if (item.second->zone==this->zone) {
            localZone = item.second;
        }
    }
/*
    int allFactorSum = 0;
    int currFactorSum = 0;
    int zoneFactorSum = 0;
    for (auto &sz : *serviceZones) {
        allFactorSum += sz->factorSum;
        // TODO: cpu is 0.123?
        currFactorSum += sz->factorSum*sz->cpu;
    }
    int avgCPU = currFactorSum/allFactorSum;
    if (localZone->nodes.empty() || (avgCPU < localZone->cpu && localZone->cpu > this->cpuThreshold)) {
        int needCrossAZFactor = 0;
        if (localZone->nodes.empty()) {
            needCrossAZFactor = currFactorSum;
        } else {
            needCrossAZFactor = (localZone->cpu - avgCPU)*localZone->factorSum;
        }
        for (auto &sz : *serviceZones) {
            if (sz->cpu < avgCPU) {
                sz->idleZoneFactor = sz->factorSum*(avgCPU - sz->cpu);
                zoneFactorSum += sz->idleZoneFactor;
            }
        }
        for (auto &sz : *serviceZones) {
            if (sz->zone!=this->zone) {
                sz->zoneFactor = needCrossAZFactor*sz->idleZoneFactor/zoneFactorSum;
            }
        }
    }
    if (!localZone->nodes.empty()) {
        localZone->zoneFactor = avgCPU*localZone->factorSum;
        zoneFactorSum += localZone->zoneFactor;
    } else {
        zoneFactorSum = currFactorSum;
    }

    if (logger!=nullptr) {
        LOG4CPLUS_INFO(*(this->logger),
                       "avgCPU: [" << avgCPU << "] zoneFactorSum: [" << zoneFactorSum << "] zone: [" << this->zone
                                   << "]");
        for (auto &item : *serviceZones) {
            LOG4CPLUS_INFO(*(this->logger),
                           "update zone: [" << item->zone << "], serviceZone: [" << item->to_json().dump() << "]");
        }
    }
*/
    this->serviceZones = serviceZones;
    this->localZone = localZone;
    this->updateCandidatePool();

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateCandidatePool() {
    auto localZone = this->localZone;
    auto serviceZones = this->serviceZones;
    auto &balanceFactorCache = this->balanceFactorCache;
    auto candidatePool = std::make_shared<CandidatePool>();
    for (auto &serviceZone : *serviceZones) {
        if (localZone->zone==serviceZone->zone) {
            for (auto &node : localZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                auto balanceFactor = node->balanceFactor;
                if (balanceFactorCache.count(node->instanceid) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceid];
                }
                if (abs(node->workload - serviceZone->workload) > this->rateThreshold) {
                    balanceFactor += balanceFactor*(node->workload - serviceZone->workload)/100*this->learningRate;
                }
                candidatePool->factors.emplace_back(balanceFactor);
                candidatePool->factorSum += balanceFactor;
            }
        } else if (localZone->nodes.empty()
            || localZone->workload > this->cpuThreshold && localZone->workload > serviceZone->workload) {
            for (auto &node: serviceZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                auto balanceFactor = node->balanceFactor;
                if (balanceFactorCache.count(node->instanceid) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceid];
                }
                // TODO: cross zone adjust
//                balanceFactor = balanceFactor*(localZone->workload - serviceZone->workload)/100;
                if (abs(node->workload - serviceZone->workload) > this->rateThreshold) {
                    balanceFactor += balanceFactor*(node->workload - serviceZone->workload)/100*this->learningRate;
                }
                candidatePool->factors.emplace_back(balanceFactor);
                candidatePool->factorSum += balanceFactor;
            }
        }
    }

    this->serviceUpdaterMutex.lock();
    this->candidatePool = candidatePool;
    this->serviceUpdaterMutex.unlock();
    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::SelectedNode() {
    this->serviceUpdaterMutex.lock_shared();
    auto candidatePool = this->candidatePool;
    this->serviceUpdaterMutex.unlock_shared();
    std::lock_guard<std::mutex> lock_guard(this->discoverMutex);

    int idx = 0, max = 0;
    for (int i = 0; i < candidatePool->factors.size(); i++) {
        candidatePool->weights[i] += candidatePool->factors[i];
        if (max < candidatePool->weights[i]) {
            max = candidatePool->weights[i];
            idx = i;
        }
    }
    candidatePool->weights[idx] -= candidatePool->factorSum;

    return candidatePool->nodes[idx];
}

}
