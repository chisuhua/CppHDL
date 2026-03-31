# Width Adapter Example CMakeLists.txt

add_executable(width_adapter_example stream_width_adapter_example.cpp)

target_link_libraries(width_adapter_example PRIVATE cpphdl)

set_target_properties(width_adapter_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS width_adapter_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
