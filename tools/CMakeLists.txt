# tools/CMakeLists.txt
#
# `tools/` 下的**命令行工具**。
#
# ⚠️ **本仓二进制 MUST NOT 链 `examples/**`**（`rtl-emit-product-tooling` C1
#   硬要求）⇒ ⇒ `cpphdl-emit-rtl` 用**自己的**小设计 ⇒ ⇒ 工具不依赖 examples
#   ⇒ ⇒ `examples/` 改了不会影响它 ⇒ ⇒ 它可以当**自测载体**。
# ---------------------------------------------------------------------------

add_executable(cpphdl-emit-rtl
    cpphdl-emit-rtl/main.cpp
)
target_include_directories(cpphdl-emit-rtl PRIVATE ${PROJECT_SOURCE_DIR}/include)
if(TARGET cpphdl)
    target_link_libraries(cpphdl-emit-rtl cpphdl)
endif()

# ⚠️ **2026-11-14 · L4/L5**：RV32I 固件转储器 / ELF 装载器
#   ⇒ ⇒ **MUST NOT 链 `examples/**`**（C1 纪律）⇒ ⇒ 只链 `cpphdl` 库
#   ⇒ ⇒ 地址映射**单一真相**（`include/cpu/riscv/rv32i_mem_map.h`）
add_executable(rv32i-fw-dump
    rv32i-fw-dump/main.cpp
)
target_include_directories(rv32i-fw-dump PRIVATE ${PROJECT_SOURCE_DIR}/include)
if(TARGET cpphdl)
    target_link_libraries(rv32i-fw-dump cpphdl)
endif()
