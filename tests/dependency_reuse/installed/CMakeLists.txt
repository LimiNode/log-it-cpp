cmake_minimum_required(VERSION 3.18)
project(installed_dependency_reuse LANGUAGES CXX)

if(NOT DEFINED LOGIT_SOURCE_DIR)
    message(FATAL_ERROR "LOGIT_SOURCE_DIR must point to the LogIt++ checkout")
endif()
if(NOT CMAKE_DISABLE_FIND_PACKAGE_TimeShield OR
   NOT CMAKE_DISABLE_FIND_PACKAGE_fmt)
    message(FATAL_ERROR "The test must disable TimeShield and fmt package discovery")
endif()

# Both targets model dependencies supplied by the parent project. Discovery is
# disabled by the invoking test so the installed package must reuse these
# targets instead of calling find_dependency().
add_library(time_shield::time_shield INTERFACE IMPORTED GLOBAL)
set_target_properties(time_shield::time_shield PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LOGIT_SOURCE_DIR}/external/time-shield-cpp/include"
)

add_library(fmt::fmt INTERFACE IMPORTED GLOBAL)
set_target_properties(fmt::fmt PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LOGIT_SOURCE_DIR}/external/fmt/include"
)

find_package(log-it-cpp CONFIG REQUIRED)

add_executable(installed_dependency_reuse main.cpp)
target_link_libraries(installed_dependency_reuse PRIVATE log-it-cpp::log-it-cpp)
if(MSVC)
    target_compile_options(installed_dependency_reuse PRIVATE /utf-8)
endif()
