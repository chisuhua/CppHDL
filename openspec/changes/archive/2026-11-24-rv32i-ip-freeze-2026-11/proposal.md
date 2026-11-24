# 提案：`riscv-mini` IP 冻结 —— 判据完备（MMIO 通道 IP 级判据 + 层次化等价覆盖 + 集成契约）

- **母 improvement**：`rv32i-soc-platform-2026-11`（用户 2026-11-24 裁决：**先把 IP 做完备并冻结，再继续 SoC 批次**）
- **5 段草案**：无（本批是既有 IP 的**判据/文档收口**，不是新能力）
- **规格影响**：**无** ⇒ `.openspec.yaml` 声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`
- **规模 / 风险**：**中**（只加测试与文档 · **不改端口面 · 不改行为**）

## Why

1. **S4a 把 IP 的 MMIO 通道做出来了，但 IP 级判据没跟上**：核级 TB（`examples/riscv-mini/tb_rv32i_soc.{cpp,sv}`）
   把 8 根 `axil_*` 输入**全部 tie 0**（正确 —— 核级没有从设备）⇒ ⇒ 24 条核级判据里**没有一条**走 MMIO
   ⇒ ⇒ 通道的 IP 级行为只被 **SoC 级**（`examples/riscv-soc` 的 A1/A7/A9）间接验证
   ⇒ ⇒ 而「IP 可复用」的前提是它**自己的接口**被单独验证过（换 SoC 时才有底）。
2. **层次化等价没有覆盖 19 根新端口**：`test_rv32i_hier_rtl.cpp` 的观测面只有 **9** 个根输出、输入只驱动 **4** 根
   ⇒ ⇒ 层次化发射若**漏掉/接错** master 的 19 根端口，逐拍等价**抓不到**（端口表由 R8′ 覆盖，但那是**表**不是**值**）。
3. **复用方需要一个集中的契约陈述**：reset 极性、单事务拍数上界、MMIO 段判据、错误语义现状、`irq` 缺失的定性
   ⇒ ⇒ 现在散落在注释里，`examples/riscv-mini/AGENTS.md` 只有端口表。

## What Changes

| # | 载体 | 内容 |
|---|---|---|
| **T1** | 新 target `test_rv32i_core_mmio`（`examples/riscv-mini/tests/test_rv32i_core_mmio.cpp`） | `ch_device<Rv32iCoreTop>` + **响应式 AXI-Lite 从设备 stub**（可编程延迟 / `rdata` / `bresp`）⇒ 7 组判据 |
| **T2** | `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 追加一个 TEST_CASE | **19 根 `axil_*`** 纳入 flat≡hier 逐拍等价（8 输入受驱动 + 11 输出被观测）+ MMIO 程序 ⇒ 端口**值**等价 |
| **T3** | `examples/riscv-mini/AGENTS.md` | 新增「**IP 集成契约**」段（reset / 事务上界 / 分流判据 / 错误语义 / `irq` = polling-only / 已知限制） |
| **T4** | 载体重算 | `CTEST_TOTAL` 276 → **277** · `EXAMPLES_TEST_SOURCES` 21 → **22** · `verify_example_assertion_quality` 的 CH4 锚点新增一行 · `examples/riscv-mini/AGENTS.md` 的 target/source 计数 |
| **T5** | 验证 | 新判据先见红（见 design §3）· 全量 `ctest` · 全量 `verify_*.sh` · `run_all_ported_tests` · `rv32i_rtl_sim_env` |
| **T6** | 归档 | `openspec archive --skip-specs` · SoC 侧登记「消费 IP v1」 |

## 验收标准

- [x] **AC1** 🔴 **MMIO 读值在 IP 级可判**：stub 回 `rdata = 0x5A` ⇒ ⇒ 程序 `lui/lw/addi/sw` 之后，
  **MMIO 写（地址 0x8010_2000）的 store 数据 == 0x5A**
  （用**既有根端口** `obs_data_write_data` + `obs_data_addr` 观测 ⇒ **不加端口**）
  > ⚠️ **2026-11-24 · 实施期订正（critic 轮次 1 BLOCKER）**：初稿写「**地址 0** 的 store 数据 == 0x5A」，
  > 而实现的 store 是 `sw x6, 0(x4)`（**MMIO 写** 0x8010_2000 · 与 SoC 级 A9 的程序逐字一致 ⇒ 两处期望可对照）
  > ⇒ ⇒ 旧措辞在本程序里**无对应事件**（地址 0 的 store 属 AC5 的非 MMIO 程序，数据 53）⇒ ⇒ 已同步修正。
- [ ] **AC2** 🔴 **事件计数**：`store` 事件 == 1（只有那条 `sw`）· 写回 == 3（x4 / x6 / x7）
- [x] **AC3** 🔴 **事务依赖（两侧夹逼）**：从设备**应答** ⇒ 程序完成；从设备**永不应答** ⇒ 程序**永不完成**
  （store 不提交 · `arvalid` 保持到窗口末）⇒ ⇒ 证明「完成」不是核自说自话 ⇒ ⇒ 证明核真的在等从设备
  > ⚠️ **2026-11-24 · 实施期 dated 订正**：初稿是「事务 MUST 在**手推拍数上界**内完成」——
  > 那个上界只能由 **master 状态机的拍数**推出 ⇒ ⇒ **把实现时序写进判据**（改一发即红）
  > ⇒ ⇒ 违反「期望值不得依赖实现时序」⇒ ⇒ 改为**反证形态**（与实现拍数无关）。
  > 判据落点 = `tests/test_rv32i_core_mmio.cpp` 的第 3 个 TEST_CASE；机理见 `design.md` §1.3。
- [ ] **AC4** 🔴 **DECERR 不挂死**：stub 回 `bresp = 3` ⇒ ⇒ 程序 MUST 继续（写回仍到 3）
- [ ] **AC5** 🔴 **非 MMIO 路径零变化**：同一程序里的 `sw x3, 0(x0)`（D-L1）仍成立；核级 24 判据**零变化**
- [ ] **AC6** 🔴 **层次化等价覆盖新端口**：19 根 `axil_*` 纳入逐拍等价；变异（hier 侧漏连一根）⇒ **必红**
- [ ] **AC7** 🔴 **零回归**：`ctest` 全绿 · 全部 `verify_*.sh` 全绿 · `run_all_ported_tests` PASS · `rv32i_rtl_sim_env` PASS（53 端口）

## 用户可见变更

> 本批**不改端口面、不改行为、不改公开符号** ⇒ ⇒ **无用户可见变更**。
> 唯一对外影响 = `examples/riscv-mini/AGENTS.md` 新增「IP 集成契约」段（文档）与一个新的测试 target。

## Out of Scope（本批明确不做）

- **`bresp` / `rresp` 的错误语义实现**（IP-C）：本批只**判「DECERR 也完成」**，不定义错误观测面 ⇒ 归 **S5**（与 `irq` 同批）
- **数据侧 TCM 越界收紧**（IP-D）：行为变更 ⇒ 独立立项
- **`irq` 口**（IP-E）：G-5 契约已定但实施在 **S5**（被 `rv32i-privileged-csr-2026-11` 阻塞）⇒ 本批只在契约段写明「IP v1 = polling-only」
- **SoC 侧任何改动**（S4b 起）