# Counter Example CMakeLists.txt

# 创建可执行文件 - 完整版
add_executable(counter_example counter.cpp)

# 链接 CppHDL 库
target_link_libraries(counter_example 
    PRIVATE 
        cpphdl
)

# 设置 C++ 标准
set_target_properties(counter_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# 创建可执行文件 - 简化版
add_executable(counter_simple_example counter_simple.cpp)

# 链接 CppHDL 库
target_link_libraries(counter_simple_example 
    PRIVATE 
        cpphdl
)

# 设置 C++ 标准
set_target_properties(counter_simple_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# 安装规则
install(TARGETS counter_example counter_simple_example
    RUNTIME DESTINATION bin/examples
)
