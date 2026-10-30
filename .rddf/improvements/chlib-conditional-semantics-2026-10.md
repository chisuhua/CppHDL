---
name: chlib-conditional-semantics-2026-10
priority: P1
source: ADR-051（chlib 条件构造的语义模型 —— 守卫贡献 + 权威表面）
phase: phase-5
category: chlib
type: feature
created_at: 2026-10-30
---

# Improvement: `_if` 语句块 DSL MUST 兑现它的名字（条件被彻底忽略 ⇒ 守卫贡献）

- **Date drafted**: 2026-10-30
- **Drafted by**: build-agent（planner role）
- **Status**: ✅ **已实施 + 已归档**（2026-10-30，commit `102f39b`）⇒ 归档目录
  `openspec/changes/archive/2026-10-30-chlib-conditional-semantics-2026-10/`
  ⇒ capability **新建** `chlib-conditional-semantics`（ADDED ×6，归档**未**用
  `--skip-specs`）⇒ ⇒ 验收：`ctest -LE perf` **191/191** · examples **28/28** ·
  samples **36/36** · 护栏全 exit 0 · **变异 6/6** 全被抓
  ⚠️ **实施期实测订正**（3 条，见归档 change 的 `design.md` §5.5 / §5.6 / §6.1）：
  ① 「计数会变」是**事实错误**（⇒ 真值：计数**全部不变**，202 / 38 / 191 / 17）；
  ② **M6 的初稿预测被证伪**（链序对 ADR-050 **不可观测** ⇒ 状态守卫**互斥**）；
  ③ **3 个独立缺陷**已登记（E1 / E2 / E3，见下）
  ⚠️ **后续**：优先级链序的契约缺口 ⇒ 独立 change
  `chlib-guard-contribution-priority-2026-10`
- **归属 feature**：`feat-dual-backend-coverage`（覆盖质量族）—— ⚠️ 该 feature 的
  主题是「双后端覆盖」，与本项**不**直接相关 ⇒ **待裁决**：或新开 feature，或归
  `feat-riscv-mini-rtl-debt`（不对）⇒ ⇒ **本项暂挂在 phase-5**，归属待 planner 裁决。

## 1. Why (背景与动机)

`include/chlib/if_stmt.h` 的 `_if(cond).then(body).elif(cond).else_().end()` 是仓内
**唯一**的「语句块风格」条件构造，已由 `include/chlib.h` 聚合器导出、被
`tests/chlib/test_if_stmt.cpp`（**10** 用例，已注册进 CTest）覆盖 ⇒ **已发布** API。

⚠️ 但它**双重失效** ⇒ **条件被彻底忽略**（源码逐行核实）：

| # | 机制 | 后果 |
|---|---|---|
| ① | `next_proxy::operator=` 的**条件分支体是空的** | 被 ② 掩盖 ⇒ 从未表现为「赋值丢失」 |
| ② | `end()` **先**置 `finalized_ = true` **再**遍历分支体 ⇒ 遍历期间 `is_active()` 恒 false ⇒ 走「直接赋值」分支 | ⇒ 条件块内的 `reg->next = v` **无条件生效** |

⇒ 写 `_if(rst).then([&]{ reg->next = 0_d; })` ⇒ `reg` **每拍**被清零
（与意图**相反**）⇒ ⇒ 这是「**名字不兑现语义**」的典型 —— 与 ADR-050 处理的 FSM
缺陷**同族**（那次是「闭包内的写入被覆盖」，这次是「条件被忽略」）。

⚠️ **测试钉住了坏行为** ⇒ 长期「假绿」：`tests/chlib/test_if_stmt.cpp:204-226`
的注释明写「This test pins down that current … behavior so future changes are
noticed」⇒ 任何**正确**的实现都会让它变红 ⇒ ⇒ 「修好了」与「测试红了」无法区分
⇒ 且同文件另两个用例只断言 **C++ 标志** ⇒ **零硬件覆盖**。

**Why now**：ADR-050 已把「守卫贡献」确立为仓内的**唯一**条件/状态合成机制 ⇒ ⇒
本项可**直接复用**该机制（共享组合器）⇒ ⇒ 成本比当初低一个量级。

## 2. What Changes (变更范围)

按 **ADR-051** 的 D1–D10：

- **D1** 语句块 = **守卫贡献**（条件守卫的组合贡献）⇒ 框架合成**一条** select 链
  ⇒ 写入动作由框架在**唯一一处**完成 ⇒ `ch_reg` 单 next 源契约零变化、`core/` 零改动；
