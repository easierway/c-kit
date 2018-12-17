#pragma once

#include <json11.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "consul_node.h"

namespace kit {

class ConsulClient {
    std::string address;

public:
    explicit ConsulClient(const std::string &address) {
        this->address = address;
    }

    // TODO: making lastIndex working, for now useless
    std::tuple<int, std::vector<std::shared_ptr<ServiceNode>>, std::string> GetService(const std::string &serviceName,
                                                                                       int timeoutS,
                                                                                       std::string &lastIndex);
    std::tuple<int, json11::Json, std::string> GetKV(const std::string &path, int timeoutS, std::string &lastIndex);
};
}
