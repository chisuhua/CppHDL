# tasks · 补齐 AXI-Lite 11 根输出进跨后端向量（步 2）

> **Change**：`rv32i-obs-vector-axil-2026-11` · **母 improvement**：同名
> **纪律**：期望值手推 · 每条判据配变异 · 逐拍 diff 无掩码 · 零产品代码 · 零端口变更
> ⚠️ **本路径未启用 `rdd-builder`**：canonical 前置（`.rddf/state/.planner-handoff.json`）
> 不成立 ⇒ 按「不得伪造前置条件」走**内置流程**。

## T0 · 侦察（只读 · 已完成）

- [x] T0.1 ✅ 核顶层 `ch_out` = 39 · 向量 = 28 · 差 11 根 `axil_*`
- [x] T0.2 ✅ 既有 8 根 `axil_*` 输入**未被驱动**（`load_firmware` 只驱动 `ext_rst` / `prog_*`）
- [x] T0.3 ✅ MMIO 程序 + 手推常量先例 = `test_rv32i_core_mmio.cpp`（5 字 · 地址/数据/写回数已手推）
- [x] T0.4 ✅ 核侧 master 是**单事务**语义（`axil_mmio_master.h` 文件头：`ready` = 完成握手那一拍）

## T1 · 立项

- [x] T1.1 ✅ `proposal.md` / `design.md` / `tasks.md`
- [x] T1.2 ✅ improvement 草案 HARD-GATE（`pre_create_brainstorm_check.sh` ✅）+ 索引登记

## T2 · 实现

- [x] T2.1 ✅ `kObsNames[]` +11 · `static_assert` 39 · `obs_snapshot()` +11
- [x] T2.2 ✅ `AxilSlave` + `drive_axil()`（默认立即应答）· `load_firmware()` 内调用
- [x] T2.3 ✅ 新用例 `Rv32iSoc - AxilMmioCrossBackend`（A–G 七条判据）
- [x] T2.4 ✅ 登记表 11 行归零 + analyzer 允许空表 + 自检 P0
- [x] T2.5 ✅ 文档状态同步（test-architecture §2.1 · gap-analysis §0/§2/§3/§8/§9）

## T3 · 验证

- [x] T3.1 ✅ `test_rv32i_soc_dual_parity` 全绿 ⇒ **16 用例 / 44494 断言**
- [x] T3.2 ✅ 护栏主路径 ⇒ `向量 39 + 登记 0 == 39`（C1–C4 + C2b）
- [x] T3.3 ✅ `--self-test` ⇒ 12 项 + P0 + 5/5 投毒
- [x] T3.4 ✅ 锚点零变化（`verify_test_counts` 绿）
- [x] T3.5 ✅ `verify_docs_links` / `verify_guard_reachability` / `verify_improvement_index` 全绿

## T4 · 收口

- [x] T4.1 ✅ 验收报告 `.rddf/reviews/rv32i-obs-vector-axil-2026-11-verify.md`
- [x] T4.2 ✅ 遗留清扫：无 TODO / 无注释掉的代码 / 无临时调试（新用例的 `INFO(...)` 是失败诊断，不是调试残留）
- [x] T4.3 ✅ 归档（`openspec archive --skip-specs` · 无 capability 变更）
