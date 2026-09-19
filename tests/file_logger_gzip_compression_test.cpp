#include <logit.hpp>
#if defined(LOGIT_HAS_ZLIB)
#include <zlib.h>
#include <string>
#include <cstdlib>

int main() {
    std::system("rm -rf gzip_test");
    logit::FileLogger::Config cfg;
    cfg.directory = "gzip_test";
    cfg.compress = logit::CompressType::GZIP;
    cfg.max_file_size_bytes = 20;
    cfg.compress_async = false;
    logit::Logger::get_instance().add_logger(
        std::unique_ptr<logit::FileLogger>(new logit::FileLogger(cfg)),
        std::unique_ptr<logit::SimpleLogFormatter>(new logit::SimpleLogFormatter("%v")));

    const std::string msg = "0123456789";
    LOGIT_INFO(msg);
    LOGIT_INFO(msg);
    LOGIT_WAIT();
    std::string current = LOGIT_GET_LAST_FILE_PATH(0);

    std::string rotated = current;
    size_t pos = rotated.rfind(".log");
    rotated.insert(pos, ".001");
    rotated += ".gz";

    const logit::LogFileReadResult read = LOGIT_READ_LOG_FILE(0, rotated);
    if (!read.ok || read.content.find(msg) == std::string::npos) return 1;
    const std::vector<std::string> requested = {rotated};
    const std::vector<logit::LogFileReadResult> read_many =
        LOGIT_READ_LOG_FILES(0, requested);
    if (read_many.size() != 1 || !read_many[0].ok ||
        read_many[0].content.find(msg) == std::string::npos) return 1;

    gzFile gz = gzopen(rotated.c_str(), "rb");
    if (!gz) return 1;
    char buf[128];
    std::string out;
    int n;
    while ((n = gzread(gz, buf, sizeof(buf))) > 0) out.append(buf, n);
    gzclose(gz);
    const bool ok = out.find(msg) != std::string::npos;
    LOGIT_SHUTDOWN();
    return ok ? 0 : 1;
}
#else
int main() { return 0; }
#endif
