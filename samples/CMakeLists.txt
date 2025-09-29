add_executable(counter counter.cpp)
include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include)
include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include/core)
target_link_libraries(counter cpphdl)

include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include)
include_directories(counter INTERFACE ${CMAKE_SOURCE_DIR}/include/core)


#add_executable(stream stream.cpp)
#include_directories(stream INTERFACE ${CMAKE_SOURCE_DIR}/include)
#include_directories(stream INTERFACE ${CMAKE_SOURCE_DIR}/include/core)
#target_link_libraries(stream cpphdl)
