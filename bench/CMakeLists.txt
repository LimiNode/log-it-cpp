set(LOGIT_BENCH_SOURCES
    logit_bench.cpp
    adapters/LogItAdapter.cpp
)

if(LOGIT_BENCH_WITH_SPDLOG)
    list(APPEND LOGIT_BENCH_SOURCES adapters/SpdlogAdapter.cpp)
endif()

add_executable(logit_bench ${LOGIT_BENCH_SOURCES})

target_include_directories(logit_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(logit_bench PRIVATE cxx_std_17)

set_target_properties(logit_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(logit_bench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_BINARY_DIR}
    )
endforeach()

target_link_libraries(logit_bench PRIVATE log-it-cpp::log-it-cpp)

add_executable(logit_public_macro_bench public_macro_bench.cpp)
target_compile_features(logit_public_macro_bench PRIVATE cxx_std_17)
target_link_libraries(logit_public_macro_bench PRIVATE log-it-cpp::log-it-cpp)
set_target_properties(logit_public_macro_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(logit_hotpath_bench logger_hotpath_bench.cpp)
target_compile_features(logit_hotpath_bench PRIVATE cxx_std_17)
target_link_libraries(logit_hotpath_bench PRIVATE log-it-cpp::log-it-cpp)
set_target_properties(logit_hotpath_bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(logit_hotpath_bench_legacy logger_hotpath_bench.cpp)
target_compile_features(logit_hotpath_bench_legacy PRIVATE cxx_std_17)
target_compile_definitions(logit_hotpath_bench_legacy PRIVATE LOGIT_BENCH_LEGACY_REGISTRY=1)
target_link_libraries(logit_hotpath_bench_legacy PRIVATE log-it-cpp::log-it-cpp)
set_target_properties(logit_hotpath_bench_legacy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(benchmark_validation_test benchmark_validation_test.cpp)
target_compile_features(benchmark_validation_test PRIVATE cxx_std_17)
set_target_properties(benchmark_validation_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME benchmark_validation_test COMMAND benchmark_validation_test)

if(LOGIT_BENCH_WITH_SPDLOG)
    target_compile_definitions(logit_bench PRIVATE LOGIT_BENCH_HAVE_SPDLOG=1)
    if(NOT TARGET spdlog::spdlog)
        include(FetchContent)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.17.0
        )
        FetchContent_MakeAvailable(spdlog)
    endif()
    target_link_libraries(logit_bench PRIVATE spdlog::spdlog)

    add_executable(logit_bench_flush_test
        spdlog_flush_test.cpp
        adapters/SpdlogAdapter.cpp
    )
    target_compile_features(logit_bench_flush_test PRIVATE cxx_std_17)
    target_include_directories(logit_bench_flush_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(logit_bench_flush_test PRIVATE LOGIT_BENCH_HAVE_SPDLOG=1)
    target_link_libraries(logit_bench_flush_test PRIVATE spdlog::spdlog)
    set_target_properties(logit_bench_flush_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(logit_bench_flush_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_BINARY_DIR}
        )
    endforeach()
    add_test(NAME logit_bench_flush_test COMMAND logit_bench_flush_test)
endif()
