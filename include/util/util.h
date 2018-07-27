#include <string>

namespace kit {

double                                    CPUUsage();
std::string                               Zone();
std::tuple<std::string, int>              GetStatusOutput(const std::string& command);
std::tuple<std::string, int, std::string> HttpGet(std::string url);
}
