# 提案：实施 ADR-051 —— chlib 条件构造的语义模型（守卫贡献 + 权威表面）

- **架构依据**：**ADR-051**（`docs/adr/ADR-051-chlib-conditional-semantics.md`，
  2026-10-30，critic `review_profile: arch` 第 1 轮 **APPROVE_WITH_CONDITIONS**，
  **0 BLOCKER**）
- **5 段草案**：`.rddf/improvements/chlib-conditional-semantics-2026-10.md`
- **规格影响**：**ADDED ×N**（**新建** capability `chlib-conditional-semantics`）
  ⇒ 归档**禁** `--skip-specs**
- **规模 / 风险**：**中**（公开 API 改名 + 移除；框架**语义**变更；活消费者 0）

## Why

`_if(cond).then(body).elif(cond).else_().end()` 是仓内唯一的「语句块风格」条件
构造，已由 `include/chlib.h` 聚合器导出、被 `tests/chlib/test_if_stmt.cpp`
（**10** 用例，已注册进 CTest）覆盖 ⇒ **已发布** API。

⚠️ 但它**双重失效** ⇒ **条件被彻底忽略**（源码逐行核实）：

| # | 机制 | 后果 |
|---|---|---|
| ① | `next_proxy::operator=` 的**条件分支体是空的** | 被 ② 掩盖 ⇒ 从未表现为「赋值丢失」 |
| ② | `end()` **先**置 `finalized_ = true` **再**遍历分支体 ⇒ 遍历期间 `is_active()` 恒 false ⇒ 走「直接赋值」分支 | ⇒ 条件块内的 `reg->next = v` **无条件生效** |

⇒ 写 `_if(rst).then([&]{ reg->next = 0_d; })` ⇒ `reg` **每拍**被清零（与意图相反）。

⚠️ **测试钉住了坏行为** ⇒ 长期「假绿」：`test_if_stmt.cpp:204-226` 的注释明写
「This test pins down that current … behavior so future changes are noticed」
⇒ 任何**正确**的实现都会让它变红 ⇒ 且同文件另两个用例只断言 **C++ 标志**
⇒ **零硬件覆盖**。

⇒ 与 ADR-050 处理的 FSM 缺陷**同族**（那次是「闭包内的写入被覆盖」，这次是
「条件被忽略」）⇒ ⇒ 处置 = **契约 + 框架能力 + 护栏**。

## What Changes

### 范围内

**commit 1 —— 共享组合器 + `_if` 的守卫贡献实现**

- **新增** `include/chlib/guard_contribution.h`：与 ADR-050 的 `contrib` **同型**的
  「累积 `(守卫, 值)` + 合成**一条** select 链」组合器 ⇒ **一处实现、两处使用**
  （ADR-051 D10）；
- **重构** `include/chlib/state_machine.h`：`contrib` / `resolve_*` 改用共享组合器
  ⇒ ⚠️ **纯重构**：既有金标 MUST **逐项不变**（`busy` 高 3906 拍 + `0xA5` 位序；
  `0x96` MSB-first 16 位 + 状态序列 + `bit_count` 复位）；
- **实现** `include/chlib/if_stmt.h` 的条件化：
  - `conditional_assign(target).next() = value`（**改名**自 `conditional_reg_assign`，
    统一 `ch_reg` / `ch_out`）⇒ 拦截点**在 `chlib` 层** ⇒ **`core/` 零改动**；
  - `end()` 的 `finalized_` MUST 在**遍历分支体之后**置位（机制修正）；
  - 优先级 = **先匹配者胜**；默认尾部 = 寄存器**保持** / 输出**必须**有 `else_`
    候选值（**target 级**判据 ⇒ 缺失即**构建期抛异常**）；
  - active block MUST 是**栈**（嵌套）；
  - 析构**兜底**调 `end()` ⇒ 抛出路径 MUST 兼容析构（`noexcept(false)` +
    展开期不抛 —— ADR-050 实测③ 的同一陷阱）；
- **重写** `tests/chlib/test_if_stmt.cpp`：**移除**钉住坏行为的用例 ⇒ 补
  **硬件级**断言（条件为假 ⇒ 寄存器**不被写**）。

**commit 2 —— 护栏 + 表面收敛 + sample 归位**

- **移除** `include/chlib/if.h` 的 `priority_if` / `priority_if_then`（与 `multi_if`
  **逐行等价** ⇒ 名字 over-promise ⇒ Zero-Debt + ADR-043 先例）；
- **新增** 静态护栏 `scripts/verify_if_stmt_wiring.sh`（**词法级** ⇒ 方向为
  「宁可漏报、不误报」）：检出条件块内**未走** `conditional_assign` 的
  `->next =` **与** `io().x =` ⇒ 分析器**复用** `scripts/lib/fsm_wiring.py` 的
  **词法器 + 花括号配平**（其 `find_closures` 的入口表是 `on_active(` 专用 ⇒
  **新增** `.then(` / `.else_(` 入口）；
- **归位** `samples/conditional_demo.cpp`（重写 + 注册进 `CH_SAMPLE_TARGETS` ⇒
  从 `samples/disabled_samples.txt` 移除）。

### 范围外

- **不改** `core/`（`ch_reg` / `ch_out` / `core/reg.h` **零改动**）；
- **不**动 `chlib-state-machine-semantics`（那是 FSM 的契约 ⇒ 边界失焦）；
- **不**归位 `samples/if_demo.cpp`（卡在 `ch_in<ch_bool>` **不能**绑到
  `const ch_bool&` ⇒ 框架层绑定能力 ⇒ **独立立项**）；
- **不**改表达式形式 `multi_if` / `if_then` 的行为（D2 只要求语句块形式与之
  **优先级顺序**等价）。

## Acceptance

验收判据 = ADR-051 §5 的**可勾选**清单（要点）：

- [ ] **条件化语义**：条件为假 ⇒ 寄存器**不被写**（**硬件级**）；多分支 ⇒
      **先匹配者胜**；无 `else_` 的寄存器 ⇒ **保持**；输出**缺 `else_` 候选值**
      ⇒ **构建期抛异常**；
- [ ] **拦截与捕获**：`conditional_assign` 对 `ch_reg` / `ch_out` 都可用；嵌套
      `_if` 内外两层都正确；`conditional_reg_assign` **不再存在**；
- [ ] **静态护栏**：主路径绿 + `--self-test` 全绿（含「未走 `conditional_assign`
      ⇒ 违规」与「走了 ⇒ PASS」两条**对照**）+ 对**真实历史形态**有效；
- [ ] **变异 6/6 全被抓**（每条先确认编译成功）；
- [ ] ADR-050 的既有金标**逐项不变**；
- [ ] `samples/conditional_demo` 归位并被运行护栏执行；
- [ ] **计数锚点：全部**不变**（`CTEST_TOTAL` **202**、`EXAMPLES_TEST_SOURCES` **17**、
      non-perf **191**、`verilator_e2e` **38**）—— ⚠️ 本提案初稿写的是
      「202 → 203 / 17 → 18」⇒ **事实错误**，实施期已订正（`test_if_stmt` 是**既有**
      已注册 target ⇒ ctest **条目数**不变；新增的是 **samples/** 下的 sample ⇒
      `examples/**` 口径的锚点不变）⇒ ⇒ **只有 samples 注册数变**（35 → 36 / 5 → 4）。

## Capabilities

- **ADDED ×N**：**新建** `chlib-conditional-semantics` ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`include/chlib/guard_contribution.h`（新）· `include/chlib/if_stmt.h`
  （主）· `include/chlib/if.h` · `include/chlib/state_machine.h`（**纯重构**）·
  `tests/chlib/test_if_stmt.cpp` · `scripts/verify_if_stmt_wiring.sh`（新）·
  `scripts/lib/` 分析器 · `samples/conditional_demo.cpp` · `samples/CMakeLists.txt` ·
  `samples/disabled_samples.txt` · `include/chlib/AGENTS.md` · 根 `AGENTS.md` 计数锚点
- **计数**：**会变**（新增用例 + 新 sample）
- **爆炸半径**：**小**（活消费者 **0**）⇒ 但 D10 会触碰 ADR-050 的已落地实现 ⇒
  第二段爆炸半径在 `chlib/state_machine.h` ⇒ 由既有金标守住
- **风险**：**中** —— ① 抽取共享组合器若改变链构造顺序 ⇒ ADR-050 金标变化
  ② **析构兜底 × 抛出路径** ⇒ `std::terminate` ③ 嵌套若 active block 不是栈 ⇒
  静默丢候选值 ④ 输出缺 `else_` 若不显式拒绝 ⇒ 组合环 ⇒ 难诊断

## Non-Goals

- **不改** `core/` 的任何语义
- **不**泛化 ADR-050 的 `contrib` 为公开通用 API（共享的是**内部**组合器）
- **不**归位 `samples/if_demo.cpp`（框架层绑定能力 ⇒ 独立立项）
- **不**改 `multi_if` / `if_then` 的行为
