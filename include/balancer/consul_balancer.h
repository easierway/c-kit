#include <iostream>
#include <sstream>
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
//    std::shared_ptr<ppconsul::catalog::Catalog> client;
    std::string                                 service;
    uint64_t                                    lastIndex;
    std::string                                 myService;
    uint64_t                                    myLastIndex;
    std::string                                 zone;
    int                                         factorThreshold;
    int                                         myServiceNum;
    std::shared_ptr<ServiceZone>                localZone;
    std::shared_ptr<ServiceZone>                otherZone;
    int                                         intervalS;
    bool                                        done;
    int                                         cpuPercentage;
    double                                      ratio;

    double _cpuUsage();
    void   _resolve();
    void   _calFactorThreshold();

   public:
    ConsulResolver(const std::string& address,
                   const std::string& service,
                   const std::string& myServices,
                   int intervalS, double ratio);

    std::shared_ptr<ServiceZone> GetLocalZone() {
        return this->localZone;
    }

    std::shared_ptr<ServiceZone> GetOtherZone() {
        return this->otherZone;
    }

    std::shared_ptr<ServiceNode> DiscoverNode();
};
}
