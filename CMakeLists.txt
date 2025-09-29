# CMakeLists.txt (根目录)

cmake_minimum_required(VERSION 3.14)
project(CppHDL LANGUAGES CXX)

# 设置C++标准
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bitvector INTERFACE)
target_include_directories(bitvector
    INTERFACE ${PROJECT_SOURCE_DIR}/include
)



# 添加核心库（虽然是头文件，但作为接口库）
add_library(cpphdl 
    src/core/context.cpp 
    src/core/lnodeimpl.cpp
    src/component.cpp
    src/simulator.cpp
    src/codegen.cpp
    src/sim/instr_op.cpp
    src/sim/instr_io.cpp
    src/sim/instr_proxy.cpp
    src/sim/instr_reg.cpp
)

include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include)
include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include/core)

target_include_directories(cpphdl INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(cpphdl INTERFACE ${CMAKE_SOURCE_DIR}/include/core)
target_link_libraries(cpphdl PRIVATE bitvector)

# 添加 examples 子目录
add_subdirectory(samples)

# --- 4. (可选) 添加测试 ---
# 注册 test_ch_bitvector 作为 CTest 可执行测试。
# 这允许您运行 'ctest' 命令来执行测试。
enable_testing()
add_subdirectory(tests)

