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
    int timeoutS) : client(address) {
    this->address = address;
    this->service = service;
    this->cpuThresholdKey = cpuThresholdKey,
    this->zoneCPUKey = zoneCPUKey,
    this->instanceFactorKey = instanceFactorKey,
    this->onlinelabFactorKey = onlinelabFactorKey,
    this->timeoutS = timeoutS;
    this->cpuThreshold = 0;
    this->lastIndex = "0";
    this->zone = Zone();
    this->metric = std::make_shared<ResolverMetric>();
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
    std::tie(code, err) = this->updateCandidatePool();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update candidate pool failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateZoneCPUMap() {
    static time_t lastUpdated = 0;
    int status = -1;
    json11::Json kv;
    std::string err;
    std::tie(status, kv, err) = this->client.GetKV(this->zoneCPUKey, this->timeoutS, this->lastIndex);
    if (status!=STATUSCODE::SUCCESS) {
        return std::make_tuple(status, err);
    }

    if (kv["data"].is_null() || kv["updated"].is_null()) {
        return std::make_tuple(STATUSCODE::ERROR_CONSUL_VALUE, "no correct key, please check zone cpu map");
    }

    // skip the same updated record
    time_t updated = static_cast<time_t>(kv["updated"].number_value());
    if(updated == lastUpdated) {
        this->zoneCPUUpdated = false;
        LOG4CPLUS_INFO(*(this->logger), "zone cpu no update, will hold factor learning");
        return std::make_tuple(STATUSCODE::SUCCESS, "");
    } else {
        lastUpdated = updated;
        this->zoneCPUUpdated = true;
    }

    auto zoneCPUMap = std::unordered_map<std::string, double>();
    for (const auto &item : kv["data"].array_items()) {
        for (const auto &zone: item.object_items()) {
            zoneCPUMap[zone.first] = zone.second.number_value();
        }
    }

    this->zoneCPUMap = zoneCPUMap;

//    LOG4CPLUS_INFO(*(this->logger), "update zoneCPUMap: [" << json11::Json(this->zoneCPUMap).dump() << "]");
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
    for (const auto &item : kv["data"].array_items()) {
        instanceFactorMap[item["instanceid"].string_value()] = item["CPUUtilization"].number_value();
    }

    this->instanceFactorMap = instanceFactorMap;

//    LOG4CPLUS_INFO(*(this->logger),
//                   "update instanceFactorMap: [" << json11::Json(this->instanceFactorMap).dump() << "]");
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
    // TODO: return error when not enough parameter provided
    if (!kv["rateThreshold"].is_null()) {
        this->onlinelab.rateThreshold = kv["rateThreshold"].number_value();
    }
    if (!kv["learningRate"].is_null()) {
        this->onlinelab.learningRate = kv["learningRate"].number_value();
    }
    if (!kv["crossZoneRate"].is_null()) {
        this->onlinelab.crossZoneRate = kv["crossZoneRate"].number_value();
    }

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateServiceZone() {
    int status = -1;
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string err;
    std::tie(status, nodes, err) = this->client.GetService(this->service, this->timeoutS, this->lastIndex);
    if (status!=STATUSCODE::SUCCESS) {
        return std::make_tuple(status, err);
    }

    std::unordered_map<std::string, std::shared_ptr<ServiceZone>> serviceZoneMap;
    for (auto &node : nodes) {
        if (this->instanceFactorMap.count(node->instanceID)==0) {
            // TODO: default 100?
            node->workload = 50;
        } else {
            // TODO: enlarge the map content
            node->workload = this->instanceFactorMap[node->instanceID];
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

    this->serviceZones = serviceZones;
    this->localZone = localZone;

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::updateCandidatePool() {
    auto localZone = this->localZone;
    auto serviceZones = this->serviceZones;
    auto &balanceFactorCache = this->balanceFactorCache;
    auto candidatePool = std::make_shared<CandidatePool>();
    static auto BALANCEFACTOR_MAX = 10000;
    static auto BALANCEFACTOR_MIN = 100;
    for (auto &serviceZone : *serviceZones) {
        if (localZone->zone==serviceZone->zone) {
            for (auto &node : serviceZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                auto balanceFactor = node->balanceFactor;
                if (balanceFactorCache.count(node->instanceID) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceID];
                }
                if (this->zoneCPUUpdated && abs(node->workload - serviceZone->workload)/100.0 > this->onlinelab.rateThreshold) {
                    if (node->workload > serviceZone->workload) {
                        balanceFactor -= balanceFactor*this->onlinelab.learningRate;
                    } else {
                        balanceFactor += balanceFactor*this->onlinelab.learningRate;
                    }
                }
                // risk control
                if (balanceFactor > BALANCEFACTOR_MAX) {
                    balanceFactor = BALANCEFACTOR_MAX;
                } else if (balanceFactor < BALANCEFACTOR_MIN) {
                    balanceFactor = BALANCEFACTOR_MIN;
                }
                node->currentFactor = balanceFactor;
                candidatePool->factors.emplace_back(balanceFactor);
                candidatePool->factorSum += balanceFactor;
                balanceFactorCache[node->instanceID] = balanceFactor;
            }
        } else if (localZone->nodes.empty()
            || localZone->workload > this->cpuThreshold && localZone->workload > serviceZone->workload) {
            for (auto &node: serviceZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                auto balanceFactor = node->balanceFactor;
                // TODO: cross zone adjust
                balanceFactor =
                    balanceFactor*(localZone->workload - serviceZone->workload)/100*this->onlinelab.crossZoneRate;
                if (balanceFactorCache.count(node->instanceID) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceID];
                }
                if (this->zoneCPUUpdated && abs(node->workload - serviceZone->workload)/100.0 > this->onlinelab.rateThreshold) {
                    if (node->workload > serviceZone->workload) {
                        balanceFactor -= balanceFactor*this->onlinelab.learningRate;
                    } else {
                        balanceFactor += balanceFactor*this->onlinelab.learningRate;
                    }
                }
                // risk control
                if (balanceFactor > BALANCEFACTOR_MAX) {
                    balanceFactor = BALANCEFACTOR_MAX;
                } else if (balanceFactor < BALANCEFACTOR_MIN) {
                    balanceFactor = BALANCEFACTOR_MIN;
                }
                node->currentFactor = balanceFactor;
                candidatePool->factors.emplace_back(balanceFactor);
                candidatePool->factorSum += balanceFactor;
                balanceFactorCache[node->instanceID] = balanceFactor;
            }
        }
    }

    // metric
    auto metric = std::make_shared<ResolverMetric>();
    metric->candidatePoolSize = candidatePool->nodes.size();

    this->serviceUpdaterMutex.lock();
    LOG4CPLUS_INFO(*(this->logger), "previous metric: " << this->metric->to_json().dump());
    this->candidatePool = candidatePool;
    this->metric = metric;
    this->serviceUpdaterMutex.unlock();
    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::SelectedNode() {
    this->serviceUpdaterMutex.lock_shared();
    auto candidatePool = this->candidatePool;
    auto metric = this->metric;
    this->serviceUpdaterMutex.unlock_shared();
    std::lock_guard<std::mutex> lock_guard(this->discoverMutex);

    int idx = 0;
    double max = 0;
    for (int i = 0; i < candidatePool->factors.size(); i++) {
        candidatePool->weights[i] += candidatePool->factors[i];
        if (max < candidatePool->weights[i]) {
            max = candidatePool->weights[i];
            idx = i;
        }
    }
    candidatePool->weights[idx] -= candidatePool->factorSum;

    // metric
    metric->selectNum += 1;
    if (candidatePool->nodes[idx]->zone != this->zone) {
        metric->crossZoneNum += 1;
    }

    LOG4CPLUS_DEBUG(*(this->logger), "SelectedNode: " << candidatePool->nodes[idx]->to_json().dump());
    return candidatePool->nodes[idx];
}

}
