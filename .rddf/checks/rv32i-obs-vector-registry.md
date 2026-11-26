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
> ## 今日实数（2026-11-26 实测）
> 核顶层 `ch_out` = **39** · 向量 = **28** · 本表 = **11** ⇒ 28 + 11 = 39 ✅

| 端口 | 方向 / 宽度 | 为什么**未纳入**跨后端向量 | 归位条件（完成后 MUST 删本行） |
|---|---|---|---|
| `axil_awaddr` | out / 32 | S4a（change `rv32i-soc-mmio-channel-2026-11` · 2026-11-24）给核加 AXI-Lite 通道时**未同步**向量 ⇒ ⇒ 该通道 11 根输出整体在跨后端证据之外；且该 fixture 连 8 根 `axil_*` **输入都没驱动**（未驱动 = 未定义值）⇒ 现在补比对只会比两个「未定义输入下的输出」 | change `rv32i-obs-vector-axil-2026-11`（步 2）：驱动 8 根 `axil_*` 输入（范式 = `test_rv32i_trap_entry.cpp` 的 tie-0）+ 固件加一条 MMIO 事务 + 11 根输出进 `kObsNames[]` ⇒ 本行删除 |
| `axil_awprot` | out / 2 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_awvalid` | out / 1 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_wdata` | out / 32 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_wstrb` | out / 4 | 同上（WSTRB 车道 = 最易被 codegen 算错的一族） | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_wvalid` | out / 1 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_bready` | out / 1 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_araddr` | out / 32 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_arprot` | out / 2 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_arvalid` | out / 1 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
| `axil_rready` | out / 1 | 同上 | 同上（`rv32i-obs-vector-axil-2026-11`） |
