# CppHDL ch_op 操作符类型安全技能

## 触发条件

当 CppHDL 项目中出现以下问题时激活：
- 编译时操作符类型不匹配错误
- `ch_op::` 操作使用错误
- 模板元编程中的类型推导失败
- 操作符位宽计算错误
- 需要理解 ch_op 枚举和对应操作语义

## ch_op 操作符完整列表

### 算术操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::add` | 二元 | lhs, rhs | max(lhs, rhs) + 1 | 未考虑进位 |
| `ch_op::sub` | 二元 | lhs, rhs | max(lhs, rhs) + 1 | 未考虑借位 |
| `ch_op::mul` | 二元 | lhs, rhs | lhs + rhs | 位宽溢出 |
| `ch_op::div` | 二元 | lhs, rhs | lhs | 除零未处理 |
| `ch_op::mod` | 二元 | lhs, rhs | rhs | 除零未处理 |

### 逻辑操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::and_` | 二元/一元 | lhs, rhs | max(lhs, rhs) | 位宽不匹配 |
| `ch_op::or_` | 二元/一元 | lhs, rhs | max(lhs, rhs) | 位宽不匹配 |
| `ch_op::xor_` | 二元/一元 | lhs, rhs | max(lhs, rhs) | 位宽不匹配 |
| `ch_op::not_` | 一元 | operand | operand | 无 |

### 比较操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::eq` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |
| `ch_op::ne` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |
| `ch_op::lt` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |
| `ch_op::le` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |
| `ch_op::gt` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |
| `ch_op::ge` | 二元 | lhs, rhs | 1 | 有符号/无符号混淆 |

### 位移操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::shl` | 二元 | lhs, rhs | lhs + (2^rhs_width - 1) | 位移量 UB |
| `ch_op::shr` | 二元 | lhs, rhs | lhs | 逻辑/算术右移混淆 |
| `ch_op::sshr` | 二元 | lhs, rhs | lhs | 仅用于有符号数 |

### 位操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::bit_sel` | 一元 | operand, index | 1 | 索引越界 |
| `ch_op::bits_extract` | 一元 | operand, hi, lo | hi - lo + 1 | 范围越界 |
| `ch_op::bits_update` | 三元 | operand, value, index | operand | 位宽不匹配 |
| `ch_op::concat` | 二元 | lhs, rhs | lhs + rhs | 顺序错误 |

### 扩展操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::sext` | 一元 | operand | NewWidth | 新宽度 < 原宽度 |
| `ch_op::zext` | 一元 | operand | NewWidth | 新宽度 < 原宽度 |

### 规约操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::and_reduce` | 一元 | operand | 1 | 空操作数 |
| `ch_op::or_reduce` | 一元 | operand | 1 | 空操作数 |
| `ch_op::xor_reduce` | 一元 | operand | 1 | 空操作数 |

### 其他操作符
| 操作符 | 类型 | 操作数 | 结果位宽 | 常见错误 |
|--------|------|--------|---------|---------|
| `ch_op::mux` | 三元 | cond, true_val, false_val | max(true, false) | 条件非 1 位 |
| `ch_op::neg` | 一元 | operand | operand + 1 | 最小值取负溢出 |
| `ch_op::popcount` | 一元 | operand | ceil(log2(operand_width)) | 位宽计算错误 |
| `ch_op::assign` | 二元 | lhs, rhs | lhs | 位宽不匹配 |

## 常见错误模式

### 错误 1: 位移操作 UB

```cpp
// ❌ 错误：位移量 >= 32 时 UB
return (1 << ch_width_v<RHS>) - 1;

// ✅ 正确：使用 64 位类型
return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
// 或使用 1ULL
return (1ULL << ch_width_v<RHS>) - 1;
```

**参考技能**: `cpphdl-shift-fix`

### 错误 2: 符号/零扩展位宽错误

```cpp
// ❌ 错误：新宽度小于原宽度
auto extended = sext<8>(value);  // value 是 16 位

// ✅ 正确：新宽度必须大于等于原宽度
auto extended = sext<32>(value);  // value 是 16 位
```

### 错误 3: 连接操作顺序错误

```cpp
// ❌ 错误：期望 {high, low} 但顺序反了
auto result = concat(low, high);  // 实际是 {low, high}

// ✅ 正确：明确顺序
auto result = concat(high, low);  // 结果是 {high, low}
```

### 错误 4: 位提取范围越界

```cpp
// ❌ 错误：hi < lo 或超出操作数位宽
auto bits = value.range(3, 8);   // hi < lo
auto bits = value.range(16, 8);  // 超出 16 位操作数

// ✅ 正确：确保 hi >= lo 且不超出位宽
auto bits = value.range(8, 3);   // [8:3] 共 6 位
```

### 错误 5: 比较操作有符号/无符号混淆

```cpp
// ❌ 错误：有符号数和无符号数直接比较
ch_int<32> signed_val(-1_d);
ch_uint<32> unsigned_val(100_d);
auto result = signed_val < unsigned_val;  // 结果不确定

// ✅ 正确：统一类型后比较
auto result = sext<33>(signed_val) < zext<33>(unsigned_val);
```

