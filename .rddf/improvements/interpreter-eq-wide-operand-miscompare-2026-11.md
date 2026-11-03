---
name: interpreter-eq-wide-operand-miscompare-2026-11
priority: P1
source: ADR-065 实施期实测挖出（chlib 仲裁器族修复过程中）
phase: phase-5
category: framework
type: bugfix
created_at: 2026-11-03
---

# Improvement: interpreter 的 `eq` 在「宽操作数 vs 窄操作数」且**移位量 ≥ 2** 时**判错**

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 **待实施**（ADR-065 的 change **只绕开**、**不**修框架 ⇒ 见 §5）
- **来源**：ADR-065（`chlib-arbiter-buildtime-eval-fix-2026-11`）实施期 —— 修
  `round_robin_arbiter` 时发现「N=64 时 `next_ptr` 恒 0」，逐层二分后定位到
  **框架层的 `eq`**。

## 1. Why (背景与动机)

### 1.1 现象

`chlib::round_robin_arbiter<64>`（selector 层轮转原语）在 **N=64** 时**不轮转**：
唯一有效 = bit `k`（`k ≥ 2`）⇒ `next_ptr` **恒 0**（期望 `k+1`）。
而 `k = 0` / `k = 1` **正常** ⇒ ⇒ 与「移位量」强相关。

### 1.2 最小复现（**独立探针**，不入仓）

`/tmp/opencode/probe_eq_wide.cpp`：把「64 位值 `a`」与「`ch_uint<64>(1) << pos`」
（`pos` 是 6 位）直接比较：

```
pos=0 a=0x1   shl=0x1    eq_wide=1 bit_sel=1 OK
pos=1 a=0x2   shl=0x2    eq_wide=1 bit_sel=1 OK
pos=2 a=0x4   shl=0x4    eq_wide=0 bit_sel=1 **FAIL**   ← shl 值正确，eq 判错
pos=3 a=0x8   shl=0x8    eq_wide=0 bit_sel=1 **FAIL**
pos=4 a=0x10  shl=0x10   eq_wide=0 bit_sel=1 **FAIL**
pos=5 a=0x20  shl=0x20   eq_wide=0 bit_sel=1 **FAIL**
```

⇒ ⇒ **`shl` 的值是对的**（`0x4`），**错的是 `eq`** ⇒ ⇒ 与 `shl` 无关，
**与 `eq` 的两个操作数位宽不同有关**（64 位 vs **127 位**）⇒ ⇒ 且**只在
移位量 ≥ 2 时**触发。

### 1.3 为什么 127 位

`ch_uint<N>(1_d) << pos` 的**结果宽度 = N + (N-1)**（桶形移位的全量输出宽度）
⇒ N=64 ⇒ **127 位**。生成 Verilog 里可直接看到：

```verilog
logic [126:0] shl_127;
assign shl_127 = uint_lit_129 << bits_254;   // uint_lit_129 = 64'h1
assign eq_eq_63 = mux_select_126 == _shl_127;  // 64 位 vs 127 位
```

⇒ ⇒ **Verilog 侧是零扩展比较 ⇒ 正确**；**interpreter 侧判错** ⇒ ⇒
**这是两后端的行为分歧**（⇒ 双后端 parity 会把它抓出来 —— 本次是用
**单后端探针**发现的，因为 `round_robin_arbiter` 当时还没进 dual 覆盖）。

### 1.4 影响面（**只登记已确认的，不做全仓普查**）

- **已确认受害者**：`chlib::round_robin_arbiter<64>`（ADR-065 已**绕开**：
  改用 `bit_select(grant, pos)` ⇒ 不再产生 127 位中间节点）。
- ⚠️ **同型风险面**：任何 `ch_uint<N> == (ch_uint<N>(1) << <runtime pos>)`
  的写法（N ≥ 32 ⇒ 中间节点 ≥ 63 位 ⇒ 两操作数位宽不同）。
  ⚠️ **本 change 未做全仓普查**（B8 已否决「全仓普查」义务）⇒ ⇒
  **影响面未知**，只登记本条。

## 2. What Changes (变更范围 —— **待 architect 裁**）

- **定位**：`src/ast/instr_op_*.cpp` 的 `eq` 实现 + `include/bv/bv_compare.h`
  的 `bv_eq`（**位宽不同**时的比较语义）。
- **候选修法**（**MUST 由 architect 裁决**，本 improvement 只登记）：
  - A. `bv_eq` 在**位宽不同**时按**较大宽度零扩展**比较（与 Verilog 一致）⇒
    **语义修正**（⇒ 现有依赖旧行为的代码行为会变 ⇒ 需 blast radius 评估）；
  - B. `eq` 指令在**建图期**把两个操作数**统一到同一宽度**（避免运行期位宽差）⇒
    **零行为变更**（⇒ 但改变了生成的 Verilog ⇒ RTL diff 门）；
  - C. 文档化「`eq` 的两操作数 MUST 同宽」⇒ **零代码变更**，但把坑变成契约
    （⇒ 与 ADR-053 的「入口集合」思路一致：把不可判定变成可判定）。
- ⚠️ **MUST NOT** 只在 `chlib` 层绕开而不留记录 —— 那会让**下一个**同型用法
  继续踩坑 ⇒ ⇒ 本条 improvement **必须存在**。

## 3. Acceptance (验收判据)

- [ ] 最小复现（§1.2）**修后** `eq_wide == 1`（`pos = 0..5` 全部）
- [ ] **两后端一致**：同一表达式在 interpreter 与 Verilator 下给出**相同**结果
      （⇒ **必须**有 dual parity 判据 ⇒ ⇒ 这是本条的**核心**验收点）
- [ ] 既有 `ctest -LE perf` **零回归**
- [ ] 若选 A/B ⇒ 生成 Verilog **逐字节 diff** 已评估（⇒ RTL 变化的 blast radius）

## 4. Impact

- **爆炸半径**：目前**已确认** 1 个消费者（`round_robin_arbiter<64>`，已绕开）；
  潜在面 = 所有「宽位宽移位结果参与比较」的写法（**未普查**）。
- **风险**：若选 A（语义修正）⇒ 任何依赖旧（错误）行为的代码行为会变 ⇒
  需全仓回归；选 B ⇒ 生成 Verilog 变 ⇒ **golden file 变** ⇒ 需 ADR。
- **不做**：全仓普查「宽移位 + 比较」的用法（**代价高、收益低**）⇒
  改为「修好 `eq` 本身 ⇒ 全部受益」。

## 5. 备注（为何 ADR-065 没有顺手修）

ADR-065 是**仲裁器族**的缺陷修复 change ⇒ 修**框架层**的 `eq` 会把爆炸半径从
「两个 chlib 组件」扩大到「全仓所有比较运算」⇒ ⇒ 属**另一个决策**（需要 ADR
与 blast radius 评估）⇒ ⇒ ADR-065 **只绕开**（`bit_select` 等价形式），
并把框架缺陷**独立立项**（本文件）。
