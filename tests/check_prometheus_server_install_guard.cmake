if(NOT DEFINED BUILD_DIR OR NOT DEFINED INSTALL_PREFIX)
    message(FATAL_ERROR "BUILD_DIR and INSTALL_PREFIX must be provided")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_stdout
    ERROR_VARIABLE _install_stderr
)

if(_install_result EQUAL 0)
    message(FATAL_ERROR
        "Prometheus server installation unexpectedly succeeded; the unsupported configuration was not blocked")
endif()

set(_install_output "${_install_stdout}\n${_install_stderr}")
if(NOT _install_output MATCHES "LOGIT_WITH_PROMETHEUS_SERVER")
    message(FATAL_ERROR
        "Prometheus server install failed without the expected LOGIT_WITH_PROMETHEUS_SERVER diagnostic:\n"
        "${_install_output}")
endif()

message(STATUS "Prometheus server install guard rejected the unsupported package configuration")
