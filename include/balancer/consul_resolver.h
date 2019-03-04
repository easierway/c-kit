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
#include "onlinelab.h"
#include "resolver_metic.h"

namespace kit {

class ConsulResolver {
    ConsulClient                                               client;
    std::string                                                address;              // consul 地址，一般为本地 agent
    std::string                                                service;              // 要访问的服务名
    std::string                                                lastIndex;            // blocking 访问 consul
    std::string                                                zone;                 // 服务地区

    std::shared_ptr<CandidatePool>                             candidatePool;        // candidate nodes
    std::shared_ptr<ServiceZone>                               localZone;            // 本地 zone
    std::shared_ptr<std::vector<std::shared_ptr<ServiceZone>>> serviceZones;         // 所有 zone 的服务节点

    std::unordered_map<std::string, double>                    zoneCPUMap;           // 各个 zone 负载情况，从 consul 中获取
    std::unordered_map<std::string, double>                    instanceFactorMap;    // 各个机型的权重，从 consul 中获取
    std::unordered_map<std::string, double>                    balanceFactorCache;   // 内存中调整后的factor缓存
    double                                                     cpuThreshold;         // cpu 阀值，根据 qps 预测要访问的服务 cpu，超过阀值，跨 zone 访问，[0,1]
    OnlineLab                                                  onlinelab;


    std::string                                                cpuThresholdKey;      // cpu 阀值，超过阀值跨 zone 访问，从 consul 中获取
    std::string                                                instanceFactorKey;    // 机器权重在 consul 中的 key
    std::string                                                onlinelabFactorKey;   // tuning factor
    std::string                                                zoneCPUKey;           // cpu 阀值在 consul 中的 key

    std::shared_ptr<ResolverMetric>                            metric;               // metric of resolver
    bool                                                       zoneCPUUpdated;       // zone cpu updated
    int                                                        timeoutS;             // 访问 consul 超时时间
    boost::shared_mutex                                        serviceUpdaterMutex;  // 服务更新锁
    std::mutex                                                 discoverMutex;        // 阻塞调用 DiscoverNode
    log4cplus::Logger*                                         logger;               // 日志

   public:
    ConsulResolver(
        const std::string& address,
        const std::string& service,
        const std::string& cpuThresholdKey    = "clb/rs/cpu_threshold.json",
        const std::string& zoneCPUKey         = "clb/rs/zone_cpu.json",
        const std::string& instanceFactorKey  = "clb/rs/instance_factor.json",
        const std::string& onlinelabFactorKey = "clb/rs/onlinelab_factor.json",
        int                timeoutS           = 1);

    json11::Json to_json() const {
        return json11::Json::object{
            {"address", this->address},
            {"service", this->service},
            {"zone", this->zone},
            {"cpuThreshold", this->cpuThreshold},
            {"zoneCPUMap", this->zoneCPUMap},
            {"onlinelab", this->onlinelab},
        };
    }

    // consul update
    std::tuple<int, std::string> updateCPUThreshold();
    std::tuple<int, std::string> updateZoneCPUMap();
    std::tuple<int, std::string> updateInstanceFactorMap();
    std::tuple<int, std::string> updateOnlinelabFactor();
    std::tuple<int, std::string> updateServiceZone();
    std::tuple<int, std::string> updateCandidatePool();
    std::tuple<int, std::string> updateAll();

    // clean factor cache
    std::tuple<int, std::string> expireBalanceFactorCache();

    // selection
    std::shared_ptr<ServiceNode> SelectedNode();

    // logger
    void SetLogger(log4cplus::Logger* logger) {
        this->logger = logger;
    }

    void SetZone(const std::string &zone){
        this->zone = zone;
    }
};

}
