---
name: rtl-verification-single-source-2026-11
priority: P1
phase: phase-6
category: framework
type: refactor
状态: draft
进度: '**立项（2026-11-17）** · 未开工 · 🚪 开工前门 = **ADR-074 / ADR-075 已 Proposed（双审闭合 + 复审 APPROVE）** ⇒ 立项后回填 Accepted'
主题: RTL 验证的单一真相源 —— 判据单一语义（IR）+ 生成式 `*.sv` TB + 端口具名化 + 发射面收敛
依赖: ADR-072（Accepted · v2.2 · D4 被本项部分 supersede）· ADR-073（Accepted · v1.2）·
  ADR-074（**Proposed**）· ADR-075（**Proposed**）·
  examples-rtl-sim-coverage-2026-11（已归档）· rtl-productization-surface-2026-11（已归档）
来源: 用户 2026-11-17「我的疑问是为什么需要 emit_rtl.cpp 和 emit_rtl_soc.cpp 这两个文件和 *.sv 文件，为什么不是直接从对应的 *example.cpp 文件里生成 verilog dut 和 tb」+「希望项目代码不要过于碎片化」+「从 *example.cpp 构建端口连线 ⇒ 再创建提供测试激励 + 断言 + 期望值的 cpp ⇒ 提供激励的 cpp 可以考虑直接提供对应的 .sv TB；生成端口连线的 cpp 可以直接用库层提供的能力」
生成时间: 2026-11-17
evidence: probe:scripts/verify_examples_rtl_sim.sh
revision_count: 0
---

# Improvement: RTL 验证的单一真相源与生成式 TB

**主题**: 把 RTL 验证面的 **3 组同义载体**（期望值 / 端口身份 / 发射实现）收敛到单一权威，同时**不推翻**既有产品边界（ADR-072 四件套 · ADR-073 (A)/(B)/(C) 分工）。

---

## 架构依据

**唯一权威（决策面）**：
- `docs/adr/ADR-074-rtl-verification-single-source.md`（**Proposed** · oracle 6B + critic 4B ⇒ 全闭 · 复审 `APPROVE_WITH_CONDITIONS · 0 BLOCKER`）
- `docs/adr/ADR-075-port-naming-prerequisite.md`（**Proposed** · 同上批次）

**方案细则（推导 + 逐文件清单 + 分阶段）** = `docs/architecture/2026-11-17-rtl-verification-single-source.md`（**当前态唯一权威** · 见 `docs/architecture/README.md`）。

**被修订的上位决策**：ADR-072 §2.4 D4 第 3 行（「期望值 …⇒ 不可自动化」）**被部分 supersede** ⇒ ⇒ 拆为「撰写=人（不变）」+「渲染=生成器（新增）」；ADR-072 D2（四件套）与 ADR-073 D3/D6 **零变更**。

---

## 1. Why（为什么做）

| # | 现状症状（**实测**） | 证据 |
|---|---|---|
| W1 | **期望值三处重复**（`.rddf/checks/*.md` 手推表 / `tb_*.sv` 的 `expect_*` / `tests/*` 的 `REQUIRE`） | `examples/axi4/tb_axi4_lite_example.sv`（37 断言）· `tests/test_axi_lite_slave_conformance.cpp`（15 例） |
| W2 | **手写端口映射表且当前就是漂的** —— 头注释写 `awprot=input_1 · arprot=input_8`，实例化是 `.top_unnamed_input_1(arprot)` / `.top_unnamed_input_8(awprot)` | `examples/axi4/tb_axi4_lite_example.sv:18-25` vs `:84-93` |
| W3 | **发射实现两处**：`emit_rtl_soc.cpp` 483 行自解析（其「库层只发扁平」的前提**已被 `2db2eda` 推翻**）vs `emit_rtl_hier.cpp` 52 行薄壳 | `examples/axi4/emit_rtl_soc.cpp:5-8,64-89,415-483` · `src/rtl_emit/cli.cpp:77-78,186-208` |
| W4 | **生成物无新鲜度判据**；护栏⑥ 只判 `checks >= min_checks`（不判 TODO 残留） | `scripts/verify_examples_rtl_sim.sh:486-490` |
| W5 | **`AxiLiteTop` 双权威**（`tests/test_axi_lite_verilator_e2e.cpp:74-142` 复制一份） | C3/C7 禁 tests 反向 include examples ⇒ 只能抽到 `include/` |

