#pragma once

#include <json11.hpp>

namespace kit {

struct OnlineLab {
    bool crossZone;
    // TODO: delete future
    double crossZoneRate;
    double factorCacheExpire;
    // rate for the factor when starting
    double factorStartRate;
    double learningRate;
    double rateThreshold;

    json11::Json to_json() const {
        return json11::Json::object{
            {"crossZone", this->crossZone},
            {"crossZoneRate", this->crossZoneRate},
            {"factorCacheExpire", this->factorCacheExpire},
            {"factorStartRate", this->factorStartRate},
            {"learningRate", this->learningRate},
            {"rateThreshold", this->rateThreshold},
        };
    }
};

}
