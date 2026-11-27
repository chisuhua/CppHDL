---
name: axi4-submodule-port-naming-2026-11
priority: P2
phase: phase-6
category: framework
type: feature
状态: draft
状态依据: >-
  登记型草案（ADR-075 §2.1 的「不在范围」项明确要求「若要扩 MUST 单独立项并登记子 module
  端口名变更」）⇒ 未批准、未开工 ⇒ 目的是把 `rtl-verification-single-source-2026-11` 的
  移交项 W2 登记为独立立项，**不是**承诺本轮实施
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）
依赖: ADR-075（Accepted · §2.1 N5 已知边界）· rtl-verification-single-source-2026-11（W2 交接来源）· ADR-074（验证单一真相源）· ADR-059 / ADR-060（端口命名策略与棘轮）
来源: 交接项 W2（「子 module 端口名具名化」）来自 `rtl-verification-single-source-2026-11` + ADR-075 §2.1 的「本次不做（若要扩，MUST 单独立项并登记子 module 端口名变更）」
生成时间: 2026-11-27
evidence: probe:examples/axi4/axi_soc_example_top.h#L37
---

# Improvement: AXI4 测试主设备的**子 module 端口名**具名化（23 条 · ADR-075 N5）

- **Date drafted**: 2026-11-27
- **Drafted by**: build-agent（builder 角色）
- **Status**: 📝 **draft v1**（登记型 · 未批准 · 未开工）

**主题**: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）

## 架构依据

| 事实 | 证据 |
|---|---|
| `AxiLiteTestMaster` 的 **23** 个端口全部是**无名**形态（`ch_in<ch_bool> start;` 无名字串）⇒ 产物端口名为 `top_unnamed_input_N` / `top_unnamed_output_N` | `examples/axi4/axi_soc_example_top.h:37`–`:73` |
| 该 module **会作为子 module 进入产物**（`tb_axi_soc_example` 行登记 5 个 module：top / AxiLiteTestMaster / AxiLiteMatrix2x2_32_32_ / AxiLiteGpio_8_ / AxiLiteUart_16_） | `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `tb_axi_soc_example` 行（`kind=soc`） |
| ADR-075 的 32 端口具名化**明确排除**这 23 条，并要求「若要扩 ⇒ MUST 单独立项并登记子 module 端口名变更」 | `docs/adr/ADR-075-port-naming-prerequisite.md` §2.1 · N5 · AC10 |
| 端口名是**产品可见面**（层次化产物的 module 端口声明）⇒ 改名属公开面变更 | 同 ADR-075 §2.1 的「另一类产品可见变更」 |

## 范围

### IN

- `AxiLiteTestMaster` 的 **23** 个端口（13 `ch_in` + 10 `ch_out`）全部具名，名字串与标识符逐字相同
  （形态：`ch_in<ch_uint<32>> addr{"addr"};`）。
- 与之配套的**登记面**：产物端口名期望集（手推）· 棘轮豁免重算 · 公开面 / CHANGELOG 各一行。

### OUT

- **不改** `AxiLiteTop` / `AxiSocTop` 的 32 个顶层端口（已由 ADR-075 交付）。
- **不改**其他内层 module（`AxiLiteMatrix2x2` / `AxiLiteGpio` / `AxiLiteUart` / `AxiLiteTestMaster` 之外的组件）。
- **不改行为**：本项只改端口名，不改任何连接语义或时序。
- **不做**「把根 module 名或合成端口名重新设计」——归既有 hierarchy naming 机制。

## Why

1. **可读性 / 自解释**：层次化产物是产品交付物，子 module 端口名 `top_unnamed_input_7` 无法被下游集成者理解
   ⇒ 与「产物端口名 == C++ 成员名」这条已兑现的诉求**不连续**（顶层做了、内层没做）。
2. **ADR-075 自己留了这条口子**：「若要扩 ⇒ MUST 单独立项并登记」⇒ 不登记 = 这条已知边界**没有归属**，
   下次谁再碰到它都要重新发现一遍。
3. **风险已被隔离**：本项**不触**顶层端口面 / manifest `port_table[]`（`is_top_io` 判定域不变）
   ⇒ 影响面集中在「子 module 端口声明 + 端口名期望集 + 棘轮」三处，可独立评审。

## What Changes

1. `examples/axi4/axi_soc_example_top.h` 的 `AxiLiteTestMaster::__io(...)`：23 个端口逐个具名。
2. 产物级期望集：在 `.rddf/checks/examples-rtl-expectations.md` 的 `#soc-example` 增一节
   「子 module 端口名」（**手推** 23 条，禁从产物转录）。
