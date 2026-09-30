#include <cstdio>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
int main() {
    std::puts("Before mutex");std::fflush(stdout);
    std::mutex mutex;{std::lock_guard lock(mutex);std::puts("Mutex works");std::fflush(stdout);}
    auto logger=spdlog::basic_logger_mt("smoke","voxel-runtime-smoke.log");
    logger->info("C++ runtime and logging work: {}",42);logger->flush();
    std::puts("Logging works");return 0;
}