- **D2** 优先级 = **先匹配者胜**（与 `multi_if` 的**优先级顺序**等价）；
- **D3** 默认尾部 = 寄存器**保持** / 输出**必须**有 `else_` 候选值
  ⇒ 缺失即**构建期拒绝**（**target 级**判据）；
- **D4** 拦截点 = `conditional_assign(target).next() = value`（**改名**自
  `conditional_reg_assign`，统一 `ch_reg` / `ch_out`）；
- **D5** `end()` = **捕获**（遍历所有分支体收集候选值）⇒ `finalized_` MUST 在遍历
  **之后**置位 ⇒ 分支体在**建图期**执行 ⇒ MUST 无副作用；
- **D6** 条件块内的**普通** `reg->next = v` / `io().x = v` **不被条件化**
  ⇒ ⇒ MUST 有**词法级静态护栏**（复用 `scripts/lib/fsm_wiring.py` 的**词法器 +
  花括号配平**；其 `find_closures` 的入口表是 `on_active(` 专用 ⇒ 新增 `.then(`/`.else_(`）；
- **D7** 嵌套 ⇒ active block MUST 是**栈**；
- **D8** `priority_if` / `priority_if_then`（与 `multi_if` **逐行等价** ⇒ 名字
  over-promise）⇒ **物理移除**；
- **D9** capability = **新建** `chlib-conditional-semantics`；
- **D10** 与 ADR-050 抽取**共享**「守卫贡献」组合器（一处实现、两处使用）
  ⇒ 约束：ADR-050 的既有金标 MUST **逐项不变**（纯重构）。

⇒ **顺带**：`samples/conditional_demo.cpp` 归位（它用的是**已不存在**的
`seq_if` / `reg_if` / `.endif()` API ⇒ 需**重写**）⇒ 从 `samples/disabled_samples.txt`
移除该行。

⚠️ **不在范围**：`samples/if_demo.cpp` 的归位（卡在 `ch_in<ch_bool>` **不能**绑到
`const ch_bool&` ⇒ 框架层绑定能力 ⇒ **独立立项**）。

## 3. Acceptance (验收标准)

- [ ] `_if(cond).then([&]{ conditional_assign(reg).next() = V; })` ⇒ **`cond` 为假时
      `reg` 不被写**（**硬件级**断言，非 C++ 标志）；
- [ ] 多分支 ⇒ **先匹配者胜**（`c0` 与 `c1` **同时**为真 ⇒ 取 `c0`）⇒ 与 `multi_if`
      的**优先级顺序**等价；
- [ ] 无 `else_` 的**寄存器** ⇒ 未匹配时**保持**；
- [ ] **输出端口缺 `else_` 候选值** ⇒ **构建期抛异常**（`REQUIRE_THROWS_AS`）
      ⇒ **不**是组合环 CHFATAL；
- [ ] `conditional_assign` 对 `ch_reg` **与** `ch_out` 都可用；
- [ ] **嵌套** `_if` ⇒ 内层先合成一条表达式再作为外层候选值；
- [ ] `conditional_reg_assign` **不再存在**（编译期断言：无可用重载）；
- [ ] `priority_if` / `priority_if_then` **不再存在**（编译期断言）；
- [ ] 新护栏 `scripts/verify_if_stmt_wiring.sh` 主路径绿 + `--self-test` 全绿
      （含「闭包内未走 `conditional_assign` ⇒ 违规」与「走了 ⇒ PASS」两条**对照**）
      ⇒ 且对**真实历史形态**有效（`samples/conditional_demo.cpp:63-78` 的裸
      `reg_count->next =`）；
- [ ] **变异 6/6 全被抓**（每条先确认编译成功）：M1 `finalized_` 置回遍历之前 ·
      M2 捕获分支改回空实现 · M3 `_if` 侧的**贡献收集顺序**反转 · M4 去掉输出的
      「缺 `else_` ⇒ 抛异常」+ 默认退化为 `0` · M5 嵌套不再委托给外层 ·
      M6 **共享组合器**的链构造方向反转 ⇒ **`_if` 侧**「先匹配者胜」转红。
      ⚠️ 初稿的「M6 ⇒ **ADR-050 金标转红**」**被实测证伪** ⇒ 状态守卫**互斥**
      ⇒ 链序对 ADR-050 **不可观测** ⇒ 「纯重构」的证据 MUST 是「**金标逐项不变**
      （绿灯）」⇒ 详见 change 的 `design.md` §6.1；
- [ ] ADR-050 的既有金标**逐项不变**（`uart_tx` 的 `busy` 高 **3906** 拍 + `0xA5`
      位序；`complex_e2e` 的 `0x96` MSB-first 16 位 + 状态序列 + `bit_count` 复位）；
