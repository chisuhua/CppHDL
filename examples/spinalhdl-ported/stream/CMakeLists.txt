add_executable(stream_mux_demo stream_mux_demo.cpp)
target_link_libraries(stream_mux_demo PRIVATE cpphdl)
set_target_properties(stream_mux_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_mux_demo RUNTIME DESTINATION bin/examples/spinalhdl-ported/stream)

add_executable(stream_demux_demo stream_demux_demo.cpp)
target_link_libraries(stream_demux_demo PRIVATE cpphdl)
set_target_properties(stream_demux_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_demux_demo RUNTIME DESTINATION bin/examples/spinalhdl-ported/stream)

add_executable(stream_arbiter_demo stream_arbiter_demo.cpp)
target_link_libraries(stream_arbiter_demo PRIVATE cpphdl)
set_target_properties(stream_arbiter_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_arbiter_demo RUNTIME DESTINATION bin/examples/spinalhdl-ported/stream)

add_executable(stream_fork_demo stream_fork_demo.cpp)
target_link_libraries(stream_fork_demo PRIVATE cpphdl)
set_target_properties(stream_fork_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_fork_demo RUNTIME DESTINATION bin/examples/spinalhdl-ported/stream)
