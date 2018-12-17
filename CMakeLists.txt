cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(C_KIT)

include(ExternalProject)

if(NOT LOG4CPLUS_ROOT)
#    ExternalProject_Add(
#        log4cplus_log4cplus
#        URL https://github.com/log4cplus/log4cplus/archive/REL_1_2_1.zip
#        PREFIX ${C_KIT_SOURCE_DIR}/third/log4cplus
#        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${C_KIT_SOURCE_DIR}/third/log4cplus/build -DBUILD_SHARED_LIBS=OFF
#    )
#    add_dependencies(third log4cplus_log4cplus)
    set(LOG4CPLUS_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/log4cplus")
endif()

if(NOT GTEST_ROOT)
#    ExternalProject_Add(
#        google_gtest
#        URL https://github.com/google/googletest/archive/release-1.8.0.zip
#        PREFIX ${C_KIT_SOURCE_DIR}/third/gtest
#        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${C_KIT_SOURCE_DIR}/third/gtest/build -DBUILD_SHARED_LIBS=OFF
#    )
#    add_dependencies(third google_gtest)
    set(GTEST_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/gtest")
endif()

if(NOT BOOST_ROOT)
#    ExternalProject_Add(
#        boostorg_boost
#        URL https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.gz
#        PREFIX ${C_KIT_SOURCE_DIR}/third/boost
#        BUILD_IN_SOURCE true
#        CONFIGURE_COMMAND ""
#        BUILD_COMMAND sh bootstrap.sh && ./b2 link=static -j8
#        INSTALL_COMMAND ./b2 install --prefix=${C_KIT_SOURCE_DIR}/third/boost/build
#    )
#    add_dependencies(third boostorg_boost)
    set(BOOST_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/boost")
endif()

if(NOT JSON11_ROOT)
#    ExternalProject_Add(
#        dropbox_json11
#        URL https://github.com/dropbox/json11/archive/master.zip
#        PREFIX ${C_KIT_SOURCE_DIR}/third/json11
#        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${C_KIT_SOURCE_DIR}/third/json11/build -DBUILD_SHARED_LIBS=OFF
#    )
#    add_dependencies(third dropbox_json11)
    set(JSON11_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/json11-master")
endif()

if(NOT OPENSSL_ROOT)
#    ExternalProject_Add(
#        openssl_openssl
#        URL https://github.com/openssl/openssl/archive/OpenSSL_1_0_2o.zip
#        PREFIX ${C_KIT_SOURCE_DIR}/third/openssl
#        BUILD_IN_SOURCE true
#        CONFIGURE_COMMAND ./config -fPIC --prefix=${C_KIT_SOURCE_DIR}/third/openssl/build
#    )
#    add_dependencies(third openssl_openssl)
    set(OPENSSL_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/openssl-OpenSSL_1_0_2o")
endif()

if(NOT CURL_ROOT)
#    ExternalProject_Add(
#        haxx_curl
#        URL https://curl.haxx.se/download/curl-7.61.0.tar.gz
#        PREFIX ${C_KIT_SOURCE_DIR}/third/curl
#        BUILD_IN_SOURCE true
#        CONFIGURE_COMMAND ./configure
#            --prefix=${C_KIT_SOURCE_DIR}/third/curl/build
#            --with-ssl=${OPENSSL_ROOT}
#            --disable-shared
#    )
#    add_dependencies(haxx_curl openssl_openssl)
#    add_dependencies(third haxx_curl)
    set(CURL_ROOT "${C_KIT_SOURCE_DIR}/../third-lib/curl-7.61.0")
endif()

set(CMAKE_CXX_FLAGS "-w -g -std=c++11 -lpthread")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

include_directories(
    "${C_KIT_SOURCE_DIR}/include"
    "${GTEST_ROOT}/include"
    "${BOOST_ROOT}/include"
    "${CURL_ROOT}/include"
    "${JSON11_ROOT}/include"
    "${LOG4CPLUS_ROOT}/include"
)

link_directories(
    "${GTEST_ROOT}/lib"
    "${BOOST_ROOT}/lib"
    "${CURL_ROOT}/lib"
    "${JSON11_ROOT}/lib"
    "${LOG4CPLUS_ROOT}/lib"
    "${C_KIT_SOURCE_DIR}/../third-lib/openssl-OpenSSL_1_0_2o/lib"
)

aux_source_directory(${C_KIT_SOURCE_DIR}/src/util util_source)
aux_source_directory(${C_KIT_SOURCE_DIR}/src/balancer balancer_source)
SET(C_KIT_SRC ${util_source} ${balancer_source})

# enable test
ENABLE_TESTING()

# library
add_library(ckit STATIC ${all_source})

install(TARGETS ckit DESTINATION lib)
install(DIRECTORY ${C_KIT_SOURCE_DIR}/include DESTINATION .)

ADD_SUBDIRECTORY(test)
