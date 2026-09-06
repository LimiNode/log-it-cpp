cmake_minimum_required(VERSION 3.18)
project(install_consumer LANGUAGES CXX)

option(LOGIT_CONSUMER_REQUIRE_FMT "Exercise the installed fmt-enabled package" OFF)

find_package(log-it-cpp CONFIG REQUIRED)

add_executable(install_consumer main.cpp)
target_link_libraries(install_consumer PRIVATE log-it-cpp::log-it-cpp)
if(LOGIT_CONSUMER_REQUIRE_FMT)
    target_compile_definitions(install_consumer PRIVATE LOGIT_CONSUMER_REQUIRE_FMT=1)
endif()
