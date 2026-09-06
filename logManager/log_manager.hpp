#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace log_manager {

struct Options {
    std::filesystem::path directory = "./logs";
    std::string file_prefix = "app";
    int retention_days = 7;
    std::chrono::minutes cleanup_interval = std::chrono::hours(1);
    bool also_log_to_stderr = false;
    bool color_stderr = true;
};

class LogManager final {
public:
    static LogManager& instance();

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    bool initialize(const Options& options = {});
    void shutdown();
    bool initialized() const noexcept;
    const Options& options() const noexcept { return options_; }

    // 删除过期文件，并刷新 <prefix>.INFO 软链接。
    void cleanup_now();

    // 同一个 key 在 interval 内只允许通过一次，适合热循环限流。
    bool every(const char* key, std::chrono::milliseconds interval);

private:
    LogManager() = default;
    ~LogManager();

    void cleanup_loop();
    void refresh_latest_link() const;

    Options options_;
    std::thread cleanup_thread_;
    std::mutex throttle_mutex_;
    std::mutex lifecycle_mutex_;
    std::mutex cleanup_wait_mutex_;
    std::condition_variable cleanup_wait_cv_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_log_;
    std::atomic<bool> stop_cleanup_{false};
    std::atomic<bool> initialized_{false};
};

}  // namespace log_manager

#include <glog/logging.h>

#define LM_LOG(level) LOG(level)
#define LM_LOG_IF(level, condition) LOG_IF(level, condition)
#define LM_VLOG(verbose_level) VLOG(verbose_level)
#define LM_CHECK(condition) CHECK(condition)

#define LM_LOG_EVERY_MS(level, key, interval_ms) \
    if (!::log_manager::LogManager::instance().every((key), std::chrono::milliseconds(interval_ms))) \
        ; \
    else \
        LOG(level)

#define LM_LOG_IF_EVERY_MS(level, condition, key, interval_ms) \
    if (!(condition) || !::log_manager::LogManager::instance().every((key), std::chrono::milliseconds(interval_ms))) \
        ; \
    else \
        LOG(level)