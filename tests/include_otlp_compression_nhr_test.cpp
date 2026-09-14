#include <logit/loggers/otlp/OtlpCompression.hpp>

int main() {
    return static_cast<int>(logit::OtlpCompression::None) == 0 ? 0 : 1;
}
