#include "util/util.h"
#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace kit {

double CPUUsage() {
    static uint64_t lastUser, lastUserLow, lastSys, lastIdle;

    double   percent;
    uint64_t user, userLow, sys, idle, total;

    auto fp = fopen("/proc/stat", "r");
    if (fp == nullptr) {
        return -1.0;
    }
    fscanf(fp, "cpu %llu %llu %llu %llu", &user, &userLow, &sys, &idle);
    fclose(fp);

    if (user < lastUser || userLow < lastUserLow ||
        sys < lastSys || idle < lastIdle) {
        percent = -1.0;
    } else {
        total   = (user - lastUser) + (userLow - lastUserLow) + (sys - lastSys);
        percent = total;
        total += (idle - lastIdle);
        percent /= total;
        percent *= 100;
    }

    lastUser    = user;
    lastUserLow = userLow;
    lastSys     = sys;
    lastIdle    = idle;

    return percent;
}

std::tuple<std::string, int> GetStatusOutput(const std::string& command) {
    std::array<char, 128> buffer;
    std::string           result;
    auto                  fp = popen(command.c_str(), "r");
    if (fp == nullptr) {
        return std::make_tuple<>("", -1);
    }
    while (!feof(fp)) {
        if (fgets(buffer.data(), 128, fp) != nullptr) {
            result += buffer.data();
        }
    }
    return std::make_tuple<>(result, pclose(fp));
}

std::string Zone() {
    std::string output;
    int         status;
    std::tie(output, status) = GetStatusOutput("/opt/aws/bin/ec2-metadata -z");
    if (status != 0) {
        return "unknown";
    }

    std::vector<std::string> kv;
    boost::split(kv, output, boost::is_any_of(" "));
    if (kv.size() != 2) {
        return "unknown";
    }

    return kv[1];
}

}  // namespace kit
