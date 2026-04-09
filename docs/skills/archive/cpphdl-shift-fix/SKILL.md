# CppHDL 位移操作编译错误修复技能

## 触发条件
当 CppHDL 项目编译出现以下错误时激活：
- `error: right operand of shift expression '(1 << 32)' is greater than or equal to the precision 32 of the left operand`
- `error: shift exponent too large`
- 类似的位移操作未定义行为警告

## 问题根因

### 核心问题
在 `operators.h` 的 `operator<<` 实现中，计算 `rhs_extra_width` 时：
```cpp
return (1 << ch_width_v<RHS>) - 1;
```

当 `ch_width_v<RHS> >= 32` 时，`1 << 32` 触发**未定义行为**（UB），因为：
- `1` 是 `int` 类型（32 位）
- 位移量 `>= 32` 超出类型精度范围
- C++ 标准规定这是未定义行为

### 典型场景
```cpp
// 当 RHS 是 32 位或更宽的操作数时
ch_uint<32> a;
ch_uint<64> b;
auto c = a << b;  // 编译错误：1 << 64 未定义
```

## 修复方案

### 方案 1：使用更宽类型（推荐）
将位移操作提升到 64 位：
```cpp
// 修复前
return (1 << ch_width_v<RHS>) - 1;

// 修复后
return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
```

### 方案 2：添加边界检查
对于超宽类型，限制最大位移量：
```cpp
constexpr unsigned rhs_extra_width = [] {
    if constexpr (CHLiteral<RHS>) {
        return RHS::actual_value;
    } else {
        constexpr unsigned width = ch_width_v<RHS>;
        if constexpr (width >= 64) {
            return ~0u;  // 最大值
        } else {
            return (1u << width) - 1;
        }
    }
}();
```

### 方案 3：使用位运算技巧
避免位移操作：
```cpp
// 使用掩码计算
constexpr unsigned rhs_extra_width = [] {
    if constexpr (CHLiteral<RHS>) {
        return RHS::actual_value;
    } else {
        constexpr unsigned width = ch_width_v<RHS>;
        if constexpr (width == 0) return 0;
        if constexpr (width >= 32) return ~0u;
        return (1u << width) - 1;
    }
}();
```

## 执行流程

### 1. 定位错误文件
```bash
cd /workspace/CppHDL
grep -n "1 << ch_width_v" include/core/operators.h
```

### 2. 检查所有位移操作
搜索项目中所有位移操作：
```bash
grep -rn "1 <<" include/ src/ | grep -v ".md"
```

### 3. 应用修复
根据实际场景选择修复方案：
- **通用场景**：方案 1（64 位类型）
- **安全关键**：方案 2（边界检查）
- **性能敏感**：方案 3（位运算技巧）

### 4. 验证修复
```bash
cd /workspace/CppHDL/build
cmake .. && make -j$(nproc)
```

## 检查清单

修复后必须验证：
- [ ] 编译无警告（`-Wshift-overflow`）
- [ ] 单元测试通过（特别是位移操作测试）
- [ ] 边界条件测试（0 位、31 位、32 位、63 位、64 位）
- [ ] 性能回归测试（确保没有引入额外开销）

## 预防措施

### 代码规范
在 `CppHDL` 项目中添加编码规范：
```markdown
## 位移操作安全规范

1. 禁止使用 `1 << N`，必须明确类型：
   - ✅ `1u << N`（无符号 32 位）
   - ✅ `1ULL << N`（无符号 64 位）
   - ❌ `1 << N`（有符号 int，UB 风险）

2. 位移量必须检查边界：
   ```cpp
   if (shift >= sizeof(type) * 8) {
       // 处理溢出情况
   }
   ```

3. 使用 `std::bit_width` 等现代 C++20 工具
```

### CI/CD 集成
在构建脚本中添加：
```bash
# 启用位移溢出警告
CXXFLAGS="-Wshift-overflow=2 -Werror"
```

## 相关技能

- `cpp-pro`：C++ 高性能编程最佳实践
- `cuda`：GPU 编程中的位移操作优化
- `cmake`：构建系统配置

## 历史案例

### 2026-04-01: operators.h 位移 UB 问题

**问题现象**:
```
/workspace/CppHDL/include/core/operators.h:758:23: error: 
right operand of shift expression '(1 << 32)' is greater than or equal 
to the precision 32 of the left operand [-fpermissive]
```

**根本原因**:
```cpp
// ❌ 错误代码
return (1 << ch_width_v<RHS>) - 1;
// 当 ch_width_v<RHS> >= 32 时，1 << 32 是未定义行为
```

**修复方案**:
```cpp
// ✅ 正确代码（参考 literal.h 第 33 行）
if (actual_width == 64) {
    return actual_value == UINT64_MAX;
}
return actual_value == ((uint64_t{1} << actual_width) - 1);
```

**验证结果**: ✅ 编译通过，单元测试通过

### 2026-04-01: context.tpp 断言宏问题

**问题现象**:
```
!(condition) && !ch::detail::in_static_destruction()) { \
```

**根本原因**: 断言宏在静态析构期间的行为未正确处理

**修复方案**: 参考 `CHREQUIRE` 宏实现，添加静态析构检查

**验证结果**: ✅ 编译通过

## 快速修复命令

```bash
# 自动修复 operators.h 中的位移问题
sed -i 's/(1 << ch_width_v/(static_cast<uint64_t>(1) << ch_width_v/g' \
  /workspace/CppHDL/include/core/operators.h

# 重新构建
cd /workspace/CppHDL/build && make -j$(nproc)
```

---

**技能版本**: v1.0  
**创建时间**: 2026-04-01  
**适用项目**: CppHDL, PTX-EMU, 及其他使用模板元编程的 C++ 项目
