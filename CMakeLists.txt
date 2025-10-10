# CMakeLists.txt (根目录)
cmake_minimum_required(VERSION 3.14)
project(CppHDL LANGUAGES CXX VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 日志配置选项
option(ENABLE_DEBUG_LOGGING "Enable debug level logging" OFF)
option(ENABLE_VERBOSE_LOGGING "Enable verbose logging with source locations" OFF)

# 设置编译定义
if(ENABLE_DEBUG_LOGGING)
    add_compile_definitions(CH_DEBUG=1)
    message(STATUS "Debug logging enabled")
endif()

if(ENABLE_VERBOSE_LOGGING)
    add_compile_definitions(CH_LOG_VERBOSE=1)
    message(STATUS "Verbose logging with source locations enabled")
else()
    add_compile_definitions(CH_LOG_VERBOSE=0)
    message(STATUS "Simple logging enabled")
endif()

# 编译器特定选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # 针对格式化安全警告
    add_compile_options(-Wno-format-security)
endif()

# 优化构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# 根据构建类型设置优化选项
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")

# 包含目录（使用相对路径）
set(CH_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ast
    ${CMAKE_CURRENT_SOURCE_DIR}/include/abstract
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
)

# 创建接口库用于包含目录
add_library(ch_includes INTERFACE)
target_include_directories(ch_includes INTERFACE 
    $<BUILD_INTERFACE:${CH_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:include>
)

# 创建 bitvector 接口库
add_library(bitvector INTERFACE)
target_include_directories(bitvector INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(bitvector INTERFACE ch_includes)

# 收集源文件
file(GLOB_RECURSE CORE_SOURCES 
    "src/core/*.cpp"
)

file(GLOB_RECURSE AST_SOURCES 
    "src/ast/*.cpp"
)

set(CH_SOURCES
    ${CORE_SOURCES}
    ${AST_SOURCES}
    src/component.cpp
    src/simulator.cpp
    src/codegen.cpp
)

# 创建主库
add_library(cpphdl ${CH_SOURCES})

# 设置库属性
set_target_properties(cpphdl PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 链接依赖
target_link_libraries(cpphdl 
    PUBLIC bitvector
)

# 设置包含目录
target_include_directories(cpphdl 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 添加示例子目录
add_subdirectory(samples)

# 添加测试
enable_testing()
add_subdirectory(tests)
