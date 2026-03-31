# AXI4-Lite Example CMakeLists.txt

add_executable(axi4_lite_example axi4_lite_example.cpp)

target_link_libraries(axi4_lite_example PRIVATE cpphdl)
target_include_directories(axi4_lite_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set_target_properties(axi4_lite_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS axi4_lite_example
    RUNTIME DESTINATION bin/axi4
)
