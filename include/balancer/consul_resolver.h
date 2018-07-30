#pragma once

#include <log4cplus/logger.h>
#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <json11.hpp>
#include <sstream>
#include <thread>
#include <vector>

namespace kit {

struct ServiceNode {
    std::string host;
    int         port;
    std::string zone;
    int         balanceFactor;

    std::string Address() {
        std::stringstream ss;
        ss << this->host << ":" << this->port;
        return ss.str();
    }

    json11::Json to_json() const {
        return json11::Json::object{
            {"host", this->host},
            {"port", this->port},
            {"zone", this->zone},
            {"balancerFactor", this->balanceFactor},
        };
    }
};

struct ServiceZone {
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::vector<int>                          factors;
    int                                       factorMax;

    json11::Json to_json() const {
        std::vector<ServiceNode> nodes(this->nodes.size());
        for (int i = 0; i < this->nodes.size(); i++) {
            nodes[i] = *(this->nodes[i]);
        }
        return json11::Json::object{
            {"nodes", nodes},
            {"factors", this->factors},
            {"factorMax", this->factorMax},
        };
    }
};

class ConsulResolver {
    std::string                  address;              // consul 地址，一般为本地 agent
    std::string                  service;              // 要访问的服务名
    std::string                  myService;            // 本服务名
    std::string                  zone;                 // 服务地区
    double                       serviceRatio;         // 要访问的服务与本服务比例
    double                       cpuThreshold;         // cpu 阀值，根据 qps 预测要访问的服务 cpu，超过阀值，跨 zone 访问，[0,1]
    int                          factorThreshold;      // 本地区服务负载的阀值，超过该负载将跨 zone 访问
    int                          myServiceNum;         // local zone 的服务数量
    std::shared_ptr<ServiceZone> localZone;            // 本地服务列表
    std::shared_ptr<ServiceZone> otherZone;            // 其他 zone 服务列表
    int                          cpuUsage;             // cpu 使用率， [1,100]
    int                          intervalS;            // 服务列表更新最小间隔秒数
    bool                         done;                 // 退出标记
    boost::shared_mutex          serviceUpdaterMutex;  // 服务更新锁
    log4cplus::Logger*           logger;               // 日志

    json11::Json to_json() const {
        return json11::Json::object{
            {"address", this->address},
            {"service", this->service},
            {"myService", this->myService},
            {"zone", this->zone},
            {"factorThreshold", this->factorThreshold},
            {"myServiceNum", this->myServiceNum},
            {"intervalS", this->intervalS},
            {"serviceRatio", this->serviceRatio},
            {"cpuThreshold", this->cpuThreshold},
            {"cpuUsage", this->cpuUsage},
            {"localZone", this->localZone->to_json()},
            {"otherZone", this->otherZone->to_json()},
        };
    }

    std::thread* serviceUpdater;
    std::thread* factorThresholdUpdater;
    std::thread* cpuUpdater;

    std::tuple<int, std::string> _updateServiceZone();
    std::tuple<int, std::string> _updateFactorThreshold();

   public:
    ConsulResolver(
        const std::string& service,
        const std::string& myService,
        const std::string& address      = "http://127.0.0.1:8500",
        int                intervalS    = 10,
        double             serviceRatio = 0,
        double             cpuThreshold = 0.6);
    ConsulResolver() : ConsulResolver("", "") {}

    void SetLogger(log4cplus::Logger* logger) {
        this->logger = logger;
    }
    std::tuple<int, std::string> Start();
    std::tuple<int, std::string> Stop();

    std::shared_ptr<ServiceZone> GetLocalZone() {
        return this->localZone;
    }

    std::shared_ptr<ServiceZone> GetOtherZone() {
        return this->otherZone;
    }

    std::shared_ptr<ServiceNode> DiscoverNode();
};
}
