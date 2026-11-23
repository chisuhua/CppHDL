# Proposal: ELF → init_data 端到端装载链路（方案 a）

> **来源**：`firmware-load-chain`（2026-11-14 归档）的**收口**——
> capability `rv32i-firmware-loading` 的「ELF 装载」与「内存初值装载」两条
> Requirement 各自成立，但**它们之间的接缝从未构建**：
> `tools/rv32i-fw-dump` 的三种产物在仓内**零消费方** ⇒ ⇒ 真 ELF **今天跑不起来**。
> **架构依据**：ADR-046（SoC 顶层归属 / 端口契约）· ADR-077 D3（`prog_*` 长期保留）
> **用户裁决**：2026-11-23「推进主线方案 a」——走**路线 A**（内存初值），**零端口变更**。

## Why

### 缺陷（2026-11-23 实测 · 可复算）

用**真工具链**（`riscv64-unknown-elf-gcc`）编一个 `.text` + `.data` + `.bss` 的
RV32I 固件，喂给 `tools/rv32i-fw-dump`，实测出 **4 个真缺陷**，
而 **`scripts/verify_rv32i_fw_dump.sh`（D0–D6）全绿**：

1. **`manifest` 的 `regions[]` 丢 D-TCM**（`main.cpp` 的 `covered` 判据写成
   「regions 里是否已有 ITCM **或** DTCM」⇒ ⇒ 第 2 个区域永远进不去）
   ⇒ ⇒ `words[]` 含 D-TCM 地址而 `regions[]` 只含 I-TCM ⇒ ⇒ **产物自相矛盾**。
2. **`mem` 格式丢地址** ⇒ ⇒ 多区域固件被**首尾相连**成一条无地址的字流
   ⇒ ⇒ 用它喂 `prog_*`（只接 I-TCM）会把 `.data` **静默装进 I-TCM 的字 5..21**。
3. **`vh` 格式不是可编译产物**（`$readmemh("<自身 .vh 路径>", <mem-array>)`）。
4. **入口零校验** ⇒ ⇒ entry 落在 D-TCM 的 ELF 被**接受** ⇒ ⇒ 装出来**必砖**（PC 从 I-TCM 取指拿到 0）。
5. **重定位检查是空循环**（`for (k < p_filesz) { (void)k; }`）⇒ ⇒ 与 capability 的
   「非零重定位 MUST 报错」**直接冲突**。
6. **SoC 级整级没有路线 A** ⇒ ⇒ 产品单元只能跑纯 `.text` 程序。

### 为什么现在做

- 「固件转储器 + ELF 装载」是用户诉求的**全部落点**，而**链路今天不闭环** ⇒
  ⇒ 有一个**被护栏钉住却没人用**的工具 = 假完成。
- 判据还得出来：**对从未演练过消费者的接口，单测护栏结构性失明**
  （D0–D6 的 fixture 是**单段** ELF ⇒ ⇒ 上面前 4 条全绿）。

## What Changes

### 1. 工具 `tools/rv32i-fw-dump`（4 修 1 增）

| 项 | 内容 |
|---|---|
| 修 `regions` | 判据改为「**本段**落在哪个区域 · 且该区域**尚未**登记」⇒ ⇒ manifest 双区域齐全 |
| 修 `mem`/`vh` | **多区域 ⇒ ⇒ exit 3 + 诊断**（单条无地址字流无法表达区域归属 ⇒ ⇒ 禁靠猜） |
| 修 `entry` | **入口 MUST == `kPcReset`** ⇒ ⇒ 否则 exit 3（装出来必砖） |
| 修 重定位 | **读 section header**：非空 `SHT_RELA`/`SHT_REL` ⇒ ⇒ exit 3（此前是空循环） |
| **增 `mem-split`** | `--format mem-split --out <dir>` ⇒ ⇒ `<dir>/itcm.mem` + `<dir>/dtcm.mem`<br>· **行号 = 区域字索引**（空洞**补零**）· 字节→字索引换算**只在一处** |

### 2. 核级 TB（`examples/riscv-mini/tb_rv32i_soc.cpp`）

- 新增 **`--firmware <dir>`**：读两个镜像 ⇒ ⇒ 构造参数 `itcm_init`/`dtcm_init`
  ⇒ ⇒ `ch_mem::init_data` ⇒ ⇒ **零 `prog_*` 写** ⇒ ⇒ 跑 ⇒ ⇒ 打印
  **协议行** `FIRMWARE-TRACE: retired=… fetch0=… wb=… stores=…`
  ⇒ ⇒ **TB 不硬编码固件期望值**（固件专属期望只存在于护栏）。