### 错误 6: 规约操作空操作数

```cpp
// ❌ 错误：0 位操作数的规约
ch_uint<0> empty_val;
auto result = and_reduce(empty_val);  // 未定义行为

// ✅ 正确：确保操作数非空
ch_uint<1> val(1_d);
auto result = and_reduce(val);
```

### 错误 7: MUX 条件位宽错误

```cpp
// ❌ 错误：条件不是 1 位
ch_uint<8> cond(5_d);
auto result = select(cond, a, b);  // cond 应该是 1 位

// ✅ 正确：条件是 1 位布尔值
ch_bool cond_bool = (cond != 0_d);
auto result = select(cond_bool, a, b);
```

## 执行流程

### 1. 识别问题操作符

```bash
cd /workspace/CppHDL

# 查找特定 ch_op 使用
grep -rn "ch_op::shl" include/ src/

# 查找所有操作符使用
grep -rn "ch_op::" include/core/operators.h
```

### 2. 检查位宽计算

```cpp
// 检查操作符位宽计算是否正确
template <ValidOperand LHS, ValidOperand RHS>
auto operator<<(const LHS &lhs, const RHS &rhs) {
    constexpr unsigned lhs_width = ch_width_v<LHS>;
    constexpr unsigned rhs_width = ch_width_v<RHS>;
    
    // 验证：结果位宽 = lhs_width + (2^rhs_width - 1)
    constexpr unsigned result_width = lhs_width + ((1ULL << rhs_width) - 1);
    
    // ... 构建操作
}
```

### 3. 应用修复

根据具体错误类型选择修复方案：
- **位移 UB**: 使用 `1ULL << N`
- **位宽不匹配**: 添加 `sext`/`zext`
- **范围越界**: 检查边界条件
- **类型混淆**: 统一有符号/无符号

### 4. 验证修复

```bash
cd /workspace/CppHDL/build

# 重新编译
cmake .. && make -j$(nproc)

# 运行测试
ctest -R operator -V
```

## 检查清单

修复后必须验证：
- [ ] 操作符类型与语义匹配
- [ ] 结果位宽计算正确
- [ ] 无位移 UB（位移量 < 64）
- [ ] 有符号/无符号类型统一
- [ ] 边界条件测试通过
- [ ] 编译无警告（`-Wshift-overflow=2`）

## 类型安全最佳实践

### 1. 使用类型概念约束

```cpp
// 定义操作数概念
template <typename T>
concept ValidOperand = requires(const T &t) {
    { t.impl() } -> std::convertible_to<lnodeimpl *>;
};

// 使用概念约束模板
template <ValidOperand LHS, ValidOperand RHS>
auto operator+(const LHS &lhs, const RHS &rhs) {
    // 类型安全保证
}
```

### 2. 编译时位宽检查

```cpp
// 使用 static_assert 检查位宽
template <unsigned NewWidth, ValidOperand T>
auto sext(const T &operand) {
    static_assert(NewWidth >= ch_width_v<T>, 
                  "sext: NewWidth must be >= operand width");
    
    // ... 实现
}
```

### 3. 使用 constexpr 计算

```cpp
// 编译时计算结果位宽
template <ValidOperand LHS, ValidOperand RHS>
constexpr unsigned add_result_width() {
    return std::max(ch_width_v<LHS>, ch_width_v<RHS>) + 1;
}

// 使用
template <ValidOperand LHS, ValidOperand RHS>
auto operator+(const LHS &lhs, const RHS &rhs) {
    constexpr unsigned result_width = add_result_width<LHS, RHS>();
    // ... 实现
}
```

## 相关技能

- `cpphdl-shift-fix`: 位移操作 UB 修复
- `cpphdl-sdata-bitwidth`: sdata_type 位宽计算（建议创建）
- `cpphdl-mem-init-dataflow`: 内存初始化数据流

## 历史案例

### 2026-04-01: 位移操作 UB 修复

**问题**: `operators.h:758` 中 `(1 << 32)` 触发 UB

**根本原因**: 
- `1` 是 32 位 int 类型
- 位移量 32 超出类型精度

**修复方案**:
```cpp
// 修复前
return (1 << ch_width_v<RHS>) - 1;

// 修复后
return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
```

**验证结果**: ✅ 编译通过，测试通过

### 2026-03-30: 符号扩展位宽错误

**问题**: `sext<8>(16_bit_value)` 编译失败

**根本原因**: 新宽度 8 小于原宽度 16

**修复方案**:
```cpp
// 修复前
auto extended = sext<8>(value);  // value 是 16 位

// 修复后
auto extended = sext<32>(value);  // 新宽度 >= 原宽度
```

**验证结果**: ✅ 编译通过

---

**技能版本**: v1.0  
**创建时间**: 2026-04-01  
**适用项目**: CppHDL, 及其他使用 CppHDL 操作符框架的项目  
**参考文件**: `include/core/lnodeimpl.h`, `include/core/operators.h`, `include/core/node_builder.h`
