# Example Instructions

Examples are compile-time and usage documentation for consumers.

- Include only installed/public headers and use the umbrella headers where
  practical. Do not reach into `detail/` from an example.
- Keep examples portable across the default C++11 build. Guard MDBX, OTLP,
  compression, and platform-specific code with the same CMake feature options
  as the library.
- Examples must be safe to run repeatedly: use temporary output paths and do
  not assume a writable system directory or external service.
- Keep output concise and explain prerequisites in the adjacent README when a
  dependency or service is required.
