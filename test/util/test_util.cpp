#include <gtest/gtest.h>
#include <iostream>
#include "util/util.h"

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace kit {

TEST(testUtil, caseCpuUsage) {
    while (true) {
        sleep(1);
        std::cout << CPUUsage() << std::endl;
    }
    std::cout << CPUUsage() << std::endl;
}

}  // namespace kit