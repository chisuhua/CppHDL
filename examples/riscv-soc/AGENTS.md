# AGENTS.md - RV32I SoC 平台（SoC 级产品单元）

Child of root `AGENTS.md`.

## OVERVIEW

**两级顶层**（ADR-077 D1 · `Accepted` 2026-11-23）的**SoC 级**那一级：

| 级 | 住址 | 角色 |
|---|---|---|
| **IP 级**（核复合体） | `../riscv-mini/src/rv32i_core_top.h` | pipeline + I-TCM + D-TCM · **34 端口** · 24 条核级判据 |
| **SoC 级**（本目录） | `src/rv32i_soc_top.h` | 把核复合体 + 观测 IP 装配成 SoC 根 · **8 端口** |

> ⚠️ **本目录 MUST NOT 复制任何微架构实现**（ADR-077 D1）⇒ 只做实例化与连线。

## STRUCTURE

```
examples/riscv-soc/
├── src/
│   ├── rv32i_soc_top.h        # SoC 根（Rv32iSocTop · 无 main）
│   └── soc_trace_counter.h    # 观测 IP（SocTraceCounter · 两个事件的饱和计数）
├── tb_riscv_soc.cpp           # 激励驱动三模式：默认验证 / --emit-rtl / --check-products
├── tb_riscv_soc.sv            # 独立 Verilog TB（手写 · generated=no 终态）
├── CMakeLists.txt
└── AGENTS.md                  # 本文件（含核复合体 34 端口契约表）
```

## RUN

```bash
cmake --build build --target tb_riscv_soc -j$(nproc)

# ① 默认 = 解释器路径 + 手推断言（A1–A6）
build/examples/riscv-soc/tb_riscv_soc
#   ⇒ RESULT: checks=7 fails=0

# ② 发射（恒加 --hierarchical ⇒ 10 module）
build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out out/hier

# ②' 从 **ELF 派生的镜像**装载（**方案 a**：零 `prog_*` 写 ⇒ `ch_mem::init_data`）
build/tools/rv32i-fw-dump --in fw.elf --format mem-split --out out/fwimg
build/examples/riscv-soc/tb_riscv_soc --firmware out/fwimg
#   ⇒ 打印协议行 `SOC-FIRMWARE-TRACE: store_count=… writeback_count=…`
#   ⇒ ⚠️ SoC 级只判「固件真的在跑」（根端口面 8 ⇒ 看不到 `.data` 读回；
#      那条承重判据在**核级** `examples/riscv-mini`）
build/examples/riscv-soc/tb_riscv_soc --emit-rtl --firmware out/fwimg --out out/fwprod

# ③ 四件套自检（module 集 / 顶层端口 + 未驱动 / lint / 双形态逐拍 + 篡改自证）
build/examples/riscv-soc/tb_riscv_soc --check-products --out out/chk
#   ⇒ PRODUCTS-CHECK: PASS
```

⚠️ **fail-closed**：默认模式收到任何未知参数 ⇒ exit 2；发射模式缺 `--out` ⇒ exit 2。

## 产品面（手推 · 禁从 `ch_layer` 调用点派生）

| 项 | 值 | 出处 |
|---|---|---|
| 产物 module 集 | `top` · `Rv32iCoreTop` · `SocTraceCounter` · `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` · `ForwardingMux_32_` = **10** | ADR-077 V2 |
| 顶层端口 | **8** = 框架 2 + 根自有 6 | ADR-077 V1 |
| 变化观测量 | **2**（`store_count` / `writeback_count`） | `rtl-soc-output` 签批要求 ≥2 |
| 手推计数 | `store_count == 1` · `writeback_count == 3`（7 字程序） | 期望源见下 |

> ⚠️ **两值 MUST 不同** ⇒ 否则两个事件源接反 / 同源时观测值不变 ⇒ 判据恒绿
> （三条变异自证见期望源 §4）。

## 核复合体（`Rv32iCoreTop`）的**34 端口契约表**

> 这是「核可替换」的**唯一物证**，也是 S3（总线化）的**变更基线**。
> ⚠️ 端口名是**产物名**（带实例路径前缀 `top_core_`）⇒ 期望表由
> `scripts/verify_examples_rtl_sim.sh` 从**本表**解析并与产物比对（单一真相源 = 本表）。
> ⚠️ 核自有 32 条与 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 **C6 快照逐字一致**
> （生成时已机械校验）⇒ 改核端口会**同时**打红 C6 与本表 ⇒ 强制重算。

