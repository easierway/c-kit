#include <iostream>
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
};

struct ServiceZone {
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::vector<int>                          factors;
    int                                       factorMax;
};

class ConsulResolver {
    std::string                  address;
    std::string                  service;
    uint64_t                     lastIndex;
    std::string                  myService;
    uint64_t                     myLastIndex;
    std::string                  zone;
    int                          factorThreshold;
    int                          myServiceNum;
    std::shared_ptr<ServiceZone> localZone;
    std::shared_ptr<ServiceZone> otherZone;
    int                          intervalS;
    bool                         done;
    int                          cpuPercentage;
    double                       ratio;

    std::thread* serviceUpdater;
    std::thread* cpuUpdater;
    std::thread* factorUpdater;

    double                       _cpuUsage();
    std::tuple<int, std::string> _resolve();
    std::tuple<int, std::string> _calFactorThreshold();

   public:
    ConsulResolver(const std::string& address,
                   const std::string& service,
                   const std::string& myService,
                   int intervalS, double ratio);

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
