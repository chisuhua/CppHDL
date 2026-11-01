# Design: dual-backend-parity-batch-b-2026-11

> **文档性质**：**推导与证据**（不写进 capability 正文）⇒ ⇒ 按 ADR 形态契约的
> 三层分离，本文件承载「时序推导 / 逐条证据 / 裁决表」，正文（契约面）只留结论。

## 1. 筛选推导（**为什么只有 1 个文件**）

| 候选 | 门槛①局部 DUT | 门槛②平凡断言 | 门槛③stdout | 结论 |
|---|---|---|---|---|
| `test_axi_pwm_simple` | ✔ 0 | ❌ **Simulation 用例只有 `REQUIRE(true)`**（且**自带 main** ⇒ 不可用 macro） | ✔ | ❌ |
| `test_bool_connection` | ❌ **4 个局部类** | ✔ | ✔ | ❌（需提升 DUT） |
| `test_operator_connection` | ❌ 4 | ✔ | ✔ | ❌ |
| `test_jit_concat` | ❌ **6** | ✔ | ✔ | ❌ |
| `test_interpreter_jit_compat` | ❌ 2 | ✔ | ✔ | ❌ |
| `chlib/test_assert` | ✔ **0** | ✔ | ❌ **2 个宏用例重定向 stdout** | ✅ **只转 4 个 `AssertChecker` 用例** |
| `test_wires_connection` | ❌ 6 | ✔ | ✔ | ❌ |
| `chlib/test_multi_if_acceptance` | ❌ 1 | ✔ | ✔ | ❌（另有 3 个 `REQUIRE_THROWS_AS` **构图期**拒绝 ⇒ ⇒ 结构性不可转） |

⚠️ **门槛①的连锁后果**（**本轮新识别的代价**）：局部类 DUT **无法**传给在
文件作用域展开的 `DUAL_BACKEND_TEST_PAIR` ⇒ ⇒ 必须提升到共享头
⇒ ⇒ ⇒ **⇒ 与「原文件 MUST NOT 修改」纪律【冲突】** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ 提升 = 「同一逻辑两处维护」的漂移风险** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 本 change 明确不做**（oracle 建议的**共享 fixture** +
**cache key 守恒**判据是**独立议题** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 不在本 change 顺手做**）。

## 2. 🔴 成本模型的**二次订正**（**推导**）

### 2.1 打点证据（假 HOME 全冷 · 4 pair · 逐 0.01 s）

```
[  0.02s] 写出 top.v / sim_main.cpp
[  0.23s] Verilator: Walltime 0.012 s (elab=0.000, cvt=0.004, bld=0.000)
[ 10.89s] VerilatorBackend: cached artifact at .../<key>
[ 10.89s] VerilatorBackend: dlopen'd .../libVtop.so, created Vtop
[ 10.90s] 第 2 个 pair：wrote top.v / sim_main.cpp
[ 11.11s] Verilator: Walltime 0.012 s
[ 21.74s] 第 2 个 pair：cached artifact
```

⇒ ⇒ **两段耗时分离**：
① **0.21 s** = 生成 Verilog 源码 + **verilator 的 elaboration**（walltime 0.012 s）
② **10.66 s** = **C++ 编译 + 链接 `libVtop.so`**（verilator 调 `g++`）

⇒ ⇒ ⇒ **⇒ 【根因】**：`DUAL_BACKEND_TEST_PAIR` 为**每个 pair**建独立的
`ch_device<DutType>` ⇒ ⇒ ⇒ **⇒ ⇒ 上下文的 verilog 源码不同
⇒ ⇒ ⇒ **⇒ ⇒ 缓存 key 不同 ⇒ ⇒ ⇒ 【每个 pair 一次编译】**

⇒ ⇒⇒ **⇒ ⇒ 修正后的成本表**：

| 成本项 | 单位 | 实测 |
|---|---|---|
| verilator 自身 JIT 编译 | 每 job 一次 | ~3 s |
| 每 pair 冷编译 + 链接（组合逻辑 DUT） | **每 pair** | **10.66 s** |
| 每 pair 冷编译（mem 类 DUT） | 每 pair | 72 s（上一批实测） |
| 全热 | 每 pair | ~0.15 s |

### 2.2 ⇒ ⇒ 对**判据**的连锁影响（**上一轮的理由是错的**）

⚠️ 上一轮（critic SHOULD_FIX #1）把判据泛化为「每 pair 恰 1 条 `REQUIRE`」，
**理由**写的是「同 DUT 的多个 pair **只编译一次** ⇒ ⇒ **拆 pair 不增加墙钟**」
⇒ ⇒ ⇒ **⇒ 【这个理由是错的】** ⇒ ⇒⇒ **拆 pair = 【增加】编译次数**
（每个 **+10.66 s**；mem 类 **+72 s**）⇒ ⇒⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 【断言强度 MUST NOT 被判据绑架】**

⇒ ⇒⇒ **⇒ ⇒ ⇒ 替代判据（已实测可行）**：**Catch2 的 XML reporter**
（`-r xml`）输出逐 `TestCase` 的 `<OverallResults successes="N"/>` ⇒ ⇒⇒
**⇒ ⇒ ⇒ 判据 = 「verilator 半边每个 TEST_CASE 的 `successes` == interpreter
半边对应 TEST_CASE 的 `successes`」** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 【对多断言 pair 也成立】** ✔

### 2.3 ⚠️ 一条**被否**的替代判据（**记录以防复发**）

| 候选 | 为什么不成立 |
|---|---|
| 「**TEST_CASE 数** == 2 × pair 数」 | 🔴 verilator 缺席时 macro 的 verilator 半边**仍然注册**（只 WARN + return）⇒ ⇒ ⇒ **用例数恒为 2 × pair ⇒ ⇒ 无判别力** |
| 「**总断言数** == 2 × pair 数」 | 🔴 Catch2 汇总**不区分 per-TEST_CASE** ⇒ ⇒ ⇒ 多断言批次下**要么恒假红、要么无判别力** |
| 「每个 pair 恰 1 条 `REQUIRE`」 | 🔴 **代价高**（拆 pair = 增加编译）⇒ ⇒ ⇒ 绑架断言强度 ⇒ ⇒ **废止** |

## 3. 期望值的手推（**`AssertChecker` 是纯组合**）

`failed = enable & !condition` ⇒ ⇒ 4 个用例的真值表：

| 用例 | enable | condition | 手推 failed |
|---|---|---|---|
| ① fail | 1 | 0 | `1 ∧ ¬0` = **1** |
| ② pass | 1 | 1 | `1 ∧ ¬1` = **0** |
| ③ disabled | 0 | 0 | `0 ∧ ¬0` = **0** |
| ④ toggle | 拍1: 1 / 拍2: 1 | 拍1: 0 / 拍2: 1 | 拍1 **1** · 拍2 **0** |

⇒ ⇒ **全部手推** ⇒ ⇒ **不是**抄仿真输出 ✔

## 4. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-01 | 创建（筛选推导 + 成本模型二次订正 + 判据选型 + 手推真值表） |
