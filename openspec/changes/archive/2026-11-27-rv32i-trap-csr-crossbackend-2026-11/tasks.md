# tasks · trap / CSR 的核级跨后端 golden（步 3 · P0-2）

> **Change**：`rv32i-trap-csr-crossbackend-2026-11` · **母 improvement**：同名
> **纪律**：手推期望 · 逐拍 diff 无掩码 · 变异证明判据非空转
> ⚠️ **本路径未启用 `rdd-builder`**（无 `.rddf/state/.planner-handoff.json` ⇒ 不伪造前置）⇒ 内置流程。

## T0 · 侦察（只读 · 已完成）

- [x] T0.1 ✅ `test_rv32i_trap_entry` 的手推常量：`mepc` = 0x8000_0000 · `mcause` = 2 · trap 落点字 0x040
- [x] T0.2 ✅ `test_rv32i_csr_instructions` 的手推常量：6 种 `funct3` + 9 条写回
- [x] T0.3 ✅ 步 2 的 fixture 可复用（`load_firmware` span 版 · `AxilSlave` · `obs_snapshot` 39 根）

## T1 · 立项

- [x] T1.1 ✅ `proposal.md` / `design.md` / `tasks.md`
- [x] T1.2 ✅ improvement HARD-GATE ✅ + 索引登记

## T2 · 实现

- [x] T2.1 ✅ `run_both()` / `require_per_cycle_equal()` / `collect_writebacks()` 共享辅助
- [x] T2.2 ✅ `Rv32iSoc - TrapEntryCrossBackend`（S2）
- [x] T2.3 ✅ `Rv32iSoc - CsrInstrsCrossBackend`（S3）
- [x] T2.4 ✅ 编码手算常量交叉校验（`0x341020F3` / `0x34202173` / `0x34009173` / `0x3407F3F3`）

## T3 · 验证

- [x] T3.1 ✅ `test_rv32i_soc_dual_parity` 全绿 ⇒ **18 用例 / 60311 断言**
- [x] T3.2 ✅ 两个变异（期望值 0x8000_0004 / csrrc 读回改 0）⇒ 各自必红
- [x] T3.3 ✅ 锚点零变化（`verify_test_counts` 绿）· 护栏全绿

## T4 · 收口

- [x] T4.1 ✅ 验收报告 `.rddf/reviews/rv32i-trap-csr-crossbackend-2026-11-verify.md`
- [x] T4.2 ✅ 遗留清扫：无 TODO / 无注释代码 / 无临时调试
- [x] T4.3 ✅ 归档（`openspec archive --skip-specs`）
