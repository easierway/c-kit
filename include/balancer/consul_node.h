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
    double balanceFactor;
    int port;
    double workload;

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
            {"balanceFactor", this->balanceFactor},
            {"workload", this->workload},
        };
    }
};

struct ServiceZone {
    std::string zone;
    double workload;
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
    std::vector<double> factors;
    std::vector<double> weights;
    double factorSum;

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
