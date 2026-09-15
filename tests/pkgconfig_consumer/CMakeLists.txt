cmake_minimum_required(VERSION 3.18)
project(pkgconfig_consumer LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGIT REQUIRED IMPORTED_TARGET log-it-cpp)

add_executable(pkgconfig_consumer main.cpp)
target_compile_features(pkgconfig_consumer PRIVATE cxx_std_11)
target_link_libraries(pkgconfig_consumer PRIVATE PkgConfig::LOGIT)
