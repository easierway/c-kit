# libs
SET(TEST_NEEDED_LIBS ckit gtest curl ssl crypto z dl json11 boost_system boost_thread log4cplus)

add_executable(test_util util/test_util.cpp)
target_link_libraries(test_util ${TEST_NEEDED_LIBS} )
add_test(test_util test_util)

add_executable(test_consul_client balancer/test_consul_client.cpp)
target_link_libraries(test_consul_client ${TEST_NEEDED_LIBS})
add_test(test_consul_client test_consul_client)

add_executable(test_consul_resolver balancer/test_consul_resolver.cpp)
target_link_libraries(test_consul_resolver ${TEST_NEEDED_LIBS})
add_test(test_consul_resolver test_consul_resolver)

add_executable(test_balancer balancer/test_balancer.cpp)
target_link_libraries(test_balancer ${TEST_NEEDED_LIBS})
add_test(test_balancer test_balancer)

# apps
add_executable(balancer app/balancer.cpp)
target_link_libraries(balancer ${TEST_NEEDED_LIBS})

