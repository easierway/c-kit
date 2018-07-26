#include <gtest/gtest.h>
#include <iostream>
#include "util/util.h"

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testUtil, caseCpuUsage) {
    std::cout << CPUUsage() << std::endl;
}

TEST(testUtil, caseZone) {
    std::cout << Zone() << std::endl;
}

}  // namespace kit