#pragma once

#include <json11.hpp>

struct ResolverMetric {
    int candidatePoolSize;
    int crossZoneNum;
    int selectNum;

    ResolverMetric() {
        candidatePoolSize = 0;
        crossZoneNum = 0;
        selectNum = 0;
    }

    json11::Json to_json() const {
        return json11::Json::object{
            {"candidatePoolSize", candidatePoolSize},
            {"crossZoneNum", crossZoneNum},
            {"selectNum", selectNum},
        };
    }
};
