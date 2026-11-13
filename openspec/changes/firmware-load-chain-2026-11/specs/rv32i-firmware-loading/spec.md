# Capability: rv32i-firmware-loading

## ADDED Requirements

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

### Requirement: 顶层加载接口 MUST 用**字节地址**且 MUST 覆盖**全部**数据区

顶层加载接口 MUST 携带 `(we, addr[**字节**地址], data[word], size)`
⇒ ⇒ 由映射表译码到 I-TCM 或 D-TCM ⇒ ⇒ **一次接口 MUST 能装两个区域**。

⇒ ⇒ ⚠️ 加载端口的**地址语义**（字节地址）MUST 在**公开面登记**里写明
⇒ ⇒ 旧的「字索引」语义 MUST NOT 残留。

#### Scenario: 一次装载 MUST 能覆盖 I-TCM 与 D-TCM

- **WHEN** 一个固件同时含 `.text`（I-TCM）与 `.data`（D-TCM）
- **THEN** 同一个加载接口 MUST 能把两者分别装进对应区域
- **AND** MUST NOT 出现「`.data` 无处可装」的情况

#### Scenario: 加载端口 MUST 具名

- **WHEN** 顶层新增或修改加载端口
- **THEN** 该端口 MUST 带**显式名**（`标识符 == 名字串`）
- **AND** MUST NOT 新增未具名的端口（⇒ ⇒ 否则端口命名棘轮判红）

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
