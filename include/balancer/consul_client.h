#pragma once

#include <json11.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace kit {

struct ServiceNode {
    std::string host;
    std::string zone;
    std::string machine;
    int         port;
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
            {"machine", this->machine},
            {"balancerFactor", this->balanceFactor},
        };
    }
};

class ConsulClient {
    std::string address;

   public:
    explicit ConsulClient(const std::string& address) {
        this->address = address;
    }

    std::tuple<int, std::vector<std::shared_ptr<ServiceNode>>, std::string> GetService(const std::string& serviceName, int timeoutS, std::string& lastIndex);
    std::tuple<int, json11::Json, std::string>                              GetKV(const std::string& path, int timeoutS, std::string& lastIndex);
};
}
