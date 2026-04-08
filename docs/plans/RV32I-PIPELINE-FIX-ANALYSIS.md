# RV32I Pipeline 失败测试分析

**日期**: 2026-04-08  
**状态**: ✅ **已修复** (2026-04-09)  
**优先级**: P1  

---

## 📊 测试结果

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 测试用例 | 12 | 12 |
| 通过 | 11 ✅ | 12 ✅ |
| 失败 | 1 ❌ (3 个断言失败) | 0 ✅ |
| 断言总数 | 26 | 26 |
| 通过断言 | 23 | 26 |

---

## 🔍 失败详情 (已修复)

### 失败 1: BEQ equal (行 273) ✅ 已修复

**测试代码**:
```cpp
SECTION("BranchConditionUnit BEQ equal") {
    sim.set_input_value(unit.instance().io().branch_type, 0);
    sim.set_input_value(unit.instance().io().rs1_data, 42);
    sim.set_input_value(unit.instance().io().rs2_data, 42);
    
    auto condition = to_bool(sim.get_port_value(unit.instance().io().branch_condition));
    REQUIRE(condition == true);  // ✅ 现在通过
}
```

---

### 失败 2: BLT signed less (行 306) ✅ 已修复

**测试代码**:
```cpp
SECTION("BranchConditionUnit BLT signed less") {
    sim.set_input_value(unit.instance().io().branch_type, 4);  // BLT
    sim.set_input_value(unit.instance().io().rs1_data, 10);
    sim.set_input_value(unit.instance().io().rs2_data, 20);
    
    auto condition_val = sim.get_port_value(unit.instance().io().branch_condition);
    REQUIRE(condition_val == 1);  // ✅ 现在通过
}
```

---

### 失败 3: BLTU unsigned less (行 324) ✅ 已修复

**测试代码**:
```cpp
SECTION("BranchConditionUnit BLTU unsigned less") {
    sim.set_input_value(unit.instance().io().branch_type, 6);  // BLTU
    sim.set_input_value(unit.instance().io().rs1_data, 10);
    sim.set_input_value(unit.instance().io().rs2_data, 20);
    
    auto condition_val = sim.get_port_value(unit.instance().io().branch_condition);
    REQUIRE(condition_val == 1);  // ✅ 现在通过
}
```

---

## 🎯 根本原因 (已确认并修复)

### 问题位置

**文件**: `include/riscv/rv32i_branch_predict.h`  
**类**: `BranchConditionUnit`  
**方法**: `describe()`  
**行号**: 158-164 (修复前)

### 问题代码 (修复前)

```cpp
// ❌ 错误：使用无符号比较代替有符号比较
auto signed_less = (io().rs1_data < io().rs2_data);  // 临时简化
auto signed_ge = !signed_less;
```

**问题**: `ch_uint<32>` 的 `<` 运算符执行无符号比较，不能用于有符号数比较。

### 修复方案 (已实施)

```cpp
// ✅ 正确：使用符号位进行有符号比较
// 提取符号位
auto rs1_sign = bits<31, 31>(io().rs1_data);
auto rs2_sign = bits<31, 31>(io().rs2_data);
auto signs_equal = (rs1_sign == rs2_sign);

// 符号相同：直接比较（此时无符号比较 = 有符号比较）
// 符号不同：负数 < 正数 (rs1_sign=1, rs2_sign=0 时 rs1 < rs2)
auto same_sign_less = (io().rs1_data < io().rs2_data);
auto diff_sign_less = (rs1_sign == ch_uint<1>(1_d));  // rs1 为负，rs2 为正
auto signed_less = select(signs_equal, same_sign_less, diff_sign_less);
auto signed_ge = !signed_less;
```

### 修复逻辑

1. **符号相同** (`signs_equal = true`): 两个数同号，无符号比较结果 = 有符号比较结果
   - 例：`10 < 20` → `true` (都为正)
   - 例：`-20 < -10` → `true` (都为负，补码表示下无符号比较也成立)

2. **符号不同** (`signs_equal = false`):
   - `rs1` 为负 (`rs1_sign = 1`)，`rs2` 为正 (`rs2_sign = 0`) → `rs1 < rs2` 为 `true`
   - `rs1` 为正，`rs2` 为负 → `rs1 < rs2` 为 `false`