- 新增 **`--emit-rtl --firmware <dir>`**：同一镜像进产物 `initial` 块。
- **尾部填 NOP**（G1 纪律：I-TCM 末尾读到 0 = 未知 opcode ⇒ 冻结）。

### 3. SoC 级（`examples/riscv-soc/`）

- `Rv32iSocTop` 新增 `itcm_init` / `dtcm_init` **构造参数**（**零端口变更**：
  `__io` 一字未动 ⇒ ⇒ 根端口表仍 8）。
- `tb_riscv_soc.cpp` 新增 `--firmware <dir>`（验证 + 发射）⇒ ⇒
  协议行 `SOC-FIRMWARE-TRACE: store_count=… writeback_count=…`。

### 4. 判据

- `scripts/verify_rv32i_fw_dump.sh`：**D7–D12** + `--self-test`（投毒自证）
- **新增** `scripts/verify_rv32i_elf_e2e.sh`：**E0–E6**（端到端 + 投毒 + env-gated 真工具链 leg）

## Impact

### Affected specs

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-firmware-loading`** | **MODIFIED** ×3 | ① 内存初值装载 → 追加「ELF 派生的镜像 MUST 能进 init_data · 行号 = 区域字索引 · 空洞补零 · SoC 级 MUST 也能携带初值」<br>② 转储器可寻址 → 追加 `mem-split` 格式 + `mem`/`vh` 多区域 MUST 拒绝<br>③ ELF 装载 MUST 报错 → 重定位检查**落地为 section header 判据** + 入口 MUST == PC 复位值 |

### Affected code

| 路径 | 改动 |
|---|---|
| `tools/rv32i-fw-dump/main.cpp` | 4 修（regions / mem·vh 多区域 / entry / 重定位）+ `mem-split` |
| `examples/riscv-mini/tb_rv32i_soc.cpp` | `--firmware <dir>`（验证 + 发射）· NOP 填充 · `FIRMWARE-TRACE` 协议行 |
| `examples/riscv-soc/src/rv32i_soc_top.h` | `itcm_init`/`dtcm_init` 构造参数透传（**零端口变更**） |
| `examples/riscv-soc/tb_riscv_soc.cpp` | `--firmware <dir>`（验证 + 发射）· `SOC-FIRMWARE-TRACE` 协议行 |
| `scripts/verify_rv32i_fw_dump.sh` | D7–D12 + `--self-test` |
| `scripts/verify_rv32i_elf_e2e.sh`（**新**） | E0–E6 端到端判据 |
| `.github/workflows/ci.yml` · `.rddf/checks/guard-reachability.md` | 新护栏的 CI 入口 + 登记（双向 1:1） |

### 不做（**Out of Scope**）

- ❌ **删除 `prog_*` 根端口** —— ADR-077 D3 已裁决**长期保留**（产品端口面兼容性，ADR-072 D1）。
- ❌ **运行期统一装载口 / boot ROM** —— 独立价值有限（路线 A 已覆盖 capability 本意），
  且要动被 C6 快照冻结的 34 端口面 ⇒ ⇒ **登记为后续**。
- ❌ **总线 / DMA 装载（S3）** —— ADR-077 D3 已登记的**独立轨道**。
- ❌ **符号表解析** —— 本 capability 不要求。
- ❌ **`BootROM` stub 的处置** —— 与本项无关（`rv32i-dead-component` 类 backlog）。
- ❌ **Verilator 侧的行为判据** —— 需要 verilator（CI env-gated）⇒ ⇒ 本项只判
  「产物端口面 + `initial` 块内容」（不需要 verilator）。

## 用户可见变更

- **`tools/rv32i-fw-dump` 新增 `--format mem-split`**（按区域拆分的初值镜像）。
- **行为收紧（非破坏性）**：`--format mem` / `--format vh` 遇**多区域**固件从
  「静默拼接」改为 **exit 3**（此前产出的是**装错位置**的字流 ⇒ ⇒ 无人可依赖）；
  **入口 ≠ `kPcReset`** 与**含非空重定位表**从「接受」改为 **exit 3**。
  ⇒ ⇒ 仓内**零消费方** ⇒ ⇒ 无下游迁移。
- **`Rv32iPipelineTop` / `Rv32iSocTop` 的可选构造参数**（`itcm_init` / `dtcm_init`）
  ⇒ ⇒ 默认空 ⇒ ⇒ **非破坏性**（SoC 级的两个参数是**新增**）。
- **TB 新增模式**：`tb_rv32i_soc --firmware <dir>` / `tb_riscv_soc --firmware <dir>`
  （含与 `--emit-rtl` 组合）。
