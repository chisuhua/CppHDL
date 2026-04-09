# CppHDL 测试指南

## 概述

CppHDL 项目使用 CMake 和 CTest 框架来管理单元测试。测试被分为两组：
- **基础测试 (base)**：包含CppHDL核心功能的测试
- **chlib测试 (chlib)**：包含chlib库组件的测试

## 构建项目与测试

### 1. 环境准备

确保你已经安装了必要的工具：

- C++20 兼容编译器（GCC ≥ 10 或 Clang ≥ 12）
- CMake ≥ 3.14
- Make 或 Ninja 构建工具

### 2. 构建项目

```bash
# 克隆项目（如果尚未克隆）
git clone https://github.com/chisuhua/CppHDL.git
cd CppHDL

# 创建构建目录并配置
mkdir build && cd build
cmake ..

# 编译主库、示例和测试
make
```

### 3. 构建选项

你可以使用以下 CMake 选项来控制构建：

- `ENABLE_DEBUG_LOGGING=ON` - 启用调试日志
- `ENABLE_VERBOSE_LOGGING=ON` - 启用详细日志输出
- `CMAKE_BUILD_TYPE=Debug` - 构建调试版本
- `CMAKE_BUILD_TYPE=Release` - 构建发布版本（默认）

```bash
# 构建带调试日志的版本
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_DEBUG_LOGGING=ON
cmake --build build
```

## 运行测试

### 1. 运行所有测试

```bash
# 在构建目录中运行所有测试
cd build
ctest
```

或者使用 make 命令：

```bash
make test
```

### 2. 分组运行测试

CppHDL 测试被分为两组，你可以选择性地运行：

- **运行所有测试**：
```bash
ctest
```

- **只运行基础测试**（核心功能）：
```bash
ctest -L base
```

- **只运行chlib测试**（库组件）：
```bash
ctest -L chlib
```

### 3. 详细输出

要查看测试的详细输出，可以使用 `-V`（或 `--verbose`）选项：

```bash
# 详细输出所有测试
ctest -V

# 详细输出基础测试
ctest -V -L base

# 详细输出chlib测试
ctest -V -L chlib
```

### 4. 运行特定测试

你可以运行特定的测试：

```bash
# 运行单个测试（例如 test_basic）
ctest -R test_basic

# 运行匹配模式的测试
ctest -R "test_bundle"
```

### 5. 统计信息

要查看测试执行的统计信息：

```bash
# 显示测试摘要
ctest --output-on-failure

# 生成测试报告
ctest --output-on-failure --timeout 120
```

## 测试结构

### 基础测试 (base)

基础测试覆盖 CppHDL 的核心功能，包括：

- `test_basic.cpp` - 基本功能测试
- `test_reg.cpp` - 寄存器相关功能测试
- `test_operator.cpp` - 操作符重载测试
- `test_literal.cpp` - 字面量功能测试
- `test_bundle.cpp` - Bundle 功能测试
- `test_mem.cpp` - 内存相关功能测试
- `test_module.cpp` - 模块功能测试
- `test_io_operations.cpp` - IO操作测试
- `test_bitvector.cpp` - 位向量操作测试
- 等等

### chlib测试 (chlib)

chlib测试覆盖 CppHDL 的库组件功能，包括：

- `chlib/test_arithmetic.cpp` - 算术运算组件测试
- `chlib/test_logic.cpp` - 逻辑运算组件测试
- `chlib/test_combinational.cpp` - 组合逻辑组件测试
- `chlib/test_sequential.cpp` - 时序逻辑组件测试
- `chlib/test_converter.cpp` - 转换器组件测试
- `chlib/test_onehot.cpp` - 独热编码相关测试
- 等等

## 测试开发

### 添加新测试

当添加新测试时，请遵循以下准则：

1. **确定测试类型**：决定测试是属于基础测试还是chlib测试
2. **创建测试文件**：编写符合 Catch2 框架的测试代码
3. **更新 CMakeLists.txt**：将新测试添加到相应的测试列表中

#### 基础测试添加

将新测试文件添加到 `tests/CMakeLists.txt` 中的 `BASE_TEST_SOURCES` 列表：

```cmake
set(BASE_TEST_SOURCES
    # ... 现有测试 ...
    your_new_test.cpp
)
```

#### chlib测试添加

将新测试文件添加到 `tests/CMakeLists.txt` 中的 `CHLIB_TEST_SOURCES` 列表：

```cmake
set(CHLIB_TEST_SOURCES
    # ... 现有测试 ...
    chlib/your_new_chlib_test.cpp
)
```

### 测试命名约定

- 测试文件应以 `test_` 开头
- 对于chlib测试，应放在 `tests/chlib/` 目录下
- 测试函数应使用描述性的名称，清晰地表明测试目的

### 测试最佳实践

1. **测试独立性**：每个测试应独立于其他测试，不应依赖测试执行顺序
2. **明确的断言**：使用清晰的断言，便于理解测试失败的原因
3. **覆盖边界情况**：确保测试覆盖边界情况和错误情况
4. **性能考虑**：避免测试运行时间过长，保持测试快速

## CI/CD 集成

在持续集成环境中，可以使用以下命令：

```bash
# 运行基础测试
ctest -L base --output-on-failure

# 运行chlib测试
ctest -L chlib --output-on-failure
```

这允许分阶段验证，确保核心功能正常后再验证库组件。

## 故障排除

### 测试失败

如果测试失败，请：

1. 检查详细输出：`ctest -V -R test_name`
2. 确认构建环境：`cmake --build build --verbose`
3. 查看测试日志：在 `build/Testing/Temporary/` 目录中

### 性能问题

如果测试运行缓慢：

1. 使用 `-j` 选项并行运行测试：`ctest -j4`
2. 仅运行相关的测试组：`ctest -L base`
3. 运行特定测试：`ctest -R specific_test_name`

## 附录

### 常用ctest命令

- `ctest -N` - 列出所有测试但不运行
- `ctest -L label` - 运行指定标签的测试
- `ctest -LE label` - 运行不包含指定标签的测试
- `ctest --timeout seconds` - 设置测试超时时间
- `ctest --repeat-until-fail n` - 重复测试直到失败或达到指定次数
- `ctest -j n` - 并行运行n个测试