---

## ✅ 验证结果

### 修复后测试运行

```bash
$ cd /workspace/project/CppHDL/build && ctest -R test_rv32i_pipeline --verbose
...
100% tests passed, 0 tests failed out of 1

Label Time Summary:
base    =   0.06 sec*proc (1 test)

Total Test time (real) =   0.25 sec
```

### 相关测试

```bash
$ ctest -R "test_rv32i|test_branch"
100% tests passed, 0 tests failed out of 2

Test #58: test_branch_predict ..............   Passed
Test #59: test_rv32i_pipeline ..............   Passed
```

---

## 📝 修复日志

**修复日期**: 2026-04-09  
**修复人**: AI Agent  
**修复时间**: ~15 分钟  
**修改文件**: `include/riscv/rv32i_branch_predict.h` (第 156-169 行)  
**测试验证**: 26 个断言全部通过 ✅

---

## 🎓 经验总结

### 关键教训

1. **不要假设无符号类型可以做有符号比较** - `ch_uint<32>` 的 `<` 运算符永远是无符号比较
2. **有符号比较必须显式处理符号位** - 使用 `bits<31,31>()` 提取符号位
3. **使用 `select()` 进行条件选择** - 不能用三元运算符 `?:` 处理 `ch_bool` 条件
4. **测试覆盖重要边界** - BEQ/BLT/BLTU 分别测试了相等、有符号、无符号三种情况

### 复用模式

此有符号比较模式可复用于其他需要符号比较的场景:

```cpp
// 通用有符号比较模式
auto sign_a = bits<MSB, MSB>(a);
auto sign_b = bits<MSB, MSB>(b);
auto signs_same = (sign_a == sign_b);
auto same_sign_less = (a < b);  // 同号时无符号比较 = 有符号比较
auto diff_sign_less = sign_a;   // a 为负则 a < b
auto signed_less = select(signs_same, same_sign_less, diff_sign_less);
```

---

**分析人**: AI Agent  
**修复日期**: 2026-04-09  
**状态**: ✅ **已完成并验证**  

---

## 📊 测试结果

| 指标 | 结果 |
|------|------|
| 测试用例 | 12 |
| 通过 | 11 ✅ |
| 失败 | 1 ❌ (3 个断言失败) |
| 断言总数 | 26 |
| 通过断言 | 23 |

---

## 🔍 失败详情

### 失败 1: BEQ equal (行 273)

**测试代码**:
```cpp
SECTION("BranchConditionUnit BEQ equal") {
    sim.set_input_value(unit.instance().io().branch_type, 0);
    sim.set_input_value(unit.instance().io().rs1_data, 42);
    sim.set_input_value(unit.instance().io().rs2_data, 42);
    
    auto condition = to_bool(sim.get_port_value(unit.instance().io().branch_condition));
    REQUIRE(condition == true);  // ❌ 失败：false == true
}
```

**预期**: `condition = true` (42 == 42)  
**实际**: `condition = false`  
**问题**: BEQ (Branch if Equal) 逻辑错误

---

### 失败 2: BLT signed less (行 306)

**测试代码**:
```cpp
SECTION("BranchConditionUnit BLT signed less") {
    sim.set_input_value(unit.instance().io().branch_type, 4);  // BLT
    sim.set_input_value(unit.instance().io().rs1_data, 10);
    sim.set_input_value(unit.instance().io().rs2_data, 20);
    
    auto condition_val = sim.get_port_value(unit.instance().io().branch_condition);
    REQUIRE(condition_val == 1);  // ❌ 失败：0 == 1
}
```

**预期**: `condition = 1` (10 < 20 signed)  
**实际**: `condition = 0`  
**问题**: BLT (Branch if Less Than) 有符号比较逻辑错误

---

### 失败 3: BLTU unsigned less (行 324)

**测试代码**:
```cpp
SECTION("BranchConditionUnit BLTU unsigned less") {
    sim.set_input_value(unit.instance().io().branch_type, 6);  // BLTU
    sim.set_input_value(unit.instance().io().rs1_data, 10);
    sim.set_input_value(unit.instance().io().rs2_data, 20);
    
    auto condition_val = sim.get_port_value(unit.instance().io().branch_condition);
    REQUIRE(condition_val == 1);  // ❌ 失败：0 == 1
}
```