- [ ] `samples/conditional_demo.cpp` 归位并注册进 `CH_SAMPLE_TARGETS` ⇒ 从
      `samples/disabled_samples.txt` 移除；
- [ ] **计数锚点：全部**不变**（⚠️ 本草案初稿写的是「`CTEST_TOTAL` 202 → 203、
      `EXAMPLES_TEST_SOURCES` 17 → 18」⇒ **事实错误**，实施期已订正）：
      - `CTEST_TOTAL` = **202 不变** —— `tests/chlib/test_if_stmt.cpp` 是**既有**、
        **已注册**的 ctest target ⇒ 一个 Catch2 二进制 = **1** 个 ctest 条目
        ⇒ 用例数增加但**条目数不变**；
      - `EXAMPLES_TEST_SOURCES` = **17 不变** —— 本项新增的是 **samples/** 下的
        sample（`samples/conditional_demo.cpp`）⇒ 而该锚点的口径是
        **`examples/**` 下**的测试源文件数 ⇒ **不**受影响；
      - `verilator_e2e` = **38 不变**；non-perf = **191 不变**；
      - **变**的只有 **samples 注册数**：已注册 **35 → 36**、禁用 **5 → 4**
        （`samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS` +
        `samples/disabled_samples.txt`）⇒ 该数**不是** CI 锚点，但
        `scripts/run_all_samples.sh` 的 **C2 数量守恒**会校验
        （实测 36 == 36 ✅）。

## 4. Capabilities (涉及的能力维度)

- **新建** `chlib-conditional-semantics`（ADDED ×N）⇒ 归档**禁** `--skip-specs`
  （ADR-051 D9）；
- **不**动 `chlib-state-machine-semantics`（那是 FSM 的契约；条件构造是**组合**语义
  ⇒ 混在一起会让该 capability 边界失焦 ⇒ ADR-051 D9）；
- **零** `core/` 改动 ⇒ `dual-backend-test-coverage` / `verilator-e2e-coverage` 等
  **零 MODIFIED**。

## 5. Impact (影响面分析)

- **改动文件**：`include/chlib/if_stmt.h`（+ 共享组合器新头 · `include/chlib/if.h`
  移除 `priority_if` · `tests/chlib/test_if_stmt.cpp` 重写 · 新护栏
  `scripts/verify_if_stmt_wiring.sh` + `scripts/lib/` 分析器 · `samples/conditional_demo.cpp`
  重写 · `samples/disabled_samples.txt` · `samples/CMakeLists.txt` ·
  `include/chlib/AGENTS.md` + capability delta + 根 `AGENTS.md` 计数锚点）；
- **爆炸半径**：**极小** —— 活消费者 **0**（`_if` / `conditional_reg_assign` /
  `if_then` / `priority_if*` 的全部消费者只有本测试文件 + 2 个**禁用** sample）
  ⇒ 但 D10 会**触碰 ADR-050 的已落地实现** ⇒ 爆炸半径的第二段在 `chlib/state_machine.h`
  ⇒ ⇒ 由 ADR-050 的**既有金标**逐项守住；
- **计数**：**会变**（新增用例 + 新 sample ⇒ `CTEST_TOTAL` 202→203 /
  `EXAMPLES_TEST_SOURCES` 17→18）⇒ MUST 同步锚点；
- **风险**：**中** —— ① D10 的抽取若改变链构造顺序 ⇒ ADR-050 金标变化（由金标把关）
  ⚠️ **实施期实测修正**：链序对 ADR-050 **不可观测**（状态守卫**互斥**）⇒ ⇒ 「纯重构」
  的证据是「**金标逐项不变**（绿灯）」，**不**可能是「链序变异下金标转红」；
  ⚠️ **另两条实测教训**（**变异工具链**自身，详见 change 的 `design.md` §6.2）：
  ② 「还原源码」**不等于**「复原」—— **二进制也是状态** ⇒ 变异工具链 MUST 在
  还原后**重建**（否则后续跑的是上一条变异的二进制 ⇒ **假红**）；
  ③ `build_targets` MUST 覆盖**每一个**被观测的测试（否则「观测」退化成「看旧结果」）；
  ② **析构兜底 × 抛出路径**：`~conditional_block()` 兜底调 `end()` ⇒ 新增的构建期
  抛异常经析构触发 ⇒ `std::terminate` ⇒ MUST `noexcept(false)` + 展开期不抛
  （ADR-050 实测③ 的同一陷阱）；③ 嵌套若 active block 不是栈 ⇒ 静默丢候选值；
  ④ 输出端口缺 `else_` 若不显式拒绝 ⇒ 组合环 ⇒ 拓扑排序 CHFATAL（**难**诊断）。
