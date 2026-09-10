cmake_minimum_required(VERSION 3.18)
project(parent_provided_timeshield LANGUAGES CXX)

if(NOT DEFINED LOGIT_SOURCE_DIR)
    message(FATAL_ERROR "LOGIT_SOURCE_DIR must point to the LogIt++ checkout")
endif()
if(NOT CMAKE_DISABLE_FIND_PACKAGE_TimeShield)
    message(FATAL_ERROR "The test must disable TimeShield package discovery")
endif()

# Model a parent project that has already provided TimeShield. The test invokes
# CMake with CMAKE_DISABLE_FIND_PACKAGE_TimeShield=TRUE so a regression that
# probes the package before checking the target cannot fall back silently.
add_library(time_shield::time_shield INTERFACE IMPORTED GLOBAL)

add_subdirectory("${LOGIT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/log-it-cpp")

if(NOT TARGET log-it-cpp::log-it-cpp)
    message(FATAL_ERROR "LogIt++ target was not created")
endif()
