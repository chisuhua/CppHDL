# PWM Example CMakeLists.txt

add_executable(pwm_example pwm.cpp)

target_link_libraries(pwm_example PRIVATE cpphdl)

set_target_properties(pwm_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS pwm_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
