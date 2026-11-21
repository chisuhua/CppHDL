# Quadrature Encoder Example CMakeLists.txt
# 设计定义 `quadrature_encoder_top.h`（无 `main`）+ 激励驱动 `tb_quadrature_encoder_example.cpp`
add_executable(tb_quadrature_encoder_example tb_quadrature_encoder_example.cpp)
target_link_libraries(tb_quadrature_encoder_example PRIVATE cpphdl)
target_include_directories(tb_quadrature_encoder_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_quadrature_encoder_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
target_compile_definitions(tb_quadrature_encoder_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

install(TARGETS tb_quadrature_encoder_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
