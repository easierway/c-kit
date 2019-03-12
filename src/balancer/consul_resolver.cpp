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
        return std::make_tuple(code, err);
    }
    std::tie(code, err) = this->updateZoneCPUMap();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update zoneCPUMap failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    std::tie(code, err) = this->updateOnlinelabFactor();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update onlinelabFactor failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    std::tie(code, err) = this->updateInstanceFactorMap();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger),
                       "update instanceFactorMap failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }
    std::tie(code, err) = this->updateServiceZone();
    if (code!=0 && this->logger!=nullptr) {
        LOG4CPLUS_WARN(*(this->logger), "update serviceZone failed. code: [" << code << "], err: [" << err << "]");
        return std::make_tuple(code, err);
    }

    this->expireBalanceFactorCache();
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
    if (updated==lastUpdated) {
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

    LOG4CPLUS_DEBUG(*(this->logger),
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
    // TODO: return error when not enough parameter provided
    if (!kv["rateThreshold"].is_null()) {
        this->onlinelab.rateThreshold = kv["rateThreshold"].number_value();
    } else {
        this->onlinelab.rateThreshold = 0.1;
    }
    if (!kv["learningRate"].is_null()) {
        this->onlinelab.learningRate = kv["learningRate"].number_value();
    } else {
        this->onlinelab.learningRate = 0.01;
    }
    if (!kv["crossZoneRate"].is_null()) {
        this->onlinelab.crossZoneRate = kv["crossZoneRate"].number_value();
    } else {
        this->onlinelab.crossZoneRate = 0.01;
    }
    // balance factor cache expire control, default if null
    if (!kv["factorCacheExpire"].is_null()) {
        this->onlinelab.factorCacheExpire = kv["factorCacheExpire"].number_value();
    } else {
        this->onlinelab.factorCacheExpire = 300;
    }
    // enable cross zone or not, true default
    if (!kv["crossZone"].is_null()) {
        this->onlinelab.crossZone = kv["crossZone"].bool_value();
    } else {
        this->onlinelab.crossZone = true;
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
        LOG4CPLUS_INFO(*(this->logger), "zone: " << item.first << " node: " << item.second->nodes.size());
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
    // TODO: using onlinelab
    static auto BALANCEFACTOR_MAX_LOCAL = 3000;
    static auto BALANCEFACTOR_MIN_LOCAL = 200;
    static auto BALANCEFACTOR_MAX_CROSS = 1000;
    static auto BALANCEFACTOR_MIN_CROSS = 50;
    for (auto &serviceZone : *serviceZones) {
        if (localZone->zone==serviceZone->zone) {
            for (auto &node : serviceZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                auto balanceFactor = node->balanceFactor;
                if (balanceFactorCache.count(node->instanceID) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceID];
                }
                if (this->zoneCPUUpdated && not nodeBalanced(*node, *serviceZone)) {
                    if (node->workload > serviceZone->workload) {
                        balanceFactor -= balanceFactor*this->onlinelab.learningRate;
                    } else {
                        balanceFactor += balanceFactor*this->onlinelab.learningRate;
                    }
                }
                // risk control
                if (balanceFactor > BALANCEFACTOR_MAX_LOCAL) {
                    balanceFactor = BALANCEFACTOR_MAX_LOCAL;
                } else if (balanceFactor < BALANCEFACTOR_MIN_LOCAL) {
                    balanceFactor = BALANCEFACTOR_MIN_LOCAL;
                }
                node->currentFactor = balanceFactor;
                candidatePool->factors.emplace_back(balanceFactor);
                candidatePool->factorSum += balanceFactor;
                balanceFactorCache[node->instanceID] = balanceFactor;
            }
        } else if (this->onlinelab.crossZone) {
            // cross zone
            for (auto &node: serviceZone->nodes) {
                candidatePool->nodes.emplace_back(node);
                candidatePool->weights.emplace_back(0);

                // initial balanceFactor if cached, use cache
                auto balanceFactor = node->balanceFactor;
                if (balanceFactorCache.count(node->instanceID) > 0) {
                    balanceFactor = balanceFactorCache[node->instanceID];
                } else {
                    // cross zone threshold double the node threshold
                    if (not zoneBalanced(*localZone, *serviceZone) && localZone->workload > serviceZone->workload) {
                        balanceFactor = balanceFactor*(localZone->workload - serviceZone->workload)/100.0;
                    } else {
                        balanceFactor = BALANCEFACTOR_MIN_CROSS;
                    }
                }

                if (this->zoneCPUUpdated) {
                    // update cross zone all node factor
                    if (localZone->workload > this->cpuThreshold && not zoneBalanced(*localZone, *serviceZone)
                        && localZone->workload > serviceZone->workload) {
                        balanceFactor += balanceFactor*this->onlinelab.learningRate;
                    } else {
                        balanceFactor -= balanceFactor*this->onlinelab.learningRate;
                    }

                    // update single lazy/busy node factor
                    if (not nodeBalanced(*node, *serviceZone)) {
                        if (node->workload > serviceZone->workload) {
                            balanceFactor -= balanceFactor*this->onlinelab.learningRate;
                        } else {
                            balanceFactor += balanceFactor*this->onlinelab.learningRate;
                        }
                    }
                }
                // risk control
                if (balanceFactor > BALANCEFACTOR_MAX_CROSS) {
                    balanceFactor = BALANCEFACTOR_MAX_CROSS;
                } else if (balanceFactor < BALANCEFACTOR_MIN_CROSS) {
                    balanceFactor = BALANCEFACTOR_MIN_CROSS;
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

std::tuple<int, std::string> ConsulResolver::expireBalanceFactorCache() {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    static std::uniform_int_distribution<int> dist(1, this->onlinelab.factorCacheExpire);
    if (1==dist(mt)) {
        this->balanceFactorCache.clear();
        LOG4CPLUS_INFO(*(this->logger), "balanceFactorCache expired");
    } else {
        LOG4CPLUS_DEBUG(*(this->logger), "balanceFactorCache alive");
    }
    return std::make_tuple(0, "");
}

bool ConsulResolver::nodeBalanced(const kit::ServiceNode &node, const kit::ServiceZone &zone) {
    return abs(node.workload - zone.workload)/100.0 < this->onlinelab.rateThreshold;
}

bool ConsulResolver::zoneBalanced(const kit::ServiceZone &localZone, const kit::ServiceZone &crossZone) {
    // TODO: using onlinelab
    return abs(localZone.workload - crossZone.workload)/100.0 < this->onlinelab.rateThreshold*2;
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
    if (candidatePool->nodes[idx]->zone!=this->zone) {
        metric->crossZoneNum += 1;
    }

    LOG4CPLUS_DEBUG(*(this->logger), "SelectedNode: " << candidatePool->nodes[idx]->to_json().dump());
    return candidatePool->nodes[idx];
}

}