**预期**: `condition = 1` (10 < 20 unsigned)  
**实际**: `condition = 0`  
**问题**: BLTU (Branch if Less Than Unsigned) 无符号比较逻辑错误

---

## 🎯 根本原因分析

### 可能原因 1: 分支类型解码错误

```cpp
// 检查分支类型解码逻辑
auto is_beq  = (io().branch_type == 0);  // 000
auto is_bne  = (io().branch_type == 1);  // 001
auto is_blt  = (io().branch_type == 4);  // 100
auto is_bge  = (io().branch_type == 5);  // 101
auto is_bltu = (io().branch_type == 6);  // 110
auto is_bgeu = (io().branch_type == 7);  // 111
```

**验证**: 解码逻辑看起来正确

---

### 可能原因 2: 条件计算逻辑错误

**参考文件**: `include/riscv/rv32i_branch_predict.h`

**可能问题**:
1. `equal` 计算：`rs1_data == rs2_data`
2. `signed_less` 计算：有符号比较逻辑
3. `unsigned_less` 计算：无符号比较逻辑
4. 条件 OR 组合逻辑

---

### 可能原因 3: Bundle 连接问题

由于我们刚刚记录了 Bundle 连接的技术债务，分支条件单元的 Bundle 连接可能也受到影响。

**检查**: `rv32i_branch_predict.h` 中的 `branch_condition` 输出连接

---

## 🔧 修复计划

### 步骤 1: 调试分支条件单元 (2h)

```cpp
// 在测试中添加更多调试输出
std::cout << "branch_type=" << branch_type_val 
          << ", rs1=" << rs1_val 
          << ", rs2=" << rs2_val 
          << ", condition=" << condition_val << std::endl;
```

### 步骤 2: 检查 rv32i_branch_predict.h 实现 (2h)

- 验证 equal 计算逻辑
- 验证 signed_less 计算逻辑
- 验证 unsigned_less 计算逻辑

### 步骤 3: 添加回归测试 (1h)

- 为每个分支类型添加边界测试
- 测试有符号/无符号比较的边界情况

---

## 📋 相关文件

- `include/riscv/rv32i_branch_predict.h` - 分支条件单元实现
- `tests/test_rv32i_pipeline.cpp` - 测试文件
- `docs/problem-reports/bundle-connection-issue.md` - 相关技术债务

---

## 🎯 下一步

- [ ] 读取 rv32i_branch_predict.h 实现
- [ ] 定位具体错误行
- [ ] 修复并验证
- [ ] 添加回归测试

---

**分析人**: AI Agent  
**日期**: 2026-04-08  
**状态**: 🔴 分析中


---

## 🔍 更新分析：根本原因确认

**日期**: 2026-04-08 (更新)

### 确认的问题

**位置**: `include/riscv/rv32i_branch_predict.h:162-163`

**问题代码**:
```cpp
auto signed_less = signs_equal 
                   ? (sub_sign != ch_uint<1>(0))  // 符号相同：检查 a-b 的符号
                   : rs1_sign;                     // 符号不同：a 为负则 a < b
```

**根本原因**: 
- `signs_equal` 是 `ch_bool` 类型 (硬件信号)，不是原生 `bool`
- C++ 三元运算符`?:` 不支持`ch_bool` 条件
- 导致 `signed_less` 计算结果始终为 false

**影响范围**:
- BEQ (需要使用`equal`，也依赖三元运算符)
- BLT (依赖 `signed_less`)
- BLTU (依赖 `unsigned_less`，可能也有问题)

### 修复方案

**使用 `select()` 函数代替三元运算符**:

```cpp
// 修复 signed_less
auto signed_less = select(signs_equal,
                          (sub_sign != ch_uint<1>(0)),
                          rs1_sign);

// 修复 equal (如果需要)
auto equal = select((io().rs1_data == io().rs2_data),
                    ch_bool(true),
                    ch_bool(false));
```

---

**更新时间**: 2026-04-08  
**状态**: ✅ 根因确认，待修复