| # | 端口（产物名） | 方向 | 位宽 | 归属 |
|---|---|---|---|---|
| 1 | `default_clock` | INPUT | 1 | 框架 |
| 2 | `default_reset` | INPUT | 1 | 框架 |
| 3 | `top_core_ext_rst` | INPUT | 1 | 核自有 |
| 4 | `top_core_prog_we` | INPUT | 1 | 核自有 |
| 5 | `top_core_prog_addr` | INPUT | 32 | 核自有 |
| 6 | `top_core_prog_data` | INPUT | 32 | 核自有 |
| 7 | `top_core_obs_instr_addr` | OUTPUT | 32 | 核自有 |
| 8 | `top_core_obs_data_addr` | OUTPUT | 32 | 核自有 |
| 9 | `top_core_obs_data_write_data` | OUTPUT | 32 | 核自有 |
| 10 | `top_core_obs_data_write_en` | OUTPUT | 1 | 核自有 |
| 11 | `top_core_obs_perf_cycles` | OUTPUT | 48 | 核自有 |
| 12 | `top_core_obs_perf_instructions` | OUTPUT | 48 | 核自有 |
| 13 | `top_core_obs_perf_branch_count` | OUTPUT | 32 | 核自有 |
| 14 | `top_core_obs_perf_branch_mispredict` | OUTPUT | 32 | 核自有 |
| 15 | `top_core_obs_perf_load_use_stalls` | OUTPUT | 32 | 核自有 |
| 16 | `top_core_obs_perf_control_stalls` | OUTPUT | 32 | 核自有 |
| 17 | `top_core_dbg_id_instr` | OUTPUT | 32 | 核自有 |
| 18 | `top_core_dbg_id_pc` | OUTPUT | 32 | 核自有 |
| 19 | `top_core_dbg_id_valid` | OUTPUT | 1 | 核自有 |
| 20 | `top_core_dbg_id_is_branch` | OUTPUT | 1 | 核自有 |
| 21 | `top_core_dbg_id_is_load` | OUTPUT | 1 | 核自有 |
| 22 | `top_core_dbg_id_is_store` | OUTPUT | 1 | 核自有 |
| 23 | `top_core_dbg_branch_taken` | OUTPUT | 1 | 核自有 |
| 24 | `top_core_dbg_wb_write_en` | OUTPUT | 1 | 核自有 |
| 25 | `top_core_dbg_wb_write_data` | OUTPUT | 32 | 核自有 |
| 26 | `top_core_dbg_wb_rd_addr` | OUTPUT | 5 | 核自有 |
| 27 | `top_core_dbg_exmem_alu_result` | OUTPUT | 32 | 核自有 |
| 28 | `top_core_dbg_exmem_imm` | OUTPUT | 32 | 核自有 |
| 29 | `top_core_dbg_exmem_rs2_data` | OUTPUT | 32 | 核自有 |
| 30 | `top_core_obs_data_rdata` | OUTPUT | 32 | 核自有 |
| 31 | `top_core_dbg_exmem_valid` | OUTPUT | 1 | 核自有 |
| 32 | `top_core_dbg_mem_valid_out` | OUTPUT | 1 | 核自有 |
| 33 | `top_core_dbg_load_use_detected` | OUTPUT | 1 | 核自有 |
| 34 | `top_core_dbg_illegal_reason` | OUTPUT | 2 | 核自有 |

⇒ **34** = 框架 2（`default_clock` / `default_reset`）+ 核自有 **32**（4 INPUT + 28 OUTPUT）。

## 期望源（**单一真相**）

`.rddf/checks/soc-rtl-expectations.md#riscv-soc` —— 程序编码 / 手推计数 / 判据级别 / 变异自证 / 盲区。

## 已知边界

- ⚠️ **SoC 级看不到核的内部信号**（`dbg_*` / `obs_*` 不是顶层端口 · ADR-072 D1）⇒ 核级 24 条判据**只**在 `../riscv-mini`。
- ⚠️ **观测盲区**：层次化只覆盖顶层可见行为 ⇒ 子树内部 `ch_out` 不可观测。
- ⚠️ **`prog_*` 是运行期后门**（ADR-077 D3 长期保留）⇒ 真实芯片形态的装载路径未定义。
- ⚠️ `--check-products` 的 lint / 双形态依赖 verilator ⇒ **CI 证据仅 Ubuntu**（ADR-035）。
