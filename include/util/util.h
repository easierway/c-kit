#pragma once

#include <map>
#include <string>

namespace kit {

double      CPUUsage();
std::string Zone();

// return status, output
std::tuple<int, std::string> GetStatusOutput(const std::string& command);

// return status, body, error
std::tuple<int, std::string, std::string> HttpGet(const std::string& url);

// return status, body, headers, error
std::tuple<int, std::string, std::map<std::string, std::string>, std::string> HttpGet(const std::string& url, std::map<std::string, std::string> reqheader);

}
