---
name: stream-arbiter-lock-selects-channel-zero-2026-11
priority: P2
source: 路线 B 只读查重（dual-backend-parity route B · 4 问①）实测挖出
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-11-03
evidence: test:tests/chlib/test_stream_arbiter_lock_dual.cpp
---

# Improvement: `chlib::stream_arbiter_lock` 恒选通道 0（且输出**错误的数据**）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03，ADR-065 + change `chlib-arbiter-buildtime-eval-fix-2026-11` ⇒ 已归档 `openspec/changes/archive/2026-11-03-chlib-arbiter-buildtime-eval-fix-2026-11/`）
- **来源**: 路线 B 的 4 问① **只读查重**（`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4）
  ⇒ 逐个读 `include/chlib/*.h` 的实现时**发现**，并用**独立探针实测确认**。

## 1. Why (背景与动机)

`include/chlib/stream_arbiter.h:62-68` 的 `stream_arbiter_lock` 用**普通 C++ `if /
else if / else`** 来做**硬件选择**：

```cpp
ch_bool current_locked_valid = false;
for (...) { current_locked_valid = current_locked_valid | (is_locked & input_streams[i].valid); }
ch_bool has_valid = valid_vector != 0_d;

if (current_locked_valid) {          // ← 建图期求值
    selected_idx = locked_channel;
} else if (has_valid) {              // ← 建图期求值
    selected_idx = priority_encoder<N_INPUTS>(valid_vector);
    locked_channel = selected_idx;   // ← 对 ch_reg 的 C++ 赋值（非 ->next）
} else {
    selected_idx = locked_channel;   // ← 恒走这里
}
```

🔴 `ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:43-49`）**只对 const 节点返回真值，
非 const 节点恒返 0** ⇒ ⇒ `current_locked_valid` 与 `has_valid` 都是**非 const 的组合节点**
⇒ ⇒ **C++ `if` 在建图期恒假** ⇒ ⇒ **恒走 `else` 分支** ⇒ ⇒ `selected_idx = locked_channel`
（初值 0，且 `locked_channel` 从未被真正推进 ⇒ 恒 0）⇒ ⇒ **仲裁器永远选通道 0**。

⚠️ 这与仓内**已登记的缺陷家族**同型（AGENTS.md NOTES 里的「**条件量在【建图期】被求值**」）：
`_if` 语句块（ADR-051）· `uart_tx` 的 FSM（ADR-050）· cache wrapper 的 `if`（ADR-045 批）
· `add_with_carry` / `sub_with_borrow` 的 `cin` · `edge_detector` · `johnson_counter` ·
`fixed_point_divider` 的除零守卫。

### 实测证据（独立探针，`/tmp/opencode/probe_stream_arbiter.cpp`，不入仓）

3 个输入流，**只有通道 1 有效**（`valid=1`，payload `0x22`）：

```
[probe1] lock: selected=0 (期望 1)  out.valid=1  out.payload=0x11
```

⇒ ⇒ **`selected` 是 0**（应为 1）**且 `out.payload` 是 `0x11`**（**通道 0 的数据**！）
⇒ ⇒ 这**不只是「选错了通道」，而是「输出了错误通道的数据」** ⇒ ⇒ 下游拿到的是
**另一个通道的 payload** ⇒ ⇒ **静默数据损坏**（`out.valid` 仍是对的 ⇒ 更难发现）。

### 为什么既有测试没抓到（零判别力的证据）

`tests/test_stream_arbiter.cpp` 的 2 个 lock 用例**只断言结构宽度**
（`payload.width == 8` / `valid.width == 1` / `selected.width == ...`）与
`output_stream.valid == true` ⇒ ⇒ **`selected` 与 `output_stream.payload` 从未被断言**
⇒ ⇒ 「恒选通道 0」对既有测试**完全不可见** ⇒ ⇒ 长期「假绿」。

## 2. What Changes (变更范围)

- **`stream_arbiter_lock` 的选择逻辑 MUST 用硬件原语表达**（`select` 链 / 真 `&` / `|`）
  ⇒ ⇒ 典型形态：
  `selected_idx = select(current_locked_valid, locked_channel,
  select(has_valid, priority_encoder<N_INPUTS>(valid_vector), locked_channel));`
- **锁存 MUST 走寄存器通路**：`locked_channel` 是 `ch_reg` ⇒ MUST 用 `->next`
  （当前是 `locked_channel = selected_idx;` ⇒ **C++ 赋值** ⇒ 既不是寄存器写入，
  也**不是**数据流边 ⇒ ⇒ 锁存**永不生效** ⇒ 「锁定直到事务完成」这条**语义承诺**
  完全没有实现）。
- ⚠️ **ADR 需求**：这是**行为变更**（修好后仲裁结果会变）⇒ ⇒ 需独立 ADR
  （或并入既有的 stream 域 ADR）⇒ ⇒ **不在 parity 批里做**。

## 3. Acceptance (验收标准)

- [ ] `stream_arbiter_lock` 在「唯一有效输入 = 通道 k」时 `selected == k`
      且 `output.payload` == **通道 k 的 payload**（不是别的通道的）
- [ ] 锁定语义真的生效：通道 0 有效并被授予后，即使通道 1 变有效，
      在事务完成前 `selected` **仍为 0**
- [ ] 事务完成（`output.ready`）后锁释放 ⇒ 下一拍可重新选路
- [ ] **判别性用例**：既有 `tests/test_stream_arbiter.cpp` 的 2 个 lock 用例
      MUST 补上 `selected` / `output.payload` 的**行为断言**
      ⇒ ⇒ 否则修好后仍可能「绿但没测到」
- [ ] **变异证据**：把选择逻辑改回 C++ `if` ⇒ ⇒ 用例 MUST 转红
- [ ] 双后端 parity：修好后 `stream_arbiter_lock` 的语义 MUST 进
      `DUAL_BACKEND_TEST_PAIR`（本组件的 parity 覆盖**当前刻意不做** ⇒ 见 §5）

## 4. Capabilities (契约归属)

- 建议新建 capability `chlib-stream-arbitration`（或并入既有的
  `chlib-stream-arbitration` / `chlib-stream-ready-ownership`）⇒
  **MODIFIED 或 ADDED ×1**，含「仲裁结果 MUST 与 `valid` 向量一致」
  与「锁存 MUST 在事务完成前保持」两条 Requirement。
- ⚠️ **命名诚实性**：`chlib/AGENTS.md` 的命名登记表已收录 `stream_arbiter_round_robin`
  ⇒ 本组件（`stream_arbiter_lock`）**修好后** MUST 一并登记
  （「锁存直到事务完成」这条语义在修好前是**假的**）。

## 5. Impact (影响面)

- **生产消费者**：**0**（全仓只有 `include/chlib/stream_arbiter.h` 自身 +
  `tests/chlib/test_stream.cpp` + `tests/test_stream_arbiter.cpp`）
  ⇒ ⇒ **爆炸半径为零** ⇒ ⇒ 可以独立修，不牵连流水线。
- ⚠️ **本 parity 批刻意避开本组件**：dual parity 会把「恒选通道 0」这个缺陷
  **盖章**（两后端一致地错 ⇒ parity **看不见**）⇒ ⇒ 先修缺陷，再做覆盖。
- ⚠️ **同类风险**：`include/chlib/` 全头已扫（剥注释后逐行找 C++ `if`）⇒
  同家族**其余命中点**已核实为 **C++ 侧判断**（容器 `size()`、模板常量、节点守卫）⇒
  **无第二处同型缺陷**；`include/chlib/selector_arbiter.h:94`
  （`round_robin_arbiter` 不轮转）**是另一处**，见姊妹 draft
  `selector-round-robin-not-rotating-2026-11.md`。
