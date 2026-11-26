# 跨后端观测向量 —— **缺口登记表**（RV32I 核顶层 `ch_out` 端口面）

> **判据载体** = `scripts/lib/rv32i_obs_vector.py`（挂在 `scripts/verify_dual_backend_parity.sh` 内）
> **契约** = `openspec/specs/verilator-rv32i-e2e/spec.md` §「RV32I SoC MUST 有 interpreter 与
> verilator 的双路逐周期 parity」的 **Scenario: 观测集合 MUST 对着端口面守恒**
> **母 improvement** = `.rddf/improvements/rv32i-dual-backend-obs-vector-guard-2026-11.md`
>
> ## 这张表是什么
> 核顶层（`examples/riscv-mini/src/rv32i_core_top.h` 的 `__io`）的每个 `ch_out` MUST
> 二选一：**进跨后端逐拍比对向量**（`test_rv32i_soc_dual_parity.cpp` 的 `kObsNames[]`），
> 或**登记在本表**。两者都不进 ⇒ 护栏红（`C4 归属`）。
>
> ## 双向判据（**编造豁免 / 过期豁免都红**）
> - **C2 登记有效**：本表每行的端口名 MUST 真实存在于核顶层 `ch_out` ⇒ 否则红；
> - **C3 登记不 stale**：本表每行的端口 MUST **不在**向量里 ⇒ 补齐后 MUST 删本行 ⇒ 否则红；
> - **C1 守恒**：`|向量| + |本表| == |核顶层 ch_out|`；
> - 两列（「为什么未纳入」/「归位条件」）MUST 非空 ⇒ 否则红。
>
> ## 🟢 当前状态：**本表已归零**（2026-11-26）
> 核顶层 `ch_out` = **39** · 向量 = **39** · 本表 = **0** ⇒ `39 + 0 == 39` ✅
>
> 归零动作 = change `rv32i-obs-vector-axil-2026-11`（步 2 of
> `rv32i-dual-backend-obs-vector-guard-2026-11`）：把 S4a 遗留的 **11 根 `axil_*`** 输出
> 补进 `kObsNames[]`（并同批驱动 8 根 `axil_*` 输入 + 加一条 MMIO 事务固件 + 加
> `Rv32iSoc - AxilMmioCrossBackend` 用例）。
> ⇒ ⇒ **今后新增 `ch_out` ⇒ MUST 直接进向量**（本表只用于「有明确理由暂时不进」的登记）。
>
> ## 登记格式（需要时）
> `| \`<端口名>\` | out / <位宽> | 为什么未纳入（MUST 非空） | 归位条件（MUST 非空） |`

| 端口 | 方向 / 宽度 | 为什么**未纳入**跨后端向量 | 归位条件（完成后 MUST 删本行） |
|---|---|---|---|
