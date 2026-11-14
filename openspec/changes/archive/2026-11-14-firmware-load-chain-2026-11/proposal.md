# Proposal: 固件装载链路 —— 显式地址映射 + 统一加载端口 + 转储器 / ELF 装载

> **来源**：`.rddf/improvements/firmware-load-chain-2026-11.md`（L1–L6）
> **架构依据**：`docs/adr/ADR-046-*`（SoC 顶层归属 / D10 端口契约）·
> `docs/adr/ADR-068-*`（存储器时序）· `docs/adr/ADR-069-*`（MEM 数据对齐）
> ⚠️ **🚪 开工门已闭**：公开面前置登记（R6 · `docs/CHANGELOG.md` +
> `.rddf/checks/public-api-surface.md`「行为变更」节各 1 行）· T2 裁决（R5 · ADR-069 Accepted）
> ⚠️ **顺序**：本 change 与 `memory-timing-contract` **同属 B3** ⇒ ⇒ MUST 一并实施
> （本项的装载路径要写 D-TCM ⇒ ⇒ D-TCM 的读时序 MUST 先定）
> ⚠️ **与 R17 的耦合**：R17 已把 `Rv32iPipelineTop` 的 **32 个端口具名**
> ⇒ ⇒ 本项**新增/改语义**的加载端口 MUST **一并具名**（护栏 P1 会拦未具名的新端口）

## Why

### 缺陷（实测 · 可复算）

1. **D-TCM 完全没有加载通路** ⇒ ⇒ 一个带 `.data` 的程序**装不进去**
   （现 `prog_*` **只**接 I-TCM ⇒ ⇒ 装载即丢失 `.data` ⇒ ⇒ 程序跑起来是错的）。
2. **地址静默截断**：SoC 侧无显式映射 ⇒ ⇒ 地址高位被**丢弃**（`0x8000_0000` 只低 12 位有效）
   ⇒ ⇒ 「装到了错误的地方」而**无任何报错**。
3. **PC 复位值与映射 base 的耦合是隐式的** ⇒ ⇒ 两者**恰好**都是 `0x8000_0000`
   ⇒ ⇒ 任何一侧改动 ⇒ ⇒ 另一侧**静默失配** ⇒ ⇒ 从零取指。
4. **编码器有 3 份副本**（`enc_r` / `enc_i` 等定义散落）⇒ ⇒ 改一处漏两处。
5. **没有固件转储器 / ELF 装载** ⇒ ⇒ 用户只能手写 `$readmemh` 数组。

### 为什么现在做

- 用户诉求 **②（固件转储器 + ELF 装载）** 的**全部**落点。
- 与 `memory-timing-contract` 耦合 ⇒ ⇒ **同批做** ⇒ ⇒ 避免「装载通路按旧时序写、再被时序改动推翻」。

## What Changes

### L1 — SoC 侧的**显式地址映射表**

| 区域 | base | size | 说明 |
|---|---|---|---|
| `itcm` | `0x8000_0000` | 16 KB | I-TCM · PC 起点 |
| `dtcm` | `0x8001_0000` | **由 TCM 容量派生**（禁手写） | D-TCM · 数据 |
| （预留 MMIO 区） | — | — | 本项**只留位**，不接设备 |

- ⚠️ **base 与 PC 复位值 MUST 一致** ⇒ ⇒ 消除「只低 12 位有效」的错配。
- ⚠️ **越界 ⇒ 报错** ⇒ ⇒ **禁止静默截断**（投毒自证：地址设到区域外 ⇒ MUST 有诊断）。
- ⚠️ **size 由容量派生**（`1 << (kTcmAddrWidth + 2)`）⇒ ⇒ **禁手写 16 KB 之类的字面量**。

### L2 — **内存初值装载**（2026-11-14 用户裁决：由「统一加载端口」改为「路线 A」）

固件（`.text` + **`.data`**）MUST 以**内存初值**进入对应 TCM：
`ch_mem::init_data`（构造参数）⇒ ⇒ 发射时落成 Verilog `initial` 块
⇒ ⇒ **TB 零装载动作** · **顶层端口面一字不变**。

⇒ ⇒ 理由：装载是**发射期**的事 ⇒ ⇒ 给 RTL 加装载口会把「端口契约」与「固件内容」耦合；
而路线 B 还有两条硬约束（D-TCM 写端口 MULTIDRIVEN · 装载期读地址共用）。

⇒ ⇒ **运行期装载口不在本 change**（= 「可装载性接口」，独立立项）。

### L3 — 编码器**库化**

- 新增 `include/cpu/riscv/rv32i_asm_encode.h`（header-only · `constexpr`）
  ⇒ ⇒ 既有 **3 处副本**归一为 **1 份**。
- ⚠️ **保留** `FirmwareEncoderCrossCheck`（手算常量 vs helper 产出）⇒ ⇒ **防止「库里的编码器自证」**。

### L4 — 固件转储器工具

```
tools/rv32i-fw-dump --in <fw.json|elf> --format vh|mem|manifest --out <path>
```

- 输入 = 固件清单（`{name, words[], base_addr}` 的 JSON）
- 输出 = `vh`（Verilog `$readmemh` 可读）/ `mem`（纯 hex）/ `manifest`（段 / 符号 / 入口 / 字索引映射）

