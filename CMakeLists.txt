# CMakeLists.txt (根目录)

cmake_minimum_required(VERSION 3.14)
project(MiniCash LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加核心库（虽然是头文件，但作为接口库）
add_library(minicash INTERFACE)
target_include_directories(minicash INTERFACE ${CMAKE_SOURCE_DIR}/core)

# 添加 examples 子目录
add_subdirectory(examples)