## 2. What Changes（改什么）

| # | 变更 | 对应 ADR |
|---|---|---|
| C1 | **顶层组件抽取**（`AxiLiteTop` / `AxiSocTop` / `AxiLiteTestMaster` → `include/axi4/*_top.h`）+ 删 `tests/` 副本 | 蓝图 §4.5 |
| C2 | **发射面收敛**：`emit_rtl_soc.cpp` → 库层薄壳（483→~60）· 删 `emit_rtl.cpp`（并入 example 的 `--emit-rtl`）· `--check-products` 机制下沉 `src/rtl_emit/` | 蓝图 §4.3 |
| C3 | **产品顶层端口具名化（32 个）**：`AxiLiteTop` 20 + `AxiSocTop` 12（⚠️ `AxiLiteTestMaster` 的 23 条**不改**）· 豁免表 `1447 → 1415` | **ADR-075 E1–E6** |
| C4 | **`.verif` DSL + 一份 IR + 两个执行后端**（IR 解释器 / Verilog 渲染器）· 封闭原语集 + 表达式文法 + **时序锚点** + **fail-closed** | **ADR-074 D1/D2** |
| C5 | **TB 改为生成物（checked-in）+ 新鲜度 + 条数等式** | **ADR-074 D4** |
| C6 | **期望值机器层/推导层分离 + 双向 1:1** | **ADR-074 D5** |
| C7 | **`Simulator` 新增 node_id 级非类型化访问器**（公开面登记） | **ADR-074 D2** · 蓝图 M19 |
| C8 | **护栏改造**：⑥ 改等式 + 新增 ⑥'/⑥''/⑥‴/⑧'，各配负向对照 | 蓝图 §4.4 |
| C9 | **文档同步**（usage/developer guide + CHANGELOG + 锚点） | 蓝图 Step 6 |

## 3. Acceptance（验收标准）

**23 条**（change 级编号；**唯一权威 = `openspec/changes/rtl-verification-single-source-2026-11/proposal.md` 的「验收标准」表**）。

| change AC | 内容（摘要） | 来源 |
|---|---|---|
| AC1 | 发射入口 `.cpp` 无直接 `toVerilogHierarchical`（白名单 = riscv-mini 测试/注释） | 蓝图 AC1 · ADR-074 AC1 |
| **AC2** | 32 条清单内端口 `cpp_member != "unnamed_input"` + 两表 **1:1**；⚠️ **判定域 = `role ∉ {clock,reset}` 且 `is_top_io`** | 蓝图 AC2 · ADR-075 AC1/AC2 |
| **AC3** | `.md` ↔ `.verif` **scenario 级** 1:1（**只判键集合 · 不判数值**） | 蓝图 AC3 · ADR-074 AC3 |
| **AC4** | `.sv` 无 `top_unnamed_*` **+ 连线数 == `port_table[]` 条目数**（存在面） | 蓝图 AC4 · ADR-074 AC4 |
| **AC5** | `.sv` 无 `// TODO:` **+ `scenarios ≥ 1`** | 蓝图 AC5 · ADR-074 AC5 |
| AC6 | 重新生成 ⇒ 逐字节相同 | 蓝图 AC6 · ADR-074 AC6 |
| **AC7** | **三方等式**：`.md` 手推条数 == `.verif` 静态展开 == 运行期 checks；⚠️ 禁由生成器回填 | 蓝图 AC8 · ADR-074 AC7 |
| **AC8** | `check_id` 集合相等 + 结论相同（**需 `.sv` 增 per-check 输出协议**，或降级方案） | 蓝图 AC9 · ADR-074 AC8 |
| **AC9** | ≥3 时序变异 + 5 条护栏负向对照；⚠️ 无效变异如实登记且不计入 3 条；⚠️ 编译失败算红 | 蓝图 AC10 · ADR-074 AC9 |
| AC10 | 扫描器 fail-closed | 蓝图 AC11 · ADR-074 AC10 |
| **AC11** | 列号 MUST 从 `#COLUMNS` 推导（负向对照 = 中间插列仍绿） | 蓝图 §4.4 · 本项新增 |
| AC12 | tsv `generated` 列三方一致 | 蓝图 AC12 · ADR-074 AC11 |
| AC13 | `verify_examples_rtl_sim.sh` 绿 + `--self-test` 绿 | 蓝图 AC13 · ADR-074 AC12 |
| AC14 | `ctest -LE perf` 绿 + `ctest -L verilator_e2e` 绿 | 蓝图 AC14 · ADR-074 AC13 |
| **AC15** | `verify_test_counts.sh` 绿 + `CTEST_TOTAL` 266 → 269 | 本项新增 |
| AC16 | 护栏 `:214-217` 逐字未改 | 蓝图 AC15 · ADR-074 AC14 |
| AC17 | 豁免表 `1447 → 1415` + `verify_port_naming.sh` 绿 | 蓝图 AC16 · ADR-075 AC4 |
| **AC18** | 活动面 grep 归零（**含白名单 + 精确目标集**） | 蓝图 AC17 |
| AC19 | 子 module 端口名（23 条）未变 | 蓝图 AC18 · ADR-075 AC10 |
| AC20 | 时序锚点构例两后端同结论 | 蓝图 AC19 · ADR-074 AC15 |
| **AC21** | 名字未命中/多义 ⇒ fail-closed + 点名（现状 `CHWARN` + 返回 0 ⇒ 必须拒） | 本项新增 |
| **AC22** | 发射器调用契约（`--out/--emit-hygiene` 或 tsv 增 `emitter_args`） | 本项新增 |
| **AC23** | 碎片化量化（改一处动几处 + 目录读者视角） | 本项新增（用户隐藏意图） |

