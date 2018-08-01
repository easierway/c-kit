#include "balancer/consul_resolver.h"
#include <log4cplus/loggingmacros.h>
#include <algorithm>
#include <chrono>
#include <json11.hpp>
#include "util/util.h"

namespace kit {

ConsulResolver::ConsulResolver(
    const std::string& service,
    const std::string& myService,
    const std::string& address,
    int                intervalS,
    double             serviceRatio,
    double             cpuThreshold) {
    this->address                = address;
    this->service                = service;
    this->myService              = myService;
    this->intervalS              = intervalS;
    this->serviceRatio           = serviceRatio;
    this->cpuThreshold           = cpuThreshold;
    this->done                   = false;
    this->lastIndex              = "0";
    this->myLastIndex            = "0";
    this->zone                   = Zone();
    this->cpuUsage               = CPUUsage();
    this->serviceUpdater         = nullptr;
    this->factorThresholdUpdater = nullptr;
    this->cpuUpdater             = nullptr;
    this->logger                 = nullptr;
}

std::tuple<int, std::string> ConsulResolver::Start() {
    std::string err;
    int         code;

    std::tie(code, err) = this->_updateServiceZone();
    if (code != 0) {
        return std::make_tuple(code, err);
    }

    std::tie(code, err) = this->_updateFactorThreshold();
    if (code != 0) {
        return std::make_tuple(code, err);
    }

    this->serviceUpdater = new std::thread([&]() {
        while (!this->done) {
            int         code;
            std::string err;
            std::tie(code, err) = this->_updateServiceZone();
            if (code != 0 && this->logger != nullptr) {
                LOG4CPLUS_WARN(*(this->logger), "update service zone failed. code: [" << code << "], err: [" << err << "]");
            }
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->factorThresholdUpdater = new std::thread([&]() {
        while (!this->done) {
            int         code;
            std::string err;
            std::tie(code, err) = this->_updateFactorThreshold();
            if (code != 0 && this->logger != nullptr) {
                LOG4CPLUS_WARN(*(this->logger), "update factor threshold failed. code: [" << code << "], err: [" << err << "]");
            }
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->cpuUpdater = new std::thread([&]() {
        while (!this->done) {
            int usage = CPUUsage();
            if (usage <= 0) {
                this->cpuUsage = 1;
            } else {
                this->cpuUsage = usage;
            }
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

    for (const auto& t : {this->serviceUpdater, this->factorThresholdUpdater, this->cpuUpdater}) {
        if (t != nullptr) {
            if (t->joinable()) {
                t->join();
            }
            delete t;
        }
    }
    this->serviceUpdater         = nullptr;
    this->factorThresholdUpdater = nullptr;
    this->cpuUpdater             = nullptr;

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateServiceZone() {
    std::string                        body;
    int                                status = -1;
    std::string                        err;
    std::map<std::string, std::string> header;
    std::tie(status, body, header, err) = HttpGet(this->address + "/v1/health/service/" + this->service + "?passing=true&index=" + this->lastIndex, std::map<std::string, std::string>{});
    for (const auto& kv : header) {
        std::cout << kv.first << " => " << kv.second << std::endl;
    }
    if (status != 200) {
        return std::make_tuple(-1, "HttpGet failed. err [" + err + "]");
    }
    if (header.count("X-Consul-Index") > 0) {
        this->lastIndex = header["X-Consul-Index"];
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, "Json parse failed. err [" + err + "]");
    }

    auto localZone = std::make_shared<ServiceZone>();
    auto otherZone = std::make_shared<ServiceZone>();

    for (const auto& service : jsonObj.array_items()) {
        int         balanceFactor = 100;
        std::string zone          = "unknown";
        if (service["Service"].is_null()) {
            continue;
        }
        if (!service["Service"]["Meta"]["balanceFactor"].is_null()) {
            balanceFactor = std::stoi(service["Service"]["Meta"]["balanceFactor"].string_value());
        }
        if (!service["Service"]["Meta"]["zone"].is_null()) {
            zone = service["Service"]["Meta"]["zone"].string_value();
        }

        auto node           = std::make_shared<ServiceNode>();
        node->host          = service["Service"]["Address"].string_value();
        node->port          = service["Service"]["Port"].int_value();
        node->balanceFactor = balanceFactor;
        node->zone          = zone;

        if (node->zone == this->zone) {
            localZone->nodes.emplace_back(node);
            localZone->factorMax += node->balanceFactor;
            localZone->factors.emplace_back(localZone->factorMax);
        } else {
            otherZone->nodes.emplace_back(node);
            otherZone->factorMax += node->balanceFactor;
            otherZone->factors.emplace_back(otherZone->factorMax);
        }
    }

    this->serviceUpdaterMutex.lock();
    this->localZone = localZone;
    this->otherZone = otherZone;
    this->serviceUpdaterMutex.unlock();

    if (logger != nullptr) {
        LOG4CPLUS_INFO(*(this->logger), "update localZone [" << this->localZone->to_json().dump() << "]");
        LOG4CPLUS_INFO(*(this->logger), "update otherZone [" << this->otherZone->to_json().dump() << "]");
    }

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateFactorThreshold() {
    std::string                        body;
    int                                status;
    std::string                        err;
    std::map<std::string, std::string> header;
    std::tie(status, body, header, err) = HttpGet(this->address + "/v1/health/service/" + this->myService + "?passing=true&index=" + this->myLastIndex, std::map<std::string, std::string>{});
    for (const auto& kv : header) {
        std::cout << kv.first << " => " << kv.second << std::endl;
    }
    if (status != 200) {
        return std::make_tuple(-1, "HttpGet failed. err [" + err + "]");
    }
    if (header.count("X-Consul-Index") > 0) {
        this->myLastIndex = header["X-Consul-Index"];
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, "Json parse failed. err [" + err + "]");
    }

    int factorThreshold = 0;
    int myServiceNum    = 0;
    for (const auto& service : jsonObj.array_items()) {
        int         balanceFactor = 100;
        std::string zone          = "unknown";
        if (service["Service"].is_null()) {
            continue;
        }
        if (!service["Service"]["Meta"]["balanceFactor"].is_null()) {
            balanceFactor = std::stoi(service["Service"]["Meta"]["balanceFactor"].string_value());
        }
        if (!service["Service"]["Meta"]["zone"].is_null()) {
            zone = service["Service"]["Meta"]["zone"].string_value();
        }

        if (zone == this->zone) {
            factorThreshold += balanceFactor;
            myServiceNum++;
        }
    }

    this->factorThreshold = factorThreshold;
    this->myServiceNum    = myServiceNum;

    if (logger != nullptr) {
        LOG4CPLUS_INFO(*(this->logger), "update factorThreadhold [" << factorThreshold << "]");
        LOG4CPLUS_INFO(*(this->logger), "update myServiceNum [" << myServiceNum << "]");
    }

    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::DiscoverNode() {
    this->serviceUpdaterMutex.lock_shared();
    auto localZone = this->localZone;
    auto otherZone = this->otherZone;
    this->serviceUpdaterMutex.unlock_shared();

    if (localZone->factorMax + otherZone->factorMax == 0) {
        return std::shared_ptr<ServiceNode>(nullptr);
    }

    auto factorThreshold = this->factorThreshold;
    if (this->serviceRatio != 0) {
        auto m          = double((localZone->factorMax + otherZone->factorMax) * this->myServiceNum) * this->serviceRatio;
        auto n          = double(localZone->factors.size() + otherZone->factors.size());
        factorThreshold = int(m / n);
    }
    factorThreshold = factorThreshold * this->cpuUsage / 100;
    if (this->cpuThreshold != 0) {
        factorThreshold = int(double(factorThreshold) / this->cpuThreshold);
    }

    auto serviceZone = localZone;
    if (factorThreshold > localZone->factorMax || localZone->factorMax <= 0) {
        auto factorMax = otherZone->factorMax + localZone->factorMax;
        if (factorMax > factorThreshold && factorThreshold > 0) {
            factorMax = factorThreshold;
        }
        auto factor = rand() % factorMax;
        if (factor >= localZone->factorMax) {
            serviceZone = otherZone;
        }
    }
    auto& factors = serviceZone->factors;
    auto  idx     = std::lower_bound(factors.begin(), factors.end(), rand() % serviceZone->factorMax) - factors.begin();
    return serviceZone->nodes[idx];
}
}
