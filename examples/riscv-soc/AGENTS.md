# AGENTS.md - RV32I SoC 平台（SoC 级产品单元）

Child of root `AGENTS.md`.

## OVERVIEW

**两级顶层**（ADR-077 D1 · `Accepted` 2026-11-23）的**SoC 级**那一级：

| 级 | 住址 | 角色 |
|---|---|---|
| **IP 级**（核复合体） | `../riscv-mini/src/rv32i_core_top.h` | pipeline + I-TCM + D-TCM + MMIO 主设备 · **53 端口** · 24 条核级判据 |
| **SoC 级**（本目录） | `src/rv32i_soc_top.h` | 把核复合体 + 观测 IP + Lite 互联 + GPIO 装配成 SoC 根 · **10 端口** |

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

# ① 默认 = 解释器路径 + 手推断言（A1–A8）
build/examples/riscv-soc/tb_riscv_soc
#   ⇒ RESULT: checks=9 fails=0

# ② 发射（恒加 --hierarchical ⇒ 12 module）
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
| 产物 module 集 | `top` · `Rv32iCoreTop` · `SocTraceCounter` · `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` · `ForwardingMux_32_` · `AxiLiteMatrix2x2` · `AxiLiteGpio` = **12** | ADR-077 V2 + **S4a dated addendum** |
| 顶层端口 | **10** = 框架 2 + 根自有 8（+ `gpio_out` / `gpio_in`） | ADR-077 V1 + **S4a dated addendum** |
| 变化观测量 | **2**（`store_count` / `writeback_count`） | `rtl-soc-output` 签批要求 ≥2 |
| 手推计数 | `store_count == 2` · `writeback_count == 6`（12 字程序 · D-L1 一条 + MMIO 一条） | 期望源见下 |

> ⚠️ **两值 MUST 不同** ⇒ 否则两个事件源接反 / 同源时观测值不变 ⇒ 判据恒绿
> （三条变异自证见期望源 §4）。

## 核复合体（`Rv32iCoreTop`）的**34 端口契约表** ⇒ **已归位到核侧**

⇒ ⇒ **单一真相源 = `examples/riscv-mini/AGENTS.md` 的「核 IP 的 **34 端口契约表**」**
（2026-11-23 · R3e · 契约表跟随**被描述的对象** ⇒ ⇒ 核的接口契约住在核的家）
⇒ ⇒ 护栏 `scripts/verify_riscv_soc_core_ports.sh` 的 V1/V2 从**核侧**该表解析
⇒ ⇒ 本文件不再重复该表（避免两份真相）⇒ ⇒ **要改核的端口面 ⇒ 改核侧那张表**。

## 期望源（**单一真相**）

`.rddf/checks/soc-rtl-expectations.md#riscv-soc` —— 程序编码 / 手推计数 / 判据级别 / 变异自证 / 盲区。

## 已知边界

- ⚠️ **SoC 级看不到核的内部信号**（`dbg_*` / `obs_*` 不是顶层端口 · ADR-072 D1）⇒ 核级 24 条判据**只**在 `../riscv-mini`。
- ⚠️ **观测盲区**：层次化只覆盖顶层可见行为 ⇒ 子树内部 `ch_out` 不可观测。
- ⚠️ **`prog_*` 是运行期后门**（ADR-077 D3 长期保留）⇒ 真实芯片形态的装载路径未定义。
- ⚠️ `--check-products` 的 lint / 双形态依赖 verilator ⇒ **CI 证据仅 Ubuntu**（ADR-035）。