## 4. Capabilities（受影响能力 · **归属已按 metis P0 纠偏**）

| capability | 种类 | 说明 |
|---|---|---|
| `rtl-hierarchical-output` | **ADDED** | **命名规则**归它（`hierarchical-port-naming-guard/spec.md:8-9` 明文禁复述）⇒ 32 条清单内联 |
| `hierarchical-port-naming-guard` | **ADDED** | 只收**机制义务**（1:1 join + fail-closed） |
| `rtl-soc-output` | **MODIFIED ×2（标题不变）+ ADDED ×1** | TB 生成化 · 期望源分层 · 单一语义来源（IR） |
| **`examples-rtl-sim-coverage`** | **新建（ADDED）** | 6 条判据；⚠️ 该名字**已在 tsv 表头 / `run_all_ported_tests.sh:283` 使用**但**从未建 spec** ⇒ 补登记 |
| `rtl-emit-portability` | **不改** | 清单形状未变（不加字段，用 join） |
| `example-runtime-coverage` · `example-assertion-quality` | **无需 delta** | 已核：前者管 CTest 注册；后者扫 Catch2 测试源（`verif_run` 是 `main`） |

## 5. Impact（影响面与代价）

| # | 影响 | 缓解 |
|---|---|---|
| I1 | **新增一套机制**（扫描器 + IR + 2 执行后端 + 判据）⇒ 自身缺陷面 | fail-closed + 条数等式 + 变异自证（蓝图 §7 R1/R2） |
| I2 | **失去 C++ 编译期语法检查**（`.verif` 不进编译单元） | fail-closed + 实际执行 |
| I3 | **用户可见**：32 个产品端口名变更 | CHANGELOG + 同批改 TB |
| I4 | **公开面扩张**：`Simulator` node_id 级访问器 | `public-api-surface.md` 登记 |
| I5 | **`.verif` 是新文件类型** | 工具链无原生支持（低） |
| I6 | **全仓 1415 条端口债仍在** | 棘轮只减不增 |

---

## 6. 实施序（**硬约束**）

```
T1 抽取顶层组件  →  T2 发射面收敛  →  T3 端口具名（32）  →  T4 DSL+IR  →  T5 生成 TB+护栏  →  T6 文档
```

⚠️ **T3 必须早于 T4**（IR 解释器的 `name → node_id` 解析依赖具名 · `unnamed_input` 重名不可区分）。
⚠️ **T1 必须早于 T3/T4**（具名与 IR 解释器都要能 include 顶层组件）。
⚠️ **T2 单独做完不构成诉求闭环**（只收 G-发射）⇒ 汇报 MUST 说清。

---

## 7. metis 第 1 轮处置（**P0×10 / P1×8 / P2×6 · 全处置**）

