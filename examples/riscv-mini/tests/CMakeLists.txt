# RV32I Mini Tests CMakeLists.txt

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(test_phase4_integration test_phase4_integration.cpp)
target_link_libraries(test_phase4_integration PRIVATE cpphdl catch2)
set_target_properties(test_phase4_integration PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_hazard_complete test_hazard_complete.cpp)
target_link_libraries(test_hazard_complete PRIVATE cpphdl catch2)
set_target_properties(test_hazard_complete PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_branch_predictor_v2 test_branch_predictor_v2.cpp)
target_link_libraries(test_branch_predictor_v2 PRIVATE cpphdl catch2)
set_target_properties(test_branch_predictor_v2 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
