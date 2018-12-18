#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "json11.hpp"

namespace kit {

struct ServiceNode {
    std::string host;
    std::string instanceID;
    std::string publicIP;
    std::string zone;
    int balanceFactor;
    int port;
    int workload;

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
            {"instanceid", this->instanceID},
            {"publicIP", this->publicIP},
            {"balancerFactor", this->balanceFactor},
            {"workload", this->workload},
        };
    }
};

struct ServiceZone {
    std::string zone;
    int workload;
    std::vector<std::shared_ptr<ServiceNode>> nodes;

    json11::Json to_json() const {
        std::vector<ServiceNode> nodes(this->nodes.size());
        for (int i = 0; i < this->nodes.size(); i++) {
            nodes[i] = *(this->nodes[i]);
        }
        return json11::Json::object{
            {"zone", this->zone},
            {"workload", this->workload},
            {"nodes", nodes},
        };
    }
};

struct CandidatePool {
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::vector<int32_t> factors;
    std::vector<int32_t> weights;
    int32_t factorSum;

    json11::Json to_json() const {
        std::vector<ServiceNode> nodes(this->nodes.size());
        for (int i = 0; i < this->nodes.size(); i++) {
            nodes[i] = *(this->nodes[i]);
        }
        return json11::Json::object{
            {"nodes", nodes},
            {"factors", this->factors},
            {"factorSum", this->factorSum},
            {"weights", this->weights},
        };
    }
};

}
