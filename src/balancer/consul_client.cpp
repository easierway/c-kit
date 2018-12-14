#include "balancer/consul_client.h"
#include <iostream>
#include <json11.hpp>
#include <map>
#include <sstream>
#include "util/util.h"

namespace kit {

std::tuple<int,
           std::vector<std::shared_ptr<ServiceNode>>,
           std::string> ConsulClient::GetService(const std::string &serviceName, int timeoutS, std::string &lastIndex) {
    // @see https://www.consul.io/api/index.html#blocking-queries
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string body;
    int status = -1;
    std::string err;
    std::map<std::string, std::string> header;
    std::stringstream ss;
    ss << this->address << "/v1/health/service/" << serviceName << "?passing=true&wait=" << timeoutS << "s";
    std::tie(status, body, header, err) = HttpGet(ss.str(), std::map<std::string, std::string>{});
    if (status!=200) {
        return std::make_tuple(-1, nodes, "HttpGet failed. err [" + err + "]");
    }
    if (header.count("X-Consul-Index") > 0) {
        if (lastIndex==header["X-Consul-Index"]) {
            return std::make_tuple(0, nodes, "");
        }
        lastIndex = header["X-Consul-Index"];
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, nodes, "Json parse failed. err [" + err + "]");
    }

    for (const auto &service : jsonObj.array_items()) {
        std::string zone = "unknown";
        std::string instanceid = "unknown";
        int balanceFactor = 0;
        if (service["Service"].is_null()) {
            continue;
        }
        if (!service["Service"]["Meta"]["zone"].is_null()) {
            zone = service["Service"]["Meta"]["zone"].string_value();
        }
        if (!service["Service"]["Meta"]["instanceid"].is_null()) {
            instanceid = service["Service"]["Meta"]["instanceid"].string_value();
        }
        if (!service["Service"]["Meta"]["balanceFactor"].is_null()) {
            balanceFactor = service["Service"]["Meta"]["balanceFactor"].int_value();
        }

        auto node = std::make_shared<ServiceNode>();
        node->host = service["Service"]["Address"].string_value();
        node->port = service["Service"]["Port"].int_value();
        node->zone = zone;
        node->instanceid = instanceid;
        node->balanceFactor = balanceFactor;

        nodes.emplace_back(node);
    }

    return std::make_tuple(0, nodes, "");
}

std::tuple<int, json11::Json, std::string> ConsulClient::GetKV(const std::string &path,
                                                               int timeoutS,
                                                               std::string &lastIndex) {
    std::vector<std::shared_ptr<ServiceNode>> nodes;
    std::string body;
    int status = -1;
    std::string err;
    std::map<std::string, std::string> header;
    std::stringstream ss;
    ss << this->address << "/v1/kv/" << path << "?raw=true&wait=" << timeoutS << "s";
    std::tie(status, body, header, err) = HttpGet(ss.str(), std::map<std::string, std::string>{});
    if (status!=200) {
        return std::make_tuple(-1, json11::Json(), "HttpGet failed. err [" + err + "]");
    }
    if (header.count("X-Consul-Index") > 0) {
        if (lastIndex==header["X-Consul-Index"]) {
            return std::make_tuple(0, json11::Json(), "");
        }
        lastIndex = header["X-Consul-Index"];
    }
    auto jsonObj = json11::Json::parse(body, err);
    if (!err.empty()) {
        return std::make_tuple(-1, json11::Json(), "Json parse failed. err [" + err + "]");
    }
    return std::make_tuple(0, jsonObj, "");
}
}