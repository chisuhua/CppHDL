# 简单构建
#cmake -B build
#cmake --build build

# 启用详细日志
#cmake -B build -DENABLE_VERBOSE_LOGGING=ON
#cmake --build build

# 启用调试日志
cmake -B build -DENABLE_DEBUG_LOGGING=ON
cmake --build build -j4

# Debug 模式构建
#cmake -B build -DCMAKE_BUILD_TYPE=Debug
#cmake --build build

# 运行测试
#cmake --build build --target test
# 或者
cd build && ctest
