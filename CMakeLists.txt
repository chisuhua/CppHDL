# CMakeLists.txt (根目录)

cmake_minimum_required(VERSION 3.14)
project(MiniCash LANGUAGES CXX)

# 设置C++标准
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加核心库（虽然是头文件，但作为接口库）
add_library(cpphdl 
    src/core/context.cpp 
    src/core/lnodeimpl.cpp
    src/component.cpp
)

target_sources(cpphdl PRIVATE
    src/simulator.cpp
    src/codegen.cpp
)

include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include)
include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include/core)
target_include_directories(cpphdl INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(cpphdl INTERFACE ${CMAKE_SOURCE_DIR}/include/core)

# 添加 examples 子目录
add_subdirectory(samples)
