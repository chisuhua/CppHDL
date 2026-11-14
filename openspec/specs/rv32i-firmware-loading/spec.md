# rv32i-firmware-loading Specification

## Purpose

规定 RV32I SoC 的**固件装载链路**契约 —— 即「固件（`.text` + `.data`）**怎么进内存**」
⇒ ⇒ 覆盖：存储映射 MUST **显式声明**且越界 MUST 报错 · 装载 MUST 经**内存初值**完成
（`ch_mem::init_data` ⇒ 产物自带 `initial` ⇒ **TB 零装载** · 顶层端口面 MUST 不变）·
固件转储器 MUST 产出**可寻址**产物 · ELF 装载 MUST 覆盖 `PT_LOAD` 且异常 MUST 报错
· 装载后的值 MUST 走**真实**读通路可读回。

本 capability 源于 change `firmware-load-chain-2026-11`（2026-11-14 归档），
装载路线 = **路线 A**（内存初值；2026-11-14 用户裁决 ⇒ ⇒ 运行期装载口 = 「可装载性接口」
独立立项）· 地址映射的单一真相 = `include/cpu/riscv/rv32i_mem_map.h`。

## Requirements

### Requirement: SoC 的存储映射 MUST **显式声明**，且越界访问 MUST 报错

RV32I SoC MUST 以**显式映射表**声明其存储区域，每个区域 MUST 携带 `{base, size}`
⇒ ⇒ `base` MUST 是**字节地址**，`size` MUST **由该区域的容量派生**（禁手写字面量）。

⇒ ⇒ 访问**落在区域外**的地址时 MUST **报错**（诊断）⇒ ⇒ **MUST NOT** 静默截断地址高位
⇒ ⇒ 理由：静默截断会把访问导向**错误的区域**，而**无任何报错**。

| 区域 | base | size 来源 |
|---|---|---|
| `itcm` | `0x8000_0000` | I-TCM 容量（16 KB） |
| `dtcm` | `0x8001_0000` | D-TCM 容量（由 TCM 地址宽度派生） |

#### Scenario: 区域大小 MUST 由容量派生

- **WHEN** SoC 声明其存储区域
- **THEN** 每个区域的 `size` MUST 由该区域的**容量**派生
- **AND** MUST NOT 出现与容量无关的手写大小字面量

#### Scenario: 越界访问 MUST 报错

- **WHEN** 一次访问的地址落在**所有已声明区域之外**
- **THEN** MUST 产生**诊断**
- **AND** MUST NOT 静默丢弃地址高位（⇒ ⇒ 那会把访问导向错误区域）
- **AND** 投毒自证：把地址设到区域外 ⇒ ⇒ **MUST 有诊断**（不是静默通过）

#### Scenario: 预留区 MUST 只留位

- **WHEN** 映射表包含 MMIO 预留区
- **THEN** 该区 MUST **只占位**（声明 base / size）
- **AND** MUST NOT 接入任何设备

### Requirement: 固件装载 MUST 经**内存初值**完成，MUST NOT 要求新增装载端口

固件（`.text` + **`.data`**）MUST 能以**内存初值**的形式进入对应 TCM
⇒ ⇒ 载体 = `ch_mem::init_data`（构造参数）⇒ ⇒ 发射时落成 Verilog `initial` 块。

⇒ ⇒ 顶层**端口面 MUST 保持不变** ⇒ ⇒ **MUST NOT** 为装载新增 / 修改任何 `ch_in`
⇒ ⇒ 理由：装载是**发射期**的事，不是运行期协议；给 RTL 加装载口会把「端口契约」
和「固件内容」耦合在一起。

⇒ ⇒ **运行期装载口**（不重发射即可灌固件）**不在**本 capability 内 ⇒ ⇒ 若需要 ⇒ ⇒ 独立立项。

#### Scenario: `.text` 与 `.data` MUST 都能装载

- **WHEN** 一个固件同时含 `.text`（I-TCM）与 `.data`（D-TCM）
- **THEN** 两段 MUST 分别落进对应 TCM 的**初值镜像**
- **AND** MUST NOT 出现「`.data` 无处可装」的情况（⇒ ⇒ 修前 `prog_*` 只接 I-TCM）

#### Scenario: 产物 MUST 自带初值块

- **WHEN** 发射一个**带初值**的 SoC
- **THEN** 产物的对应内存 MUST 出现 `initial begin … end` 且**逐字**等于初值镜像
- **AND** 产物**不带初值**时 MUST **一条** init 赋值都没有
  （⇒ ⇒ 否则「初值只来自 `init_data`」不成立）

