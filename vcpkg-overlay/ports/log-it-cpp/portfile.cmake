vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LimiNode/log-it-cpp
    REF v1.0.2
    SHA512 9c193d103a1f059b5adb934d62ce01e3c7df1dda4d3005dafdee603ddeec2550ecc496ebe03a33e238c1136fde9dda278abeb689cbfa4362611294cac77f33d6
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    MAYBE_UNUSED_VARIABLES LOGIT_WITH_SYSLOG LOGIT_WITH_WIN_EVENT_LOG
    OPTIONS
      -DLOGIT_CPP_BUILD_TESTS=OFF
      -DLOGIT_WITH_SYSLOG=ON
      -DLOGIT_WITH_WIN_EVENT_LOG=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME log-it-cpp CONFIG_PATH lib/cmake/log-it-cpp)

# vcpkg's isolated fixup check does not include the dependency port's
# pkg-config directory, even though time-shield is declared in Requires.
# The relocatable metadata is still rewritten; the dependency is validated by
# the dedicated Linux pkg-config consumer job.
vcpkg_fixup_pkgconfig(SKIP_CHECK)

# Agent guidance belongs to the source repository, not to the installed package.
file(GLOB_RECURSE _logit_agent_files
    "${CURRENT_PACKAGES_DIR}/include/AGENTS.md"
    "${CURRENT_PACKAGES_DIR}/include/*/AGENTS.md"
)
if(_logit_agent_files)
    file(REMOVE ${_logit_agent_files})
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug"
    "${CURRENT_PACKAGES_DIR}/lib"
)
