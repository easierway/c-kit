#include "balancer/consul_balancer.h"
#include <algorithm>
#include <chrono>
#include <json11.hpp>
#include "util/util.h"

namespace kit {

ConsulResolver::ConsulResolver(const std::string& address,
                               const std::string& service,
                               const std::string& myService,
                               int intervalS, double ratio) {
    this->address        = address;
    this->service        = service;
    this->myService      = myService;
    this->intervalS      = intervalS;
    this->zone           = Zone();
    this->done           = false;
    this->cpuPercentage  = CPUUsage();
    this->ratio          = ratio;
    this->serviceUpdater = nullptr;
    this->factorUpdater  = nullptr;
    this->cpuUpdater     = nullptr;
}

std::tuple<int, std::string> ConsulResolver::Start() {
    std::string err;
    int         code;

    std::tie(code, err) = this->_resolve();
    if (code != 0) {
        return std::make_tuple(code, err);
    }

    std::tie(code, err) = this->_calFactorThreshold();
    if (code != 0) {
        return std::make_tuple(code, err);
    }

    this->serviceUpdater = new std::thread([&]() {
        while (!this->done) {
            this->_resolve();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->factorUpdater = new std::thread([&]() {
        while (!this->done) {
            this->_calFactorThreshold();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    this->cpuUpdater = new std::thread([&]() {
        while (!this->done) {
            this->cpuPercentage = CPUUsage();
            std::this_thread::sleep_for(std::chrono::seconds(this->intervalS));
        }
    });

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::Stop() {
    this->done = true;

    for (const auto& t : {this->serviceUpdater, this->factorUpdater, this->cpuUpdater}) {
        if (t != nullptr) {
            if (t->joinable()) {
                t->join();
            }
            delete t;
        }
    }
    this->serviceUpdater = nullptr;
    this->factorUpdater  = nullptr;
    this->cpuUpdater     = nullptr;

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_resolve() {
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

    //    var buf []byte
    //    buf, _ = json.Marshal(r.localZone)
    //    fmt.Printf("update localZone [%v], lastIndex [%v]\n", string(buf), r.lastIndex)
    //    buf, _ = json.Marshal(r.otherZone)
    //    fmt.Printf("update otherZone [%v], lastIndex [%v]\n", string(buf), r.lastIndex)

    return std::make_tuple(0, "");
}

std::tuple<int, std::string> ConsulResolver::_calFactorThreshold() {
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

    //    fmt.Printf("update factorThreshold [%v], lastIndex [%v]\n", r.factorThreshold, r.myLastIndex)

    return std::make_tuple(0, "");
}

std::shared_ptr<ServiceNode> ConsulResolver::DiscoverNode() {
    if (this->localZone->factorMax + this->otherZone->factorMax == 0) {
        return std::shared_ptr<ServiceNode>(nullptr);
    }

    auto factorThreshold = this->factorThreshold;
    if (this->ratio != 0) {
        auto m          = double((this->localZone->factorMax + this->otherZone->factorMax) * this->myServiceNum) * this->ratio;
        auto n          = double(this->localZone->factors.size() + this->otherZone->factors.size());
        factorThreshold = int(m / n);
    }
    factorThreshold = factorThreshold * this->cpuPercentage / 100;

    if (factorThreshold <= this->localZone->factorMax && this->localZone->factorMax > 0) {
        const auto& factors = this->localZone->factors;
        auto        idx     = std::lower_bound(factors.begin(), factors.end(), rand() % this->localZone->factorMax) - factors.begin();
        return this->localZone->nodes[idx];
    }

    auto factorMax = this->otherZone->factorMax + this->localZone->factorMax;
    if (factorMax > factorThreshold && factorThreshold > 0) {
        factorMax = factorThreshold;
    }
    auto factor = rand() % factorMax;
    if (factor < this->localZone->factorMax) {
        const auto& factors = this->localZone->factors;
        auto        idx     = std::lower_bound(factors.begin(), factors.end(), rand() % this->localZone->factorMax) - factors.begin();
        return this->localZone->nodes[idx];
    }

    const auto& factors = this->otherZone->factors;
    auto        idx     = std::lower_bound(factors.begin(), factors.end(), rand() % this->otherZone->factorMax) - factors.begin();
    return this->otherZone->nodes[idx];
}
}
