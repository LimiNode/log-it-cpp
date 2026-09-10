\page installation Installation guide

# Installation guide

## Requirements

LogIt++ requires CMake 3.18 or newer and **TimeShield 1.0.6 or newer**. The
core library and most built-in backends use C++11. OTLP, the Prometheus HTTP
server, and MDBX integrations require C++17.

## Vendored checkout

Use `add_subdirectory()` when LogIt++ is part of the source tree:

```cmake
add_subdirectory(external/log-it-cpp)
target_link_libraries(my_app PRIVATE log-it-cpp::log-it-cpp)
```

The directory name is arbitrary. If dependencies are supplied as sibling
targets, define them before adding LogIt++.

## Git submodule

```bash
git submodule add https://github.com/LimiNode/log-it-cpp.git external/log-it-cpp
git submodule update --init --recursive
```

Then use the vendored CMake flow above. The bundled TimeShield submodule is a
development fallback; package-manager builds may provide an installed
`time_shield::time_shield` target instead.

## Installed package

Build and install the package from a clean checkout:

```bash
cmake -S . -B build -DLOGIT_CPP_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix ./install
```

Consume it from another CMake project:

```cmake
find_package(log-it-cpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE log-it-cpp::log-it-cpp)
```

Pass `-DCMAKE_PREFIX_PATH=/path/to/install` when configuring the consumer.

## Optional dependencies and features

Enable only the features needed by the application. `fmt`, zlib, and zstd can
be provided as installed CMake packages or by bundled submodules with
`LOGIT_USE_SUBMODULES=ON`. `LOGIT_WITH_OTLP=ON` currently requires kurlyk for
both OTLP logger variants because the option controls the shared OTLP build.
MDBX requires `mdbx-containers`. Prometheus payload support has no extra
dependency; the Prometheus HTTP server uses Simple-Web-Server and Asio.

See the [`Backend matrix`](backends.html) for standards, dependencies, and
platform restrictions. All CMake options are listed in the API reference.

## Install limitations

Installed exports require optional dependencies to be available as installed
or imported targets. Bundled optional dependencies are intended for source and
build-tree development. `LOGIT_WITH_PROMETHEUS_SERVER=ON` is currently
rejected by `cmake --install` because its HTTP dependency tree is not exported
as part of the package.

## Package managers

The exported target is suitable for package-manager recipes that provide the
required dependency targets before configuring LogIt++. For the repository's
current vcpkg integration, follow the port and consumer checks in the CI
workflow. Treat package-manager manifests as versioned integration metadata and
verify them against the exact release tag being packaged.
