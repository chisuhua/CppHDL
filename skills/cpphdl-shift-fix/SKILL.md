# CppHDL 位移操作 UB 修复技能

**技能 ID**: `cpphdl-shift-fix`  
**版本**: v1.0  
**创建日期**: 2026-04-09  
**优先级**: Critical

---

## 问题描述

在 CppHDL 项目中，位移操作使用 `1 << N` 模式时，当位移量 `N >= 32` 会触发**未定义行为**（Undefined Behavior, UB）。

### 根因分析

C++ 标准规定：
- `1` 是 `int` 类型（32 位有符号整数）
- 当位移量 `>= 32` 时，超出类型精度范围
- 这是**未定义行为**，可能导致：
  - 编译错误
  - 运行时错误结果
  - 不可预测的行为

### 典型触发场景

```cpp
// ❌ 错误：当 RHS 是 32 位或更宽时触发 UB
ch_uint<32> a;
ch_uint<64> b;
auto c = a << b;  // 编译错误：1 << 64 是 UB

// ❌ 错误：模板参数可能导致 UB
template <unsigned N>
static constexpr unsigned SIZE = 1 << N;  // N >= 32 时 UB

InstrTCM<32> tcm;  // 1 << 32 = UB!
```

---

## 修复方案

### 标准修复模式

将位移操作提升到 64 位无符号类型：

```cpp
// ❌ 错误
1 << N

// ✅ 正确（推荐）
1ULL << N

// ✅ 正确（显式转换）
static_cast<uint64_t>(1) << N

// ✅ 正确（32 位无符号，仅当 N < 32 时安全）
1u << N
```

### 已修复文件清单

| 文件 | 行数 | 修复内容 | 状态 |
|------|------|----------|------|
| `include/core/operators.h` | 758 | 左移操作符掩码计算 | ✅ |
| `include/chlib/fifo.h` | 30,54,120,316 | FIFO 容量计算 | ✅ |
| `include/chlib/axi4lite.h` | 116 | AXI4-Lite 内存大小 | ✅ |
| `include/riscv/rv32i_tcm.h` | 28,71,111 | TCM 存储器大小 | ✅ |

---

## 执行流程

### Step 1: 搜索风险点

```bash
cd /workspace/project/CppHDL

# 搜索所有 1 << 模式
grep -rn "1\s*<<" include/ | grep -v "1ULL\|1u\|static_cast"
```

### Step 2: 评估风险等级

| 风险等级 | 判定标准 | 修复优先级 |
|----------|----------|------------|
| 🔴 高危 | 位移量可能 >= 32 | 立即修复 |
| 🟡 中危 | 位移量由模板参数决定 | 本周修复 |
| 🟢 低危 | 位移量 < 32（如 base=0-4） | 可延后 |

### Step 3: 应用修复

**模式 1: 常量位移**
```cpp
// 修复前
static constexpr unsigned SIZE = 1 << ADDR_WIDTH;

// 修复后
static constexpr unsigned SIZE = 1ULL << ADDR_WIDTH;
```

**模式 2: 表达式位移**
```cpp
// 修复前
return (1 << ch_width_v<RHS>) - 1;

// 修复后
return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
```

**模式 3: 函数参数（低风险）**
```cpp
// base 通常是 0-4，不会触发 UB
int v = char2int(chr, 1 << base);  // 可保持原样
```

### Step 4: 验证修复

```bash
# 编译验证
cd build && make -j8

# 运行相关测试
ctest -R "fifo|literal|stream" --output-on-failure

# 验证无 shift-overflow 警告
make 2>&1 | grep -i "shift-overflow"
```

---

## 检查清单

### 代码审查

- [ ] 搜索所有 `1 <<` 位移操作
- [ ] 评估每个位移量的可能范围
- [ ] 对可能 >= 32 的位移使用 `1ULL <<`
- [ ] 添加边界检查（如 `if (shift >= 64)`）

### 编译验证

- [ ] 启用编译器警告：`-Wshift-overflow=2`
- [ ] 编译无错误
- [ ] 编译无 shift-overflow 警告

### 测试验证

- [ ] 运行位移操作相关单元测试
- [ ] 添加 32 位、64 位边界条件测试
- [ ] 性能回归测试（可选）

### CI/CD 集成

- [ ] 在 CMakeLists.txt 中添加 `-Wshift-overflow=2`
- [ ] 将警告视为错误：`-Werror=shift-overflow`
- [ ] 防止未来引入类似 UB

---

## CI/CD 配置

### CMakeLists.txt 修改

```cmake
# 添加位移溢出警告
target_compile_options(cpphdl PRIVATE
  -Wshift-overflow=2      # 警告位移量 >= 类型精度
  -Werror=shift-overflow  # 将警告视为错误（可选）
)
```

### GitHub Actions

```yaml
- name: Build and Test
  run: |
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    ctest --output-on-failure
```

---

## 相关文件

- **修复记录**: `.sisyphus/plans/2026-04-01-shift-ub-fix.md`
- **技术债务计划**: `.sisyphus/plans/cpphdl-debt-cleanup.md`
- **Week 1 执行计划**: `.sisyphus/plans/week1-execution-plan.md`

---

## 经验教训

### 编码规范更新

在 CppHDL 项目中强制要求：

```markdown
## 位移操作安全规范

1. **禁止使用 `1 << N`**，必须明确类型：
   - ✅ `1u << N`（无符号 32 位，N < 32）
   - ✅ `1ULL << N`（无符号 64 位，N < 64）
   - ✅ `static_cast<uint64_t>(1) << N`（显式转换）
   - ❌ `1 << N`（有符号 int，UB 风险）

2. **位移量必须检查边界**：
   ```cpp
   if (shift >= sizeof(type) * 8) {
       // 处理溢出情况
   }
   ```

3. **使用 C++20 工具**（如果可用）：
   - `std::bit_width()` 计算位宽
   - `std::bit_cast()` 类型转换
   - `<bit>` 头文件中的位操作工具
```

### 代码审查清单

在审查代码时，检查位移操作：

```markdown
## 位移操作审查

- [ ] 是否使用明确的类型（`1u`, `1ULL`, `uint64_t(1)`）？
- [ ] 位移量是否有边界检查？
- [ ] 模板元编程中是否处理极端情况（0, 32, 64）？
- [ ] 是否有单元测试覆盖边界条件？
```

---

## 使用方式

在项目开发中，当遇到位移操作时：

```bash
# 加载技能
skill cpphdl-shift-fix

# 或使用规划代理执行修复
task category="quick" \
     load_skills=["cpphdl-shift-fix"] \
     prompt="修复 include/my_header.h 中的位移 UB 问题"
```

---

**状态**: ✅ 已完成  
**维护人**: DevMate  
**下次更新**: 发现新的位移 UB 模式时