#### Scenario: 顶层端口面 MUST 不变

- **WHEN** 对比「带初值」与「不带初值」两次发射的产物
- **THEN** 两者的**顶层端口表** MUST **逐字相同**
- **AND** MUST NOT 出现装载专用端口

### Requirement: 装载后的值 MUST 走**真实**读通路可读回

装载完成后，判据 MUST 能断言「装进内存的值 == 经流水线读通路读回的值」
⇒ ⇒ **不是**「装载器返回成功」。

#### Scenario: `.data` 经 load 指令读回

- **WHEN** `.data` 的某字经初值镜像装载进 D-TCM
- **THEN** 一条 `lw` 从该地址读回 MUST 得到该值
- **AND** 该值 MUST 由**寄存器写回**观测到（⇒ ⇒ 不是直接读内存模型）

### Requirement: 固件转储器 MUST 产出**可寻址**的产物

转储器 MUST 支持至少三种输出格式：
`vh`（Verilog `$readmemh` 可读）· `mem`（纯 hex）· `manifest`（段 / 符号 / 入口 / 字索引映射）。

⇒ ⇒ 输入 MUST 是**固件清单**（含 `name` / `words[]` / `base_addr`）
⇒ ⇒ 输出 MUST **可寻址**（含基址信息）⇒ ⇒ 装载器 MUST NOT 靠猜。

#### Scenario: manifest MUST 携带基址与入口

- **WHEN** 转储器输出 `manifest` 格式
- **THEN** 输出 MUST 携带每个段的**基址**
- **AND** MUST 携带**入口地址**
- **AND** MUST 携带「字索引 → 地址」的映射（⇒ ⇒ 装载器 MUST NOT 靠猜）

### Requirement: ELF 装载 MUST 覆盖 `PT_LOAD` 且 MUST **报错**而非静默忽略异常

ELF 装载 MUST 解析 ELF32 header 与 program headers ⇒ ⇒ 取 `PT_LOAD` 段的
`p_paddr` / `p_filesz` / `p_memsz` ⇒ ⇒ 按映射表写入 ⇒ ⇒ `.bss`
（`p_memsz > p_filesz`）MUST **补零**。

⇒ ⇒ **不解析符号表**（本契约不要求）⇒ ⇒ **不做重定位**
⇒ ⇒ 若遇到**非零重定位**，MUST **报错** ⇒ ⇒ **MUST NOT** 静默忽略。

#### Scenario: `.bss` MUST 补零

- **WHEN** 装载一个 `p_memsz > p_filesz` 的 `PT_LOAD` 段
- **THEN** `p_filesz` 之后的字节 MUST 被写为 **0**
- **AND** MUST NOT 保留该区域的**旧内容**

#### Scenario: 非零重定位 MUST 报错

- **WHEN** 输入 ELF 含**非零**重定位项
- **THEN** 装载 MUST **报错**
- **AND** MUST NOT 静默忽略（⇒ ⇒ 静默忽略会产出「看起来装好了、实际地址全错」的固件）

### Requirement: 装载 MUST 有**可观测**的读回路径

装载完成后 MUST 提供「读回某地址」的路径，且该路径 MUST 走**真实**读通路
⇒ ⇒ 判据 MUST 能断言「装进去的值 == 期望值」⇒ ⇒ **不是**「loader 返回成功」。

#### Scenario: 读回 MUST 走真实读通路

- **WHEN** 一个值被装载进某地址
- **THEN** 通过**真实**读通路读回该地址 MUST 得到该值
- **AND** MUST NOT 以「loader 的返回值」作为「装载成功」的证据

### Requirement: SoC 的存储映射与 PC 复位值 MUST 一致

SoC MUST 显式声明其存储映射 ⇒ ⇒ 且 **PC 的复位值 MUST 等于映射中 I-TCM 的 `base`**
⇒ ⇒ 两者 MUST NOT 各自独立硬编码。

⇒ ⇒ 理由：二者**恰好**都是 `0x8000_0000` ⇒ ⇒ 任何一侧改动而另一侧未改
⇒ ⇒ **静默失配** ⇒ ⇒ 从零取指 ⇒ ⇒ 无任何报错。

#### Scenario: PC 复位值 MUST 等于 I-TCM 的 base

- **WHEN** SoC 声明了存储映射
- **THEN** PC 的复位值 MUST 等于映射中 **I-TCM 的 `base`**
- **AND** MUST NOT 出现两个**各自独立**的字面量（⇒ ⇒ 那会静默失配）
