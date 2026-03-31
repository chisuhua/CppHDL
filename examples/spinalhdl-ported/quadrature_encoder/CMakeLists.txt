# Quadrature Encoder Example CMakeLists.txt

add_executable(quadrature_encoder_example quadrature_encoder.cpp)

target_link_libraries(quadrature_encoder_example PRIVATE cpphdl)

set_target_properties(quadrature_encoder_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS quadrature_encoder_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
