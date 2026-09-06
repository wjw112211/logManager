cmake_minimum_required(VERSION 3.20)
project(log_manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glog CONFIG REQUIRED)

add_library(log_manager SHARED log_manager.cpp)
target_include_directories(log_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(log_manager PUBLIC glog::glog)
target_compile_features(log_manager PUBLIC cxx_std_17)
