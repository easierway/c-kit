#pragma once

#include <json11.hpp>

namespace kit {

struct OnlineLab {
    double learningRate;
    double rateThreshold;
    double crossZoneRate;

    json11::Json to_json() const {
        return json11::Json::object{
            {"learningRate", this->learningRate},
            {"rateThreshold", this->rateThreshold},
            {"crossZoneRate", this->crossZoneRate},
        };
    }
};

}
