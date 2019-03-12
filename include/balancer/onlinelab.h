#pragma once

#include <json11.hpp>

namespace kit {

struct OnlineLab {
    bool crossZone;
    double learningRate;
    double rateThreshold;
    double crossZoneRate;
    double factorCacheExpire;

    json11::Json to_json() const {
        return json11::Json::object{
            {"crossZone", this->crossZone},
            {"crossZoneRate", this->crossZoneRate},
            {"factorCacheExpire", this->factorCacheExpire},
            {"learningRate", this->learningRate},
            {"rateThreshold", this->rateThreshold},
        };
    }
};

}