### L5 — ELF 装载

- 解析 ELF32 header + program headers（`PT_LOAD`）⇒ ⇒ 取 `p_paddr` / `p_filesz` / `p_memsz`
  ⇒ ⇒ 按 L1 映射写入 ⇒ ⇒ `.bss`（`memsz > filesz`）补零。
- ⚠️ **不解析符号表**（本项不需要）。
- ⚠️ **不做重定位** ⇒ ⇒ 若遇到**非零重定位** ⇒ ⇒ **报错**（不是静默忽略）。

### L6 — 装载**可观测性**

装载后提供「读回某地址」的路径（走**真实**读通路）⇒ ⇒ TB 可断言「装进去的值 == 期望」
⇒ ⇒ **不是**「loader 返回成功」。

## Impact

### Affected specs

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-firmware-loading`**（**新**） | ADDED | 装载链路契约：显式地址映射（byte-addressed · 越界报错 · 禁静默截断）· **内存初值装载**（零端口变更）· 转储器输出格式 · ELF 装载 · 装载可观测性 |
| `rv32i-isa-semantics` | **ADDED**（⚠️ **不是** MODIFIED） | 追加「SoC 存储映射 MUST 显式声明 · PC 复位值 MUST 与映射 base 一致」⇒ ⇒ ⚠️ **本项是「追加新 Requirement」** ⇒ ⇒ openspec 的 `MODIFIED` 是**整体替换** ⇒ ⇒ 若用 MODIFIED 会**误替换**既有条款 ⇒ ⇒ 故用 ADDED |
| `cpu-cache-subsystem` | **不动** | 本项不碰 cache |
| `rtl-emit-portability` | **不动**（引用） | SoC 端口契约变化 ⇒ ⇒ manifest 快照 MUST 同步 |

### Affected code

| 路径 | 改动 |
|---|---|
| `examples/riscv-mini/src/rv32i_pipeline_top.h` | 显式映射表 + 统一加载端口 + 越界诊断 |
| `include/cpu/riscv/rv32i_asm_encode.h`（**新**） | 编码器唯一副本（`constexpr`） |
| `tools/rv32i-fw-dump/`（**新**） | 转储器 / ELF 装载 CLI |
| `tests/**` | 装载 + 读回判据 · 编码器交叉校验（保留）· 越界投毒自证 |

### 不做（**Out of Scope**）

- ❌ **运行期装载口**（不重发射即可灌固件）⇒ ⇒ 原 L2 的「统一加载端口」已于 **2026-11-14 用户裁决**
  移出 ⇒ ⇒ 它本质是「**可装载性接口**」（真机 bring-up / 调试口）⇒ ⇒ 独立立项，且 MUST 先解决
  「D-TCM 不能加第二个 `ch_mem` 写端口（Verilog MULTIDRIVEN）」与「D-TCM 的 `addr` 被读路径共用」
  两条约束

- ❌ **符号表解析** / **重定位** —— 本项不需要 ⇒ ⇒ 留给未来工具。
- ❌ **MMIO 设备接入** —— 本项**只留位**。
- ❌ **CSR / ECALL / FENCE** —— 属 `rv32i-isa-gap-closure`（P6）。
- ❌ **cache 接入** —— 独立立项。

## 用户可见变更

- **`InstrTCM` / `DataTCM` 新增**可选**构造参数 `init_words`**（`include/cpu/pipeline/rv32i_tcm.h`）
  ⇒ ⇒ 传**字镜像** ⇒ ⇒ 内部 `ch_mem` 带 `init_data` ⇒ ⇒ **产物自带 `initial` 初值块**
  ⇒ ⇒ **TB 零装载动作** · **顶层端口面一字不变**
  ⇒ ⇒ ⚠️ 默认空 ⇒ ⇒ 与基线**逐字节等价**（**非破坏性**）。
- **`Rv32iPipelineTop` 新增 `itcm_init` / `dtcm_init` 构造参数**
  （`examples/riscv-mini/src/rv32i_pipeline_top.h`）
  ⇒ ⇒ 🔴 **`dtcm_init` 是 `.data` 段的落点** —— 此前 `prog_*` **只**接 I-TCM
  ⇒ ⇒ 有初始化 `.data` 的程序**此前装不进去**。
- **新增公开头**：`include/cpu/riscv/rv32i_mem_map.h`（**地址映射单一真相** · 修掉
  「**声明 64 KB · 实现 16 KB**」）· `include/cpu/riscv/rv32i_asm_encode.h`（**RV32I 编码器库化**）
  ⇒ ⇒ ⚠️ 下游若硬编码 `ADDR_ITCM_SIZE == 0x10000` ⇒ ⇒ **MUST 改为 16384**。
- **新增工具** `tools/rv32i-fw-dump`（固件**转储器 / ELF 装载器** · `mem` / `vh` / `manifest`）
  ⇒ ⇒ ⚠️ **不解析符号表**（`.rela` 在 section 里 ⇒ ⇒ 静态重定位检查**不完整**）。
- **契约面**：新增 capability **`rv32i-firmware-loading`**（7 requirements）·
  `rv32i-isa-semantics` 追加 1 条（PC 复位值 MUST 等于 I-TCM 的 `base`）。
