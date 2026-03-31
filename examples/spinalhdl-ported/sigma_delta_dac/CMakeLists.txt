# Sigma-Delta DAC Example CMakeLists.txt

add_executable(sigma_delta_dac_example sigma_delta_dac.cpp)

target_link_libraries(sigma_delta_dac_example PRIVATE cpphdl)

set_target_properties(sigma_delta_dac_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS sigma_delta_dac_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
