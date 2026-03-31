# FIFO Example CMakeLists.txt

add_executable(fifo_stream_example stream_fifo_example.cpp)

target_link_libraries(fifo_stream_example PRIVATE cpphdl)

set_target_properties(fifo_stream_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS fifo_stream_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