| # | finding | 处置 |
|---|---|---|
| **P0-1** | `D1` 写「产品面顶层全量」与保留的 1415 条自相矛盾 | ✅ 改为「**本 change 登记的 32 条清单内联**」；且**规则归 `rtl-hierarchical-output`**（`naming-guard` 明文禁复述） |
| **P0-2** | `D3` 归 `rtl-emit-portability` 错（其 Purpose 只管清单形状） | ✅ **新建 `examples-rtl-sim-coverage`**（名字已在用却无 spec） |
| **P0-3** | `AC2` 判定域未定义（clock/reset 被误要求） | ✅ 判定域 = `role ∉ {clock,reset}` 且 `is_top_io` |
| **P0-4** | `AC7` 循环论证风险 | ✅ **三方等式** + 禁由生成器回填；声明**全表重新基线**为交付物 |
| **P0-5** | 名字未命中会静默读 0 | ✅ **AC21**（fail-closed + 点名 + 负向对照） |
| **P0-6** | `#COLUMNS` 规则被护栏自己违反（位置绑定） | ✅ **AC11**（从 `#COLUMNS` 推导 + 中间插列负向对照） |
| **P0-7** | `T2` 发射器调用契约缺口 | ✅ **AC22** + `T2` 与 M8/tsv **同 commit** |
| **P0-8** | N11–N13 无归属任务 | ✅ 分派：N11→T4 · N12/N13→T5 |
| **P0-9** | `AC16`（活动面 grep）不可满足 | ✅ 白名单 + 精确目标集（教学段保留） |
| **P0-10** | `AC3` 粒度不可判 | ✅ **scenario 级** + **只判键集合** |
| P1-11/12 | `AC4`/`AC5` 是缺席判据（少生成 = 全绿） | ✅ 补存在面（连线数 / `scenarios ≥ 1`） |
| P1-12 | `AC8` 不可实现（`.sv` 只打 `RESULT: checks=`） | ✅ 增 per-check 协议（或降级方案，T0 定） |
| P1-13 | `AC9` 与根 `AGENTS.md` 的「无效变异如实登记」冲突 | ✅ 无效变异登记且**不计入 3 条**；编译失败算红 |
| P1-14 | `D4` 悬空 `T-confirm` | ✅ 已核：两个 capability **无需 delta**，结论写进 `D4` |
| P1-15 | `D2` 应拆（2 MODIFIED 标题不变 + 1 ADDED） | ✅ 已拆 |
| P1-16 | 缺公开面 / 测试锚点 AC | ✅ 用户可见变更表 + **AC15** |
| P1-17 | 缺 tsv `generated` 三方一致 AC | ✅ **AC12** |
| P1-18 | ADR-075 AC3（禁 `top_` 拼接）不可机械判定 | ✅ 明确降为 **review 义务** + 可判替代（非 `top` 根名发射时端口名仍严格 == `port_table[].name`） |
| P2-19 | `Out of Scope` 漏项 + 自相矛盾句 | ✅ 已补 3 项 + 改写第 5 条 |
| P2-20 | M 清单漏 `*.md` / `tb_stub.h` / usage_guide 教学段 | ✅ 见 proposal C9/M 清单 |
| P2-21 | `AC6` 执行者与缓存成本 | ✅ 归 T5；缓存会被新产物击穿一次（已知代价） |
| P2-22 | improvement AC 表缺 ADR 反向对照 | ✅ 本文件 §3 表已加「来源」列 |
| P2-23 | `AC19`（端口绑定）是搬家式合规 | ✅ 改为「注册表条目数 == `generated` 行数，且全仓唯一耦合点」 |
| P2-24 | `R9`（`--self-test` 投毒对象）未进 T5 交付物 | ✅ 已进 T5 |

## 8. 净成本（**如实披露**）

| 项 | 量 |
|---|---|
| 净新增机制 | **≈ +1300 行**（扫描器 / IR / 解释器 / 渲染器 / 判据 / 3 个测试文件） |
| 净删除 | **≈ −530 行**（`emit_rtl_soc.cpp` 483→60 · `emit_rtl.cpp` 43→0） |
| 换掉的重复 | 2 个手写 TB 的 **51 条断言**的端口映射 + 期望值双写 |

⚠️ **若用户认为不值** ⇒ 替代方案 = **只做 C1/C2/C3**（抽取 + 发射收敛 + 具名，≈ −530 行、判据零损失），跳过 C4–C8 的 DSL 机制。
