# tests/downstream-consumer/CMakeLists.txt
#
# 下游视角的**独立工程**（由 scripts/verify_downstream_consumer.sh 配置与构建）。
# ⚠️ 这里**不** add_subdirectory 上游、**不** include 上游的 CMake 逻辑
#   ⇒ ⇒ 它只消费 `cmake --install` 的产物（include 前缀 + 静态库）。
cmake_minimum_required(VERSION 3.20)
project(cpphdl_downstream_consumer CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CPPHDL_PREFIX)
    message(FATAL_ERROR "CPPHDL_PREFIX is required (the install prefix)")
endif()

# ⚠️ 下游 MUST 加 5 个 include 目录 —— 与上游 `CH_INCLUDE_DIRS`
# （CMakeLists.txt:116-123）同口径：安装后的头**不是**单目录自足的
# （例：`ast/ast_nodes.h` 写 `#include "lnodeimpl.h"`，而该头装在 `core/`）。
set(CPPHDL_INCLUDE_ROOT ${CPPHDL_PREFIX}/include/cpp-hdl)

add_executable(consumer main.cpp)
target_include_directories(consumer PRIVATE
    ${CPPHDL_INCLUDE_ROOT}
    ${CPPHDL_INCLUDE_ROOT}/core
    ${CPPHDL_INCLUDE_ROOT}/ast
    ${CPPHDL_INCLUDE_ROOT}/abstract
    ${CPPHDL_INCLUDE_ROOT}/utils
)
target_link_libraries(consumer PRIVATE ${CPPHDL_PREFIX}/lib/libcpphdl.a)
