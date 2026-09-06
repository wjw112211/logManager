# log_manager

一个薄的 glog 管理层：glog 负责高性能日志输出，本类负责初始化、目录管理、定时清理、最新日志入口和限流。

## 宏与接口

### 日志宏

- `LM_LOG(level)`：等价于 `LOG(level)`，常用于 `INFO`、`WARNING`、`ERROR`、`FATAL`。
- `LM_LOG_IF(level, condition)`：条件满足时输出日志，等价于 `LOG_IF(level, condition)`。
- `LM_VLOG(verbose_level)`：等价于 `VLOG(verbose_level)`，用于详细调试日志。
- `LM_CHECK(condition)`：等价于 `CHECK(condition)`，条件失败时直接终止程序。
- `LM_LOG_EVERY_MS(level, key, interval_ms)`：同一个 `key` 在指定毫秒间隔内只放行一次，适合热循环限流。
- `LM_LOG_IF_EVERY_MS(level, condition, key, interval_ms)`：先判断条件，再按 `key` 和时间间隔限流。

### `log_manager::LogManager`

- `LogManager::instance()`：返回单例对象。
- `initialize(const Options&)`：初始化 glog、设置日志目录和后台清理线程。
- `shutdown()`：停止后台线程并关闭日志系统。
- `initialized() const`：查询是否已初始化。
- `options() const`：读取当前配置。
- `cleanup_now()`：立即执行一次过期日志清理和最新链接刷新。
- `every(const char* key, std::chrono::milliseconds interval)`：限流判断，内部宏会直接使用它。

## 使用

```cpp
#include "log_manager.hpp"

int main() {
    log_manager::Options options;
    options.directory = "/var/log/my_service";
    options.file_prefix = "my_service";
    options.retention_days = 7;
    options.also_log_to_stderr = true;
    log_manager::LogManager::instance().initialize(options);

    LM_LOG(INFO) << "started";
    LM_LOG_IF(WARNING, retry_count > 3) << "too many retries";
    LM_LOG_EVERY_MS(INFO, "stats", 1000) << "qps=" << qps;
}
```

建议在第一次使用任何 `LM_*` 宏之前先调用 `initialize()`，否则底层 glog 可能还没有完成配置。

日志文件由 glog 生成，例如 `my_service.INFO.host.20260906-120000.1234`。目录下会维护一个 `my_service.INFO` 软链接，始终指向最新 INFO 文件。清理由后台线程按 `cleanup_interval` 执行，也可以主动调用 `cleanup_now()`。

## 在其他项目中使用

在业务项目的 `CMakeLists.txt` 中通过实际路径引入本目录，不要求两个项目处于同一目录：

```cmake
set(LOG_MANAGER_DIR "/path/to/logManager")
add_subdirectory("${LOG_MANAGER_DIR}" "${CMAKE_BINARY_DIR}/log_manager_build")

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE log_manager)
```

业务代码中直接包含头文件：

```cpp
#include "log_manager.hpp"
```

`log_manager` 目标会自动传递头文件目录和 `glog::glog` 依赖，业务目标不需要再次配置 include 路径或单独链接 glog。多个程序链接同一个 `log_manager` 动态库即可。

构建业务项目：

```bash
cmake -S . -B build
cmake --build build --parallel
```

运行程序时，需要确保系统能找到生成的动态库。macOS/Linux 可以通过动态库搜索路径配置：

```bash
export DYLD_LIBRARY_PATH=/path/to/logManager/build:$DYLD_LIBRARY_PATH  # macOS
export LD_LIBRARY_PATH=/path/to/logManager/build:$LD_LIBRARY_PATH      # Linux
```

使用前还请确保系统已经安装 glog，并且 CMake 能够找到其 CONFIG 包。Homebrew 用户可以执行：

```bash
brew install glog
sudo apt install glog
```

## 构建

```bash
cmake -S . -B build
cmake --build build
```
