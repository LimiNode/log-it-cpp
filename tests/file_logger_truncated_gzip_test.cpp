#include <logit.hpp>

#if defined(LOGIT_HAS_ZLIB)

#include <zlib.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string make_unique_directory_name() {
    const long long stamp = static_cast<long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return "truncated_gzip_logs_" + std::to_string(stamp);
}

std::string make_rotated_path(const std::string& current_path) {
    std::string rotated = current_path;
    const size_t pos = rotated.rfind(".log");
    if (pos != std::string::npos) {
        rotated.insert(pos, ".001");
    }
    return rotated + ".gz";
}

bool write_gzip_file(const std::string& path, const std::string& content) {
    gzFile file = gzopen(path.c_str(), "wb");
    if (!file) return false;

    const int written = gzwrite(
        file,
        content.data(),
        static_cast<unsigned int>(content.size()));
    const int close_result = gzclose(file);
    return written == static_cast<int>(content.size()) && close_result == Z_OK;
}

bool read_binary_file(const std::string& path, std::string& content) {
    std::ifstream file(path.c_str(), std::ios_base::binary);
    if (!file.is_open()) return false;

    std::ostringstream stream;
    stream << file.rdbuf();
    content = stream.str();
    return !file.bad();
}

bool write_binary_file(const std::string& path, const std::string& content) {
    std::ofstream file(path.c_str(), std::ios_base::binary | std::ios_base::trunc);
    if (!file.is_open()) return false;

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

} // namespace

int main() {
    const std::string directory = make_unique_directory_name();
    const std::string payload = "truncated gzip payload\n";
    std::string current_path;
    std::string compressed_path;
    int result_code = 1;

    {
        logit::FileLogger::Config config;
        config.directory = directory;
        config.async = false;

        logit::FileLogger logger(config);
        current_path = logger.get_string_param(logit::LoggerParam::LastFilePath);
        compressed_path = make_rotated_path(current_path);

        if (!write_gzip_file(compressed_path, payload)) {
            logger.shutdown();
        } else {
            const logit::LogFileReadResult valid =
                logger.read_log_file(compressed_path);

            std::string compressed_bytes;
            if (valid.ok && valid.content == payload &&
                read_binary_file(compressed_path, compressed_bytes) &&
                compressed_bytes.size() > 8) {
                compressed_bytes.resize(compressed_bytes.size() - 8);
                if (write_binary_file(compressed_path, compressed_bytes)) {
                    const logit::LogFileReadResult truncated =
                        logger.read_log_file(compressed_path);
                    result_code =
                        !truncated.ok && truncated.content.empty() ? 0 : 1;
                }
            }
            logger.shutdown();
        }
    }

    std::remove(compressed_path.c_str());
    std::remove(current_path.c_str());
    return result_code;
}

#else

int main() { return 0; }

#endif
