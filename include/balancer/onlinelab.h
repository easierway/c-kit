#pragma once

#include <json11.hpp>

namespace kit {

struct OnlineLab {
    double learningRate;
    double rateThreshold;

    json11::Json to_json() const {
        return json11::Json::object{
            {"learningRate", this->learningRate},
            {"rateThreshold", this->rateThreshold},
        };
    }
};

}
