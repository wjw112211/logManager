# log_manager

一个薄的 glog 管理层：glog 负责高性能日志输出，本类负责初始化、目录管理、定时清理、最新日志入口和限流。

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