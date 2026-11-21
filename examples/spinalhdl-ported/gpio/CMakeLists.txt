# GPIO Example CMakeLists.txt
# 设计定义 `gpio_top.h`（无 `main`）+ 激励驱动 `tb_gpio_example.cpp`
add_executable(tb_gpio_example tb_gpio_example.cpp)
target_link_libraries(tb_gpio_example PRIVATE cpphdl)
target_include_directories(tb_gpio_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_gpio_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
target_compile_definitions(tb_gpio_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

install(TARGETS tb_gpio_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
