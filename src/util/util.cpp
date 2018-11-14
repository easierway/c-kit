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
#include <vector>

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
    boost::split(kv, str, [](char ch) { return ch == ' '; });
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

std::tuple<int, std::string, std::map<std::string, std::string>, std::string> HttpGet(const std::string &url, std::map<std::string, std::string> reqheader) {
    auto curl = curl_easy_init();
    if (!curl) {
        return std::make_tuple(-1, "", std::map<std::string, std::string>{}, "curl_easy_init failed");
    }

    std::stringstream  resheaderStr;
    std::stringstream  body;
    long               status;
    struct curl_slist *reqheaderStr = nullptr;
    for (const auto &kv : reqheader) {
        reqheaderStr = curl_slist_append(reqheaderStr, (kv.first + ":" + kv.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, reqheaderStr);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteToStream);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resheaderStr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToStream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    auto code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        curl_slist_free_all(reqheaderStr);
        curl_easy_cleanup(curl);
        std::stringstream ss;
        ss << "curl_easy_perform is not ok, code: [" << code << "] url: [" << url << "]";
        return std::make_tuple(-1, body.str(), std::map<std::string, std::string>{}, ss.str());
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    std::map<std::string, std::string> resheader;

    std::vector<std::string> lines;
    auto                     str = resheaderStr.str();
    boost::split(lines, str, [](char ch) { return ch == '\n'; });
    for (const auto &line : lines) {
        auto idx = line.find(":");
        if (idx != std::string::npos) {
            resheader[line.substr(0, idx)] = boost::trim_copy(line.substr(idx + 1));
        }
    }

    curl_slist_free_all(reqheaderStr);
    curl_easy_cleanup(curl);

    if (status == 200) {
        return std::make_tuple(status, body.str(), resheader, "");
    }

    std::stringstream ss;
    ss << "curl_easy_perform is not ok, status: [" << status << "] url: [" << url << "]";
    return std::make_tuple(status, body.str(), resheader, ss.str());
}
}
