#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <json11.hpp>
#include <log4cplus/logger.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include "consul_client.h"

namespace kit {

struct ServiceZone {
    std::string                               zone;
    int                                       cpu;
    int                                       zoneFactor;
    int                                       zoneWeight;
    int                                       idleZoneFactor;
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::vector<int>                          factors;
    std::vector<int>                          weights;
    int                                       factorSum;

    json11::Json to_json() const {
        std::vector<ServiceNode> nodes(this->nodes.size());
        for (int i = 0; i < this->nodes.size(); i++) {
            nodes[i] = *(this->nodes[i]);
        }
        return json11::Json::object{
            {"zone", this->zone},
            {"cpu", this->cpu},
            {"zoneWeight", this->zoneWeight},
            {"zoneFactor", this->zoneFactor},
            {"idleZoneFactor", this->idleZoneFactor},
            {"nodes", nodes},
            {"factors", this->factors},
            {"factorSum", this->factorSum},
        };
    }
};

class ConsulResolver {
    ConsulClient                                               client;
    std::string                                                address;              // consul 地址，一般为本地 agent
    std::string                                                service;              // 要访问的服务名
    std::string                                                lastIndex;            // blocking 访问 consul
    std::string                                                zone;                 // 服务地区
    int                                                        cpuThreshold;         // cpu 阀值，根据 qps 预测要访问的服务 cpu，超过阀值，跨 zone 访问，[0,1]
    std::shared_ptr<std::vector<std::shared_ptr<ServiceZone>>> serviceZones;         // 所有 zone 的服务节点
    std::shared_ptr<ServiceZone>                               localZone;            // 本地 zone
    int                                                        zoneFactorSum;        // 权重和
    std::unordered_map<std::string, int>                       zoneCPUMap;           // 各个 zone 负载情况，从 consul 中获取
    std::unordered_map<std::string, int>                       machineFactorMap;     // 各个机型的权重，从 consul 中获取
    std::string                                                cpuThresholdKey;      // cpu 阀值，超过阀值跨 zone 访问，从 consul 中获取
    std::string                                                zoneCPUKey;           // cpu 阀值在 consul 中的 key
    std::string                                                machineFactorKey;     // 机器权重在 consul 中的 key
    int                                                        intervalS;            // 服务列表更新最小间隔秒数
    int                                                        timeoutS;             // 访问 consul 超时时间
    bool                                                       done;                 // 退出标记
    boost::shared_mutex                                        serviceUpdaterMutex;  // 服务更新锁
    std::mutex                                                 discoverMutex;        // 阻塞调用 DiscoverNode
    log4cplus::Logger*                                         logger;               // 日志

    json11::Json to_json() const {
        return json11::Json::object{
            {"address", this->address},
            {"service", this->service},
            {"zone", this->zone},
            {"intervalS", this->intervalS},
            {"timeoutS", this->timeoutS},
            {"cpuThreshold", this->cpuThreshold},
        };
    }

    std::thread* serviceUpdater;

    std::tuple<int, std::string> _updateServiceZone();
    std::tuple<int, std::string> _updateZoneCPUMap();
    std::tuple<int, std::string> _updateMachineFactorMap();
    std::tuple<int, std::string> _updateCPUThreshold();
    std::tuple<int, std::string> _updateAll();

   public:
    ConsulResolver(
        const std::string& address,
        const std::string& service,
        const std::string& cpuThresholdKey  = "as/rs/cpu_threshold.json",
        const std::string& zoneCPUKey       = "as/rs/zone_cpu.json",
        const std::string& machineFactorKey = "as/rs/machine_factor.json",
        int                intervalS        = 60,
        int                timeoutS         = 1);

    void SetLogger(log4cplus::Logger* logger) {
        this->logger = logger;
    }
    std::tuple<int, std::string> Start();
    std::tuple<int, std::string> Stop();

    std::shared_ptr<ServiceNode> DiscoverNode();
};
}
