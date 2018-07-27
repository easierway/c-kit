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
        return json11::Json::object{
            {"nodes", ""},
            {"factors", this->factors},
            {"factorMax", this->factorMax},
        };
    }
};

class ConsulResolver {
    std::string                  address;
    std::string                  service;
    std::string                  myService;
    std::string                  zone;
    int                          factorThreshold;
    int                          myServiceNum;
    std::shared_ptr<ServiceZone> localZone;
    std::shared_ptr<ServiceZone> otherZone;
    int                          intervalS;
    bool                         done;
    int                          cpuPercentage;
    double                       ratio;
    std::mutex                   serviceUpdaterMutex;

    json11::Json to_json() const {
        return json11::Json::object{
            {"address", this->address},
            {"service", this->service},
            {"myService", this->myService},
            {"zone", this->zone},
            {"factorThreshold", this->factorThreshold},
            {"myServiceNum", this->myServiceNum},
            {"intervalS", this->intervalS},
            {"ratio", this->ratio},
            {"cpuPercentage", this->cpuPercentage},
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
