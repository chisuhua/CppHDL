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
    # 禁用一些不必要的警告
    add_compile_options(-Wno-deprecated-copy)
    add_compile_options(-Wno-unused-parameter)
    add_compile_options(-Wno-unused-variable)
    
    # 位移溢出警告（预防 UB）
    # -Wshift-overflow=2: 警告有符号和无符号位移溢出
    add_compile_options(-Wshift-overflow=2)
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set(CMAKE_C_COMPILER_LAUNCHER "ccache")
    set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
endif()

# AddressSanitizer 选项
option(ENABLE_ASAN "Enable AddressSanitizer for memory leak detection" OFF)
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/inipp
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

# 收集源文件（显式列表，避免 GLOB_RECURSE 反模式）
set(CORE_SOURCES
    src/core/context.cpp
    src/core/lnodeimpl.cpp
)

set(AST_SOURCES
    src/ast/ast_nodes.cpp
    src/ast/clockimpl.cpp
    src/ast/instr_clock.cpp
    src/ast/instr_io.cpp
    src/ast/instr_mem.cpp
    src/ast/instr_proxy.cpp
    src/ast/instr_reg.cpp
    src/ast/mem_port_impl.cpp
    src/ast/memimpl.cpp
    src/ast/muximpl.cpp
    src/ast/resetimpl.cpp
)

set(CH_SOURCES
    ${CORE_SOURCES}
    ${AST_SOURCES}
    src/component.cpp
    src/simulator.cpp
    src/codegen_verilog.cpp
    src/codegen_dag.cpp
    src/utils/types.cpp
    src/lnode/bool.cpp
    #src/lnode/uint.cpp
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

# 添加 SpinalHDL 移植示例
add_subdirectory(examples/spinalhdl-ported/counter)
add_subdirectory(examples/spinalhdl-ported/uart)
add_subdirectory(examples/spinalhdl-ported/fifo)
add_subdirectory(examples/spinalhdl-ported/width_adapter)
add_subdirectory(examples/spinalhdl-ported/pwm)
add_subdirectory(examples/spinalhdl-ported/gpio)
add_subdirectory(examples/spinalhdl-ported/spi)
add_subdirectory(examples/spinalhdl-ported/i2c)
add_subdirectory(examples/spinalhdl-ported/timer)
add_subdirectory(examples/spinalhdl-ported/assert)
add_subdirectory(examples/spinalhdl-ported/phase1c)
add_subdirectory(examples/spinalhdl-ported/uart_rx)
add_subdirectory(examples/spinalhdl-ported/vga)
add_subdirectory(examples/spinalhdl-ported/ws2812)
add_subdirectory(examples/spinalhdl-ported/quadrature_encoder)
add_subdirectory(examples/spinalhdl-ported/sigma_delta_dac)

# Phase 3: AXI4 总线系统
add_subdirectory(examples/axi4)

# Phase 3A: RISC-V SoC (Temporarily disabled due to API changes)
# add_subdirectory(examples/riscv)  # Deprecated, use riscv-mini
add_subdirectory(examples/riscv-mini)  # Temporarily disabled - API updates needed

# 添加测试
enable_testing()
add_subdirectory(tests)