3. `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `tb_axi_soc_example` 行 note 补一句形态说明
   （**不改** `min_checks` / `scenarios` / `spec_checks`）。
4. 公开面登记：`.rddf/checks/public-api-surface.md`（产物端口名变更）+ `docs/CHANGELOG.md` 一行。
5. 棘轮：`verify_port_naming.sh` 的豁免计数按实测重算（只减不增）。

## Acceptance

- [ ] **AC1 源码面**：23 个端口全部带名字串，且名字串 == 标识符
      ⇒ 判据 = 探针统计「无名端口数 == 0」且「具名端口数 == 23」（**手推 23**，不是抄输出）
- [ ] **AC2 产物面**：`--emit-rtl` 与 `--emit-rtl --hierarchical` 两种产物中，
      `AxiLiteTestMaster` module 的端口名 == 手推期望集（23 条），`unnamed_input_` / `unnamed_output_` 命中 == 0
      ⇒ 若某形态把该 module 内联（不发射）⇒ 该形态记 **N/A 并写明理由**，不得静默跳过
- [ ] **AC3 变异自证（反向）**：把其中 **1** 根改回无名形态 ⇒ AC2 MUST 转红且**点名**该端口
      （只加判据不做投毒 ⇒ 判别力未证）
- [ ] **AC4 TB 零改动**：`examples/axi4/tb_axi_soc_example.sv` **不需要**修改，且
      `RESULT: checks=15 fails=0` 不变（**这是「只改子 module 声明、未动顶层端口面」的证据**）
- [ ] **AC5 行为零变化**：扁平产物除端口名/注释外逐字节不变 · 层次化 ≡ 扁平逐拍等价仍绿 ·
      SoC 双后端 parity 仍绿
- [ ] **AC6 登记完备**：期望集（`#soc-example` 的新节）· tsv note · `public-api-surface.md` ·
      `docs/CHANGELOG.md` · 棘轮计数重算 ⇒ 五处**同批**
- [ ] **AC7 护栏全绿**：`verify_port_naming.sh` / `verify_hierarchical_port_naming.sh` /
      `verify_examples_rtl_sim.sh` / `verify_flat_rtl_emit.sh`（含各自 `--self-test`）

## Capabilities

- `hierarchical-port-naming-guard` —— **可能 MODIFIED**（若「子 module 端口名」需要写进机制义务；
  当前只收 1:1 join + fail-closed）⇒ **待实施时判定**。
- `rtl-hierarchical-output` —— **可能 ADDED/MODIFIED**（产物级契约名清单是否需要包含子 module 端口名）
  ⇒ **待实施时判定**。
- 若两者都不需要改：本项只动实现与登记 ⇒ **零 capability 变更**（归档时 `--skip-specs`）。

## Impact

- **改动文件**：`examples/axi4/axi_soc_example_top.h` · `.rddf/checks/examples-rtl-expectations.md` ·
  `.rddf/checks/examples-rtl-sim-targets.tsv` · `.rddf/checks/public-api-surface.md` ·
  `docs/CHANGELOG.md` · `scripts/probe_port_naming*`（若探针需覆盖子 module）
- **计数**：`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` **不变**（不新增/删除测试）。
- **风险**：**低–中** —— 只改名字；但端口名属产品可见面 ⇒ 若下游按 `top_unnamed_*` 硬编码会断
  ⇒ 必须走公开面登记 + CHANGELOG。**回滚成本 = 一次 revert**（改动自洽）。
- **归属批**：与任何 AxiSoc 产品面变更**同批**（避免两次改产物端口名）；当前无正在进行的产品面批
  ⇒ **不排期**，等下一次 AxiSoc 变更触发（ADR-075 §2.1 的要求是「登记」，不是「现在做」）。
