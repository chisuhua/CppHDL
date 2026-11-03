---
kind: registry
subject: hdl-logical-deprecation
evidence: probe:scripts/verify_deprecation_callsites.sh
---

# 登记表：HDL 逻辑算子（`&&` / `||`）的弃用状态

> ⚠️ **本文件是纯数据** ⇒ ⇒ 两个护栏（`scripts/verify_deprecation_callsites.sh`
> 与 `scripts/verify_deprecation_calendar_gate.sh`）都对它做**双向**核对 ⇒ ⇒
> 「登记与现实不一致」必红。
>
> 契约来源 = **ADR-036**（`docs/adr/ADR-036-deprecate-hdl-logical-operators.md`）
> §2（弃用范围）/ §4（测试文件策略）/ §8（移除门）。

## 1. 被弃用的 8 个 overload（定义点）

| # | 文件:行 | 形态 | 弃用消息 |
|---|---|---|---|
| 1 | `include/core/io_logical.h:73` | `port<ch_bool> && port<ch_bool>` | `use operator& for 1-bit AND; select() is for value muxing` |
| 2 | `include/core/io_logical.h:82` | `port<ch_bool> \|\| port<ch_bool>` | `use operator\| for 1-bit OR; select() is for value muxing` |
| 3 | `include/core/operators_logic.h:161` | `ch_bool && ch_bool` | `use operator& for 1-bit AND; select() is for value muxing` |
| 4 | `include/core/operators_logic.h:166` | `ch_bool \|\| ch_bool` | `use operator\| for 1-bit OR; select() is for value muxing` |
| 5 | `include/core/operators_logic.h:177` | `ch_bool && ch_uint<N>`（mixed，nonzero-test 归约） | `mixed && with ch_uint<N> uses nonzero test; write (rhs != 0) explicitly` |
| 6 | `include/core/operators_logic.h:184` | `ch_uint<N> && ch_bool`（mixed） | `mixed && with ch_uint<N> uses nonzero test; write (lhs != 0) explicitly` |
| 7 | `include/core/operators_logic.h:191` | `ch_bool \|\| ch_uint<N>`（mixed） | `mixed \|\| with ch_uint<N> uses nonzero test; write (rhs != 0) explicitly` |
| 8 | `include/core/operators_logic.h:198` | `ch_uint<N> \|\| ch_bool`（mixed） | `mixed \|\| with ch_uint<N> uses nonzero test; write (lhs != 0) explicitly` |

⚠️ **mixed 那 4 条的语义警告**：`true && 0b10` 返回 **1**（nonzero-test 归约），而
`true & 0b10` 返回 **0** ⇒ ⇒ **naive 的 `&&`→`&` 重写会静默改硬件语义** ⇒ ⇒
第 2 节的豁免 MUST 保留到物理移除那一刻。

## 2. 允许存在的调用点（豁免登记 —— MUST 与现实逐条相等）

| # | 文件:行 | 为什么必须保留 | 移除时的处置 |
|---|---|---|---|
| 1 | `tests/test_chipforge_mux_segv_repro.cpp:226` | 复现 `ch_bool(false) && (a != 0)` 的 null-impl SEGV；改成 `&` 会**静默改掉被测行为** ⇒ 回归用例失效 | 改为显式形式并保留 SEGV 覆盖 |
| 2 | `tests/test_chipforge_mux_segv_repro.cpp:238` | 同上（`we && (rd_addr == ai)`） | 同上 |
| 3 | `tests/test_chipforge_mux_segv_repro.cpp:281` | 同上（第二个 SEGV 复现点） | 同 |
| 4 | `tests/test_operator_advanced.cpp:540` | mixed overload 的 **nonzero-test 归约**语义覆盖（`bool_val && uint_val`）⇒ 改 `&` 会**丢掉归约** ⇒ 语义被改 | 改为显式 `(bool_val != 0_d) & uint_val` |
| 5 | `tests/test_operator_advanced.cpp:541` | 同上（`uint_val && bool_val`） | 同上 |
| 6 | `tests/test_operator_advanced.cpp:542` | 同上（`bool_val \|\| uint_val`） | 同上 |
| 7 | `tests/test_operator_advanced.cpp:543` | 同上（`uint_val \|\| bool_val`） | 同上 |

⚠️ 这 7 处**各自**被 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`
包住（`tests/test_chipforge_mux_segv_repro.cpp:126` ·
`tests/test_operator_advanced.cpp:539`）⇒ ⇒ 编译零警告，但**覆盖仍在**。

## 3. 移除门（calendar gate —— ADR-036 §8）

| 项 | 取值 |
|---|---|
| deprecation 落地日 | **2026-10-16** |
| 最早允许移除日 | **2027-04-17** |
| 低活动顺延（§8.2） | 移除日之前 `[2026-10-16, 移除日]` 的 commit 数 **< 5** ⇒ 顺延 **3 个月** ⇒ **2027-07-17** |
| 移除时必须一并改的 | 第 1 节的 8 个 overload + 第 2 节的 7 处豁免调用点 |

⚠️ **日历锁必须由机械信号守住**（ADR-036 §8.1 的原文是「拒绝任何移除 commit」）⇒ ⇒
`scripts/verify_deprecation_calendar_gate.sh` 负责：标记仍在 ⇒ 绿；标记已消失 ⇒
判定移除日期（提交日期 / 未提交 ⇒ 今天）⇒ 早于生效期限 ⇒ **红**。
