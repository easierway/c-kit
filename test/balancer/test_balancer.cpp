#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(testBalancer, case1) {
    std::cout << "hello world" << std::endl;
}
