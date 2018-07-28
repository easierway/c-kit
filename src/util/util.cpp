#include "util/util.h"
#include <curl/curl.h>
#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
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

std::tuple<int, std::string> GetStatusOutput(const std::string &command) {
    std::array<char, 128> buffer;
    std::string           result;
    auto                  fp = popen(command.c_str(), "r");
    if (fp == nullptr) {
        return std::make_tuple(-1, "");
    }
    while (!feof(fp)) {
        if (fgets(buffer.data(), 128, fp) != nullptr) {
            result += buffer.data();
        }
    }
    return std::make_tuple(pclose(fp), result);
}

std::string Zone() {
    std::string output;
    int         status;
    std::tie(status, output) = GetStatusOutput("/opt/aws/bin/ec2-metadata -z");
    if (status != 0) {
        return "unknown";
    }

    std::vector<std::string> kv;
    auto                     str = output.substr(0, output.length() - 1);
    boost::split(kv, str, boost::is_any_of(" "));
    if (kv.size() != 2) {
        return "unknown";
    }

    return kv[1];
}

static size_t WriteToStream(void *ptr, size_t size, size_t nmemb, std::stringstream *stream) {
    stream->write((const char *)ptr, size * nmemb);
    return size * nmemb;
}

std::tuple<int, std::string, std::string> HttpGet(const std::string &url) {
    auto curl = curl_easy_init();
    if (!curl) {
        return std::make_tuple(-1, "", "curl_easy_init failed");
    }

    std::stringstream body;
    long              status;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToStream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    auto code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        curl_easy_cleanup(curl);
        std::stringstream ss;
        ss << "curl_easy_perform is not ok, code: [" << code << "] url: [" << url << "]";
        return std::make_tuple(-1, body.str(), ss.str());
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (status == 200) {
        return std::make_tuple(status, body.str(), "");
    }

    std::stringstream ss;
    ss << "curl_easy_perform is not ok, status: [" << status << "] url: [" << url << "]";
    return std::make_tuple(status, body.str(), ss.str());
}

}  // namespace kit
