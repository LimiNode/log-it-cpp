#include <logit/loggers/otlp/OtlpCompressionType.hpp>

int main() {
    return static_cast<int>(logit::OtlpCompression::Zstd) == 2 ? 0 : 1;
}
