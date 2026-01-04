# CppHDL 测试说明

本项目包含两组测试：基础测试和chlib测试。这些测试使用CMake和CTest框架管理。

## 测试分组

- **基础测试 (base)**：包含CppHDL核心功能的测试，如寄存器、操作符、字面量、bundle、内存等
- **chlib测试 (chlib)**：包含chlib库组件的测试，如算术运算、逻辑运算、组合逻辑、时序逻辑等

## 运行测试

### 构建项目

```bash
mkdir build
cd build
cmake ..
make
```

### 运行所有测试

```bash
ctest
```

### 只运行基础测试

```bash
ctest -L base
```

### 只运行chlib测试

```bash
ctest -L chlib
```

### 运行特定测试

```bash
# 运行单个测试
./test_basic

# 或使用ctest运行特定测试
ctest -R test_basic
```

### 查看测试标签

```bash
ctest -N
```

## 添加新测试

当添加新测试时，请注意：

- 核心功能测试应添加到基础测试组
- chlib相关功能测试应添加到chlib测试组
- 修改 [CMakeLists.txt](file:///mnt/ubuntu/chisuhua/github/CppHDL/tests/CMakeLists.txt) 文件以包含新的测试文件