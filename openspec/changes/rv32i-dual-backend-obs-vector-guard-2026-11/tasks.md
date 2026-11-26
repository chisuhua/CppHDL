# tasks · 跨后端观测向量**守恒护栏**（RV32I · 步 1）

> **Change**：`rv32i-dual-backend-obs-vector-guard-2026-11` · **母 improvement**：同名
> **纪律**：先见红（自检投毒）· 每步跑验证 · **零产品代码** · **零端口面变更** · 零新增脚本
> ⚠️ **本路径未启用 `rdd-builder`**：其 canonical 前置（`.rddf/state/.planner-handoff.json`）
> 不成立 ⇒ 按「不得伪造前置条件」走**内置流程**（proposal/design/tasks 由本会话写）。

---

## T0 · 侦察（**只读 · 已完成**）

- [x] **T0.1** ✅ 核顶层 `ch_out` 实数 = **39**（`grep -cE '^\s*ch_out<' examples/riscv-mini/src/rv32i_core_top.h`）
- [x] **T0.2** ✅ 向量实数 = **28**（`kObsNames[]` + `static_assert(kObsCount == 28)`）
- [x] **T0.3** ✅ 缺口 = **11** 根 `axil_*` 输出（`axil_awaddr` / `awprot` / `awvalid` / `wdata` /
      `wstrb` / `wvalid` / `bready` / `araddr` / `arprot` / `arvalid` / `rready`）
- [x] **T0.4** ✅ 该 fixture **不驱动** 8 根 `axil_*` 输入（只驱动 `ext_rst` / `prog_*`）
- [x] **T0.5** ✅ `verify_dual_backend_parity.sh` = 薄壳 + `scripts/lib/dual_backend_parity.py`
      （双锚点）⇒ **加第二判据的挂点已定位**；`--self-test` 现为 `exec python3 $ANALYZER --self-test`
- [x] **T0.6** ✅ spec 侧：`verilator-rv32i-e2e` 的「全部 27 个输出观测值」= S4a 前的数字
      ⇒ 且 `ch::ch_device<Rv32iPipelineTop>` 是 **2026-11-23 已改名的旧类名**（`Rv32iCoreTop`）

## T1 · 立项（**本 change**）

- [x] **T1.1** ✅ `proposal.md`（Why / IN·OUT / AC1–AC8 / Capabilities / 用户可见变更）
- [x] **T1.2** ✅ `design.md`（4 判据 + 解析规则 + 登记格式 + 5 类投毒 + 4 条变异 + 接缝 + 边界）
- [x] **T1.3** ✅ `tasks.md`（本文件）+ **spec delta**（`verilator-rv32i-e2e` **MODIFIED**）
- [x] **T1.4** ✅ improvement 草案 HARD-GATE（`pre_create_brainstorm_check.sh` = ✅）+ 索引登记
      （`verify_improvement_index.sh` = ✅ 19 行）

## T2 · 实现（护栏 + 登记表）

- [x] **T2.1** ✅ `.rddf/checks/rv32i-obs-vector-registry.md`（11 行 + 两列必填）
- [x] **T2.2** ✅ `scripts/lib/rv32i_obs_vector.py`（`--check` / `--self-test` · fail-closed）
- [x] **T2.3** ✅ `scripts/verify_dual_backend_parity.sh` 挂第二判据（主路径 + 自检串联 · 头注释同步）
- [x] **T2.4** ✅ `docs/riscv-mini/test-architecture.md` §6 parity 行补新判据指针

## T3 · 验证（**每条 AC 都要工具证据**）

- [x] **T3.1** ✅ `bash scripts/verify_dual_backend_parity.sh` ⇒ PASS + 4 行 C1–C4（AC1）
- [x] **T3.2** ✅ `--self-test` ⇒ 原 10 项 + **5/5 投毒必红**（AC2）
- [x] **T3.3** ✅ 手工篡改三条（删向量项 / 删登记行 / 加进向量不清账）⇒ 各自必红（AC3/AC4）
- [x] **T3.4** ✅ `verify_test_counts.sh` / `verify_guard_reachability.sh` / `verify_docs_links.sh`（AC5–AC7）
- [x] **T3.5** ✅ `openspec validate <change> --strict` + `verify_openspec_delta.sh`（AC8）

## T4 · 收口

- [x] **T4.1** ✅ 验收报告 `.rddf/reviews/rv32i-dual-backend-obs-vector-guard-2026-11-verify.md`
- [x] **T4.2** ✅ 遗留清扫：无 TODO / 无注释掉的代码 / 无临时调试输出；**步 2 的名字已登记**在
      improvement 的 `进度:` 与登记表的「归位条件」列
- [ ] **T4.3** ⬜ 归档（`openspec archive`）—— 待用户确认（本 change 尚未归档）
