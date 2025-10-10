# samples/CMakeLists.txt

# 由于主库已经设置了包含目录，示例只需要链接主库即可
file(GLOB EXAMPLE_SOURCES "*.cpp")

foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} cpphdl)
    
    # 设置C++标准
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endforeach()
