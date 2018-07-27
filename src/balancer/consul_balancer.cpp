#include "balancer/consul_balancer.h"
#include "util/util.h"

namespace kit {

ConsulResolver::ConsulResolver(const std::string& address,
                               const std::string& service,
                               const std::string& myServices,
                               int intervalS, double ratio) {
//    ppconsul::Consul consul(address);
//    this->client = std::make_shared<ppconsul::catalog::Catalog>(consul);
//    if (!this->client) {
//        std::cout << "failed." << std::endl;
//    }
    this->service = service;
    this->myService = myService;
    this->intervalS = intervalS;
    this->zone = Zone();
    this->done = false;
    this->cpuPercentage = CPUUsage();
    this->ratio = ratio;
}

void ConsulResolver::_resolve() {
//    services, metainfo, err := r.client.Health().Service(r.myService, "", true, &api.QueryOptions{
//    WaitIndex: r.myLastIndex,
//    })
//    auto nodes = this->client->service(ppconsul::withHeaders, this->service);
//    for (const auto& node : nodes) {
//        std::cout << node.second.address << ":" << node.second.port << std::endl;
//    }
}

void ConsulResolver::_calFactorThreshold() {
}

std::shared_ptr<ServiceNode> ConsulResolver::DiscoverNode() {
    _resolve();
}
}
