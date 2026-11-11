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
