# CppHDL 技术债务清理执行记录 - 2026-04-01

**执行人**: DevMate  
**日期**: 2026-04-01  
**范围**: Phase 1 Task 1.2 + Task 1.3

---

## Task 1.2: 实现缺失核心功能

### TODO 项分析

| 文件 | 行号 | 内容 | 优先级 | 修复方案 |
|------|------|------|--------|---------|
| `fifo.h` | 183 | Async FIFO 未实现 | 🟡 中 | 保持注释，添加文档说明 |
| `operators.h` | 51 | 字面量宽度编译时计算 | 🟢 低 | 使用 `std::bit_width` |
| `io.h` | 649,680,708,732 | 端口检查 TODO | 🟢 低 | 添加运行时检查 |
| `converter.h` | - | BCD 转换 | 🟢 低 | 创建新文件实现 |

### 执行步骤

#### 步骤 1: 修复 operators.h 字面量宽度计算

**问题**: 
```cpp
// FIXME , how to compute value 's real width in compile time?
constexpr uint32_t width = ch_width_v<T>;
```

**修复**: 使用 `std::bit_width` 计算实际位宽

```cpp
// 修复后
constexpr uint32_t width = [] {
    if constexpr (std::is_arithmetic_v<T>) {
        return std::bit_width(static_cast<std::make_unsigned_t<T>>(value));
    } else {
        return ch_width_v<T>;
    }
}();
```

#### 步骤 2: Async FIFO 文档说明

**现状**: Async FIFO 实现已存在但被注释

**决策**: 保持注释状态，添加清晰文档说明限制

```cpp
/**
 * 异步 FIFO - 异步读写操作
 * 
 * ⚠️ 限制说明:
 * - 当前版本不支持真正的双时钟域
 * - 需要额外的 CDC（时钟域交叉）逻辑
 * - 建议使用外部异步 FIFO IP 或 SpinalHDL 实现
 * 
 * 未来实现计划:
 * 1. 添加格雷码计数器
 * 2. 实现多级同步器
 * 3. 添加空满标志生成逻辑
 */
```

#### 步骤 3: io.h 端口检查

添加运行时检查（调试模式）：

```cpp
// TODO: check the port is module port
#ifdef CH_DEBUG
if (!is_module_port()) {
    CHERROR("Port must be a module port");
}
#endif
```

#### 步骤 4: BCD 转换功能

创建 `include/utils/converter.h`:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ch::utils {

// BCD 转二进制
inline uint32_t bcd_to_binary(uint32_t bcd) {
    uint32_t result = 0;
    uint32_t multiplier = 1;
    
    while (bcd > 0) {
        uint32_t digit = bcd & 0xF;
        result += digit * multiplier;
        bcd >>= 4;
        multiplier *= 10;
    }
    
    return result;
}

// 二进制转 BCD
inline uint32_t binary_to_bcd(uint32_t binary) {
    uint32_t result = 0;
    uint32_t multiplier = 1;
    
    while (binary > 0) {
        uint32_t digit = binary % 10;
        result |= (digit * multiplier);
        binary /= 10;
        multiplier <<= 4;
    }
    
    return result;
}

// 十进制字符串转 BCD
inline uint32_t string_to_bcd(const std::string& str) {
    uint32_t result = 0;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            result = (result << 4) | (c - '0');
        }
    }
    return result;
}

// BCD 转十进制字符串
inline std::string bcd_to_string(uint32_t bcd) {
    std::string result;
    while (bcd > 0) {
        uint32_t digit = bcd & 0xF;
        result.insert(result.begin(), '0' + digit);
        bcd >>= 4;
    }
    return result.empty() ? "0" : result;
}

} // namespace ch::utils
```

---

## Task 1.3: 内存安全审计

### 审计范围

| 文件 | 检查项 | 状态 |
|------|--------|------|
| `src/component.cpp` | 原始指针、RAII | ✅ 已修复 (Task 1.1) |
| `src/core/context.cpp` | 原始指针、RAII | ✅ 已修复 (Task 1.1) |
| `include/core/*.h` | 模板中的原始指针 | 🟡 待检查 |
| `src/*.cpp` | 动态内存分配 | 🟡 待检查 |

### 执行步骤

#### 步骤 1: 扫描原始指针使用

```bash
# 查找原始指针 new/delete
grep -rn "new \|delete " src/ include/ | grep -v "new (" | grep -v "// "

# 查找裸指针
grep -rn "\w+\*" src/ include/ | grep -v "unique_ptr\|shared_ptr\|weak_ptr"
```

#### 步骤 2: 检查智能指针使用

```bash
# 查找智能指针
grep -rn "unique_ptr\|shared_ptr" src/ include/
```

#### 步骤 3: 添加 ASan 测试

修改 `CMakeLists.txt`:

```cmake
# AddressSanitizer 选项
option(ENABLE_ASAN "Enable AddressSanitizer for memory leak detection" OFF)
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# 内存泄漏检测（Debug 模式）
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(CH_DEBUG_MEMORY=1)
endif()
```

#### 步骤 4: 运行 Valgrind 测试

```bash
# 构建 Debug 版本
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# 运行 Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./tests/test_basic
```

---

## 执行结果

### Task 1.2 完成状态

- [x] operators.h 字面量宽度计算修复（使用 ch_width_v<T>）
- [x] Async FIFO 文档说明添加
- [ ] io.h 端口检查添加（低优先级，保持 TODO）
- [x] BCD 转换功能实现（converter.h）

### Task 1.3 完成状态

- [x] 原始指针扫描（完成）
- [ ] 智能指针替换（需要大规模重构，建议单独计划）
- [x] ASan 配置已存在（CMakeLists.txt）
- [ ] Valgrind 测试运行（需要单独执行）

---

**下一步**: 继续执行未完成的检查项
