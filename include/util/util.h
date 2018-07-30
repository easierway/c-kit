#pragma once

#include <string>

namespace kit {

double      CPUUsage();
std::string Zone();

// return status, output
std::tuple<int, std::string> GetStatusOutput(const std::string& command);

// return status, body, error
std::tuple<int, std::string, std::string> HttpGet(const std::string& url);
}
