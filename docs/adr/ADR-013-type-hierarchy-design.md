# ADR-013: 类型层次设计 — `ch_bool`、`ch_uint<N>`、`ch_sint<N>`、`ch_bits<N>`

**状态**: ✅ 已采纳  
**日期**: 2026-05-07  
**决策人**: Sisyphus + 用户  

---

## 1. 背景

CppHDL 的类型层次设计有三个待决议的问题：
1. `ch_bool` 是否应与 `ch_uint<1>` 合并或保持独立
2. 是否应引入 `ch_sint<N>` 有符号整数类型
3. 是否应引入 `ch_bits<N>` 纯位向量类型

当前类型体系的实现位置：
- `ch_bool` — `include/core/bool.h:28`，继承自 `logic_buffer<ch_bool>`
- `ch_uint<N>` — `include/core/uint.h:29`，继承自 `logic_buffer<ch_uint<N>>`
- `ch_sint<N>` — **不存在**
- `ch_bits<N>` — **不存在**

---

## 2. 分析过程

### 2.1 Q1：`ch_bool` vs `ch_uint<1>` 合并还是保持独立

#### 代码证据

**定义对比**：
```cpp
// bool.h:28-30
struct ch_bool : public logic_buffer<ch_bool> {
    static constexpr unsigned width = 1;
    static constexpr unsigned ch_width = 1;

// uint.h:29-31
template <unsigned N> struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;
```

**双向转换桥接**（`uint.h:40-51, 89-92`）：
- `ch_uint<1>` 可从 `ch_bool` 构造（requires N==1）
- `ch_uint<1>` 有 `operator ch_bool()` 转换运算符

**已存在的 `common_type` 等价处理**（`switch.h:35-41`）：
```cpp
struct common_type<ch_uint<N>, ch_bool> {
    using type = ch_uint<(N > 1 ? N : 1)>;  // ch_bool 可视为 ch_uint<1>
};
```

**使用统计**：
| 类型 | 出现次数 | 典型用途 |
|------|---------|---------|
| `ch_bool` | ~1765 处 | 控制信号（valid/ready/fire/stall），条件判断 |
| `ch_uint<1>` | 零星出现 | 1-bit 数据值 |

**Bundle 中的一致性模式**：AXI 和 Stream Bundle 全部使用 `ch_bool` 表示单比特信号（`axi_bundle.h:24-26`、`stream_bundle.h:21-22`），从未使用 `ch_uint<1>`。

#### 分析

| 因素 | 评估 |
|------|------|
| **语义区分** | `ch_bool` 代表控制逻辑，`ch_uint<1>` 代表 1-bit 数据值 — 是有意为之的设计分层 |
| **操作符负担** | 仅 8 个 bool 专用重载（`&&`, `||`, `!` 及混合重载），负担很小 |
| **SpinalHDL 先例** | Bool 和 UInt(1) 在 SpinalHDL 中是独立类型，有不同操作集 |
| **迁移成本** | 1765 处 `ch_bool` 使用，合并需全部审查，收益有限 |
| **共同操作** | `common_type` 已桥接两者，混合表达式中无需用户关心 |

#### 建议

**保持独立，不合并**。`ch_bool` 代表控制逻辑语义，`ch_uint<N>` 代表算数/数据语义，两者层次不同。双向转换桥接已足够满足互操作性需求。

---

### 2.2 Q2：是否引入 `ch_sint<N>` 有符号类型

#### 代码证据

**当前有符号处理能力**：
- `sext` 存在（`operators.h:545-558`）但返回 `ch_uint`，丢失有符号语义
- `sshr`（算术右移）已在 `ch_op` 枚举中注册（`lnodeimpl.h:60`）但未完全集成到操作符系统
- 所有比较操作（`<`, `<=`, `>`, `>=`）使用无符号语义

**缺失的场景**：
- 有符号比较（`slt`, `slti` RV32I 指令）
- 算术右移（符号位扩展）
- 有符号乘除法（RV32I M-extension）
- 正确的符号扩展传播

#### 分析

| 因素 | 评估 |
|------|------|
| **当前影响** | 无符号比较对地址、计数器、位宽等大部分场景足够 |
| **何时需要** | RV32I M-extension、有符号分支判断、有符号 DSP 运算 |
| **工作量** | 中等偏高：新类型定义 + 有符号比较操作符 + 算术移位 + 类型转换 + 测试 |
| **IR 准备度** | `sshr` 已在 `ch_op` 中，表明已有预先设计考量 |

#### 建议

**需要时再添加，当前优先级不高**。`sshr` 已注册在 IR 中表明前瞻性设计已到位，但在出现明确的有符号运算需求前不必投入实现。主要触发条件为 RV32I M-extension 或 DSP 类算法模块的开发。

---

### 2.3 Q3：是否引入 `ch_bits<N>` 纯位向量类型

#### 分析

| 因素 | 评估 |
|------|------|
| **当前角色** | `ch_uint<N>` 已同时承担算数和位操作角色 |
| **SpinalHDL 参考** | Bits（无算术）、UInt（无符号算术）、SInt（有符号算术）— 三种类型 |
| **额外负担** | 新类型引入 ~10 个操作符重载 + 类型转换 + Bundle 修改 |
| **实际价值** | 防止对"纯位"值进行意外算术运算 — 有益但非紧急 |

#### 建议

**低优先级，暂不添加**。当前阶段 `ch_uint<N>` 足以同时服务位向量和算术整数角色。引入 `ch_bits<N>` 的收益（防止误用）远小于实现成本。等待有明确的反射或类型安全需求时再行考虑。

---

## 3. 决议

| 问题 | 决议 | 理由 |
|------|------|------|
| **Q1**：`ch_bool` vs `ch_uint<1>` | **保持独立** | 语义分层清晰；双向转换桥接已满足互操作性；SpinalHDL 先例一致 |
| **Q2**：`ch_sint<N>` | **需要时添加** | `sshr` IR 前瞻性设计已到位；等待有符号运算的明确需求（如 RV32I M-extension） |
| **Q3**：`ch_bits<N>` | **低优先级，暂不添加** | `ch_uint<N>` 已满足当前全部需求；引入成本超过收益 |

**实施时间线**：无需立即代码修改。Q1 维持现状，Q2/Q3 等待需求驱动。

---

## 4. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Q1-Q3 分析与决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/bool.h:28-30` — `ch_bool` 定义
- `include/core/uint.h:29-31` — `ch_uint<N>` 定义
- `include/core/uint.h:40-51` — `ch_bool`→`ch_uint<1>` 构造
- `include/core/uint.h:89-92` — `ch_uint<1>`→`ch_bool` 转换
- `include/core/operators.h:545-558` — `sext` 实现
- `include/core/operators.h:821-856` — bool 专用逻辑操作符
- `include/core/lnodeimpl.h:60` — `sshr` IR 注册
- `include/chlib/switch.h:35-41` — `common_type` 桥接
- `include/bundle/axi_bundle.h:24-26` — Bundle 中 `ch_bool` 的使用
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #8
