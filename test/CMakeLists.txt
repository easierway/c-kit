# libs
SET(TEST_NEEDED_LIBS ckit gtest curl ssl crypto z dl json11 boost_system boost_thread log4cplus)

add_executable(test_util util/test_util.cpp)
target_link_libraries(test_util ${TEST_NEEDED_LIBS} )
add_test(test_util test_util)

add_executable(test_consul_client balancer/test_consul_client.cpp)
target_link_libraries(test_consul_client ${TEST_NEEDED_LIBS})
add_test(test_consul_client test_consul_client)

add_executable(test_consul_balancer balancer/test_consul_balancer.cpp)
target_link_libraries(test_consul_balancer ${TEST_NEEDED_LIBS})
add_test(test_consul_balancer test_consul_balancer)

#if(UNITTEST)
#    add_executable(test_util ${util_source} test/util/test_util.cpp)
#    target_link_libraries(test_util gtest curl ssl crypto z dl)

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

#    enable_testing()
#    add_test(NAME test_util COMMAND test_util)
#endif()
