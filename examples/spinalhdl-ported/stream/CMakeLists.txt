add_executable(stream_mux_demo stream_mux_demo.cpp)
target_link_libraries(stream_mux_demo PRIVATE cpphdl)
set_target_properties(stream_mux_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_mux_demo RUNTIME DESTINATION bin/examples/spinalhdl-ported/stream)
