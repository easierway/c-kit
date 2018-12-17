enable_testing()

add_executable(test_util ${util_source} util/test_util.cpp)
target_link_libraries(test_util gtest curl ssl crypto z dl)

    #add_executable(test_balancer ${balancer_source} ${util_source} test/balancer/test_balancer.cpp)
    #add_dependencies(test_balancer third)
    #if (APPLE)
    #    target_link_libraries(test_balancer gtest curl json11 boost_system boost_thread log4cplus)
    #else()
    #    target_link_libraries(test_balancer gtest curl json11 boost_system boost_thread log4cplusS)
    #endif()
    #add_custom_command(
    #    TARGET test_balancer
    #    POST_BUILD
    #    COMMAND cp ${C_KIT_SOURCE_DIR}/configs/log.ini ${C_KIT_BINARY_DIR}
    #)


add_test(NAME test_util COMMAND test_util)

