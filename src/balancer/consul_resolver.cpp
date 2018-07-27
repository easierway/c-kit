#include "balancer/consul_resolver.h"
#include <algorithm>
#include <chrono>
#include <json11.hpp>
#include "util/util.h"

namespace kit {

ConsulResolver::ConsulResolver(const std::string& address,
                               const std::string& service,
                               const std::string& myService,
                               int intervalS, double ratio) {
    this->address                = address;
    this->service                = service;
    this->myService              = myService;
    this->intervalS              = intervalS;
    this->zone                   = Zone();
    this->done                   = false;
    this->cpuPercentage          = CPUUsage();
    this->ratio                  = ratio;
    this->serviceUpdater         = nullptr;
    this->factorThresholdUpdater = nullptr;
    this->cpuUpdater             = nullptr;
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
            this->_updateServiceZone();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->factorThresholdUpdater = new std::thread([&]() {
        while (!this->done) {
            this->_updateFactorThreshold();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->cpuUpdater = new std::thread([&]() {
        while (!this->done) {
            this->cpuPercentage = CPUUsage();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    std::cout << "consul resolver start " << this->to_json().dump() << std::endl;

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
    std::string body;
    int         status = -1;
    std::string err;
    std::tie(body, status, err) = HttpGet(this->address + "/v1/health/service/" + this->service + "?passing=true");
    if (status != 200) {
        return std::make_tuple(-1, "HttpGet failed. err [" + err + "]");
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, "JsonParse. err [" + err + "]");
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

    // TODO rwlock
    this->localZone = localZone;
    this->otherZone = otherZone;

    std::cout << "update localZone [" << this->localZone->to_json().dump() << "]" << std::endl;
    std::cout << "update otherZone [" << this->otherZone->to_json().dump() << "]" << std::endl;

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_updateFactorThreshold() {
    std::string body;
    int         status;
    std::string err;
    std::tie(body, status, err) = HttpGet(this->address + "/v1/health/service/" + this->myService + "?passing=true");
    if (status != 200) {
        return std::make_tuple(-1, "HttpGet failed. err [" + err + "]");
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, "JsonParse. err [" + err + "]");
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

    // TODO rwlock
    this->factorThreshold = factorThreshold;
    this->myServiceNum    = myServiceNum;

    std::cout << "update factorThreadhold [" << factorThreshold << "]" << std::endl;
    std::cout << "update myServiceNum [" << myServiceNum << "]" << std::endl;

    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::DiscoverNode() {
    auto localZone = this->localZone;
    auto otherZone = this->otherZone;

    if (localZone->factorMax + otherZone->factorMax == 0) {
        return std::shared_ptr<ServiceNode>(nullptr);
    }

    auto factorThreshold = this->factorThreshold;
    if (this->ratio != 0) {
        auto m          = double((localZone->factorMax + otherZone->factorMax) * this->myServiceNum) * this->ratio;
        auto n          = double(localZone->factors.size() + otherZone->factors.size());
        factorThreshold = int(m / n);
    }
    factorThreshold = factorThreshold * this->cpuPercentage / 100;

    if (factorThreshold <= localZone->factorMax && localZone->factorMax > 0) {
        const auto& factors = localZone->factors;
        auto        idx     = std::lower_bound(factors.begin(), factors.end(), rand() % localZone->factorMax) - factors.begin();
        return this->localZone->nodes[idx];
    }

    auto factorMax = otherZone->factorMax + localZone->factorMax;
    if (factorMax > factorThreshold && factorThreshold > 0) {
        factorMax = factorThreshold;
    }
    auto serviceZone = localZone;
    auto factor      = rand() % factorMax;
    if (factor >= localZone->factorMax) {
        serviceZone = otherZone;
    }
    auto& factors = serviceZone->factors;
    auto  idx     = std::lower_bound(factors.begin(), factors.end(), rand() % serviceZone->factorMax) - factors.begin();
    return serviceZone->nodes[idx];
}
}
