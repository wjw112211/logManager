#include "log_manager.hpp"

#include <glog/logging.h>

#include <system_error>

namespace log_manager {
namespace {

bool is_log_file(const std::filesystem::path& path, const std::string& prefix) {
    const std::string name = path.filename().string();
    return name.rfind(prefix + ".", 0) == 0 &&
           (name.find(".INFO.") != std::string::npos ||
            name.find(".WARNING.") != std::string::npos ||
            name.find(".ERROR.") != std::string::npos ||
            name.find(".FATAL.") != std::string::npos);
}

}  // namespace

LogManager& LogManager::instance() {
    static LogManager manager;
    return manager;
}

LogManager::~LogManager() {
    shutdown();
}

bool LogManager::initialize(const Options& options) {
    std::lock_guard lock(lifecycle_mutex_);
    if (initialized_) {
        return true;
    }
    if (options.retention_days < 0 || options.cleanup_interval <= std::chrono::minutes::zero() ||
        options.file_prefix.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(options.directory, error);
    if (error) {
        return false;
    }

    options_ = options;
    FLAGS_log_dir = options_.directory.string();
    FLAGS_alsologtostderr = options_.also_log_to_stderr;
    FLAGS_colorlogtostderr = options_.color_stderr;
    FLAGS_log_prefix = true;
    google::InitGoogleLogging(options_.file_prefix.c_str());

    initialized_ = true;
    stop_cleanup_ = false;
    cleanup_now();
    cleanup_thread_ = std::thread(&LogManager::cleanup_loop, this);
    return true;
}

void LogManager::shutdown() {
    std::lock_guard lock(lifecycle_mutex_);
    if (!initialized_) {
        return;
    }
    stop_cleanup_ = true;
    if (cleanup_thread_.joinable()) {
        cleanup_wait_cv_.notify_one();
        cleanup_thread_.join();
    }
    google::ShutdownGoogleLogging();
    initialized_ = false;
}

bool LogManager::initialized() const noexcept {
    return initialized_.load();
}

void LogManager::cleanup_loop() {
    while (!stop_cleanup_) {
        std::unique_lock lock(cleanup_wait_mutex_);
        cleanup_wait_cv_.wait_for(lock, options_.cleanup_interval, [this] { return stop_cleanup_.load(); });
        if (!stop_cleanup_) {
            cleanup_now();
        }
    }
}

void LogManager::cleanup_now() {
    if (!initialized_ && options_.directory.empty()) {
        return;
    }

    std::error_code error;
    const auto deadline = std::filesystem::file_time_type::clock::now() -
                          std::chrono::hours(24 * options_.retention_days);
    for (const auto& entry : std::filesystem::directory_iterator(options_.directory, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error) || !is_log_file(entry.path(), options_.file_prefix)) {
            continue;
        }
        const auto write_time = entry.last_write_time(error);
        if (!error && write_time < deadline) {
            std::filesystem::remove(entry.path(), error);
        }
    }
    refresh_latest_link();
}

void LogManager::refresh_latest_link() const {
    std::filesystem::path newest;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(options_.directory, error)) {
        if (error || !entry.is_regular_file(error) || !is_log_file(entry.path(), options_.file_prefix) ||
            entry.path().filename().string().find(".INFO.") == std::string::npos) {
            continue;
        }
        if (newest.empty() || entry.last_write_time(error) > std::filesystem::last_write_time(newest, error)) {
            newest = entry.path();
        }
    }
    if (newest.empty()) {
        return;
    }

    const auto link = options_.directory / (options_.file_prefix + ".INFO");
    std::filesystem::remove(link, error);
    std::filesystem::create_symlink(newest.filename(), link, error);
}

bool LogManager::every(const char* key, std::chrono::milliseconds interval) {
    if (key == nullptr || interval <= std::chrono::milliseconds::zero()) {
        return true;
    }
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(throttle_mutex_);
    auto& previous = last_log_[key];
    if (previous != std::chrono::steady_clock::time_point{} && now - previous < interval) {
        return false;
    }
    previous = now;
    return true;
}

}  // namespace log_manager