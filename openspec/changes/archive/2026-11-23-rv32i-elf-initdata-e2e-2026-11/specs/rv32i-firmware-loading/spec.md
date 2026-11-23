# Capability: rv32i-firmware-loading

## MODIFIED Requirements

### Requirement: 固件装载 MUST 经**内存初值**完成，MUST NOT 要求新增装载端口

固件（`.text` + **`.data`**）MUST 能以**内存初值**的形式进入对应 TCM
⇒ ⇒ 载体 = `ch_mem::init_data`（构造参数）⇒ ⇒ 发射时落成 Verilog `initial` 块。

⇒ ⇒ 顶层**端口面 MUST 保持不变** ⇒ ⇒ **MUST NOT** 为装载新增 / 修改任何 `ch_in`
⇒ ⇒ 理由：装载是**发射期**的事，不是运行期协议；给 RTL 加装载口会把「端口契约」
和「固件内容」耦合在一起。

⇒ ⇒ **运行期装载口**（不重发射即可灌固件）**不在**本 capability 内 ⇒ ⇒ 若需要 ⇒ ⇒ 独立立项。

⇒ ⇒ **初值镜像 MUST 可由 ELF 派生**（`PT_LOAD` 段 ⇒ ⇒ 按区域拆分）⇒ ⇒ 转换器 MUST
以**区域内的字索引**为镜像下标（空洞 MUST 补零），且字节地址 → 区域字索引的换算
MUST 在**唯一一处**完成（禁各处自行解释）。

⇒ ⇒ **SoC 级产品单元 MUST 也能携带初值**（此前只有 IP 级能带 ⇒ ⇒ 产品单元只能跑
纯 `.text` 程序）⇒ ⇒ 该能力同样 MUST 经**构造参数**进入 ⇒ ⇒ **MUST NOT** 因此新增端口。

#### Scenario: `.text` 与 `.data` MUST 都能装载

- **WHEN** 一个固件同时含 `.text`（I-TCM）与 `.data`（D-TCM）
- **THEN** 两段 MUST 分别落进对应 TCM 的**初值镜像**
- **AND** MUST NOT 出现「`.data` 无处可装」的情况

#### Scenario: 产物 MUST 自带初值块

- **WHEN** 发射一个**带初值**的 SoC
- **THEN** 产物的对应内存 MUST 出现 `initial begin … end` 且**逐字**等于初值镜像
- **AND** 产物**不带初值**时 MUST **一条** init 赋值都没有
  （⇒ ⇒ 否则「初值只来自 `init_data`」不成立）

#### Scenario: 顶层端口面 MUST 不变

- **WHEN** 对比「带初值」与「不带初值」两次发射的产物
- **THEN** 两者的**顶层端口表** MUST **逐字相同**
- **AND** MUST NOT 出现装载专用端口

#### Scenario: ELF 派生的镜像 MUST 能进 init_data 并被执行

- **WHEN** 一个 ELF 被转换成按区域拆分的初值镜像并被装载进核
- **THEN** 核取指到的第一条指令 MUST 等于 I-TCM 镜像的首字（⇒ ⇒ 证明取指**来自镜像**）
- **AND** `.data` 的值 MUST 能经 `lw` 读回并被**寄存器写回**观测到
- **AND** 镜像的行号 MUST 等于该区域内的**字索引**（空洞补零 ⇒ ⇒ 索引与地址不错位）
- **AND** 投毒自证：改坏镜像首字 / 改坏 `.data` 的值 / 去掉 `.data` 段
  ⇒ ⇒ 判据 MUST 各自转红

### Requirement: 固件转储器 MUST 产出**可寻址**的产物

转储器 MUST 支持至少四种输出格式：
`vh`（Verilog `$readmemh` 可读）· `mem`（纯 hex）· `manifest`（段 / 符号 / 入口 / 字索引映射）
· `mem-split`（**按区域拆分**的字镜像）。

⇒ ⇒ 输入 MUST 是**固件清单**（含 `name` / `words[]` / `base_addr`）
⇒ ⇒ 输出 MUST **可寻址**（含基址信息）⇒ ⇒ 装载器 MUST NOT 靠猜。

⇒ ⇒ `mem-split` 的**行号 MUST 等于区域内的字索引** ⇒ ⇒ 空洞 MUST 补零
（否则索引与地址错位 ⇒ ⇒ 固件被装到**错误**的位置）。

⇒ ⇒ `mem` / `vh` 这两种**无地址**字流遇**多区域**固件时 MUST **报错**
⇒ ⇒ **MUST NOT** 静默拼接（单条无地址的字流无法表达「哪一段属于哪个区域」
⇒ ⇒ 消费者 MUST NOT 靠猜）。

#### Scenario: manifest MUST 携带基址与入口

- **WHEN** 转储器输出 `manifest` 格式
- **THEN** 输出 MUST 携带每个段的**基址**
- **AND** MUST 携带**入口地址**
- **AND** MUST 携带「字索引 → 地址」的映射（⇒ ⇒ 装载器 MUST NOT 靠猜）
- **AND** 固件覆盖的**每一个**区域 MUST 都被登记（⇒ ⇒ 双区域固件的登记数 MUST == 2）

#### Scenario: 多区域固件 MUST 用带地址语义的格式

- **WHEN** 一个固件同时覆盖 I-TCM 与 D-TCM
- **THEN** `mem` / `vh` 输出 MUST **失败**（诊断点名「多区域」）
- **AND** `mem-split` / `manifest` MUST **成功**
- **AND** 单区域固件的 `mem` 输出 MUST **仍然成功**（⇒ ⇒ 拒绝条件 MUST 是「多区域」而不是「一律拒绝」）

### Requirement: ELF 装载 MUST 覆盖 `PT_LOAD` 且 MUST **报错**而非静默忽略异常

ELF 装载 MUST 解析 ELF32 header 与 program headers ⇒ ⇒ 取 `PT_LOAD` 段的
`p_paddr` / `p_filesz` / `p_memsz` ⇒ ⇒ 按映射表写入 ⇒ ⇒ `.bss`
（`p_memsz > p_filesz`）MUST **补零**。

⇒ ⇒ **不解析符号表**（本契约不要求）⇒ ⇒ **不做重定位**
⇒ ⇒ 若遇到**非零重定位**，MUST **报错** ⇒ ⇒ **MUST NOT** 静默忽略。

⇒ ⇒ 重定位的**判据载体 MUST 可执行**：ELF32 的重定位表在 **section header** 里
（program header **看不到**）⇒ ⇒ 检查 MUST 读 section header ⇒ ⇒
存在**非空**的 `SHT_RELA` / `SHT_REL` 节 ⇒ ⇒ MUST **报错**。
（`e_shnum == 0`（节头被 strip）⇒ ⇒ 无从查起 ⇒ ⇒ 接受 ⇒ ⇒ 属**已知边界**。）

⇒ ⇒ **入口 MUST 等于 PC 复位值**（= I-TCM 的 `base`）⇒ ⇒ 否则 MUST **报错**
⇒ ⇒ 理由：核的 PC 复位值硬连线为 I-TCM 的 `base` ⇒ ⇒ 入口落在别处 ⇒ ⇒ 核从
`base` 取指 ⇒ ⇒ 装出来的固件**无法启动**，且 I-TCM 初值 `0` 是**未知 opcode**
⇒ ⇒ 流水线在首条未知指令处**冻结** ⇒ ⇒ **零报错**（静默失配）。

#### Scenario: `.bss` MUST 补零

- **WHEN** 装载一个 `p_memsz > p_filesz` 的 `PT_LOAD` 段
- **THEN** `p_filesz` 之后的字节 MUST 被写为 **0**
- **AND** MUST NOT 保留该区域的**旧内容**

#### Scenario: 非零重定位 MUST 报错

- **WHEN** 输入 ELF 含**非空**的 `SHT_RELA` / `SHT_REL` 节
- **THEN** 装载 MUST **报错**（诊断点名「重定位」）
- **AND** MUST NOT 静默忽略（⇒ ⇒ 静默忽略会产出「看起来装好了、实际地址全错」的固件）
- **AND** **空**的重定位节（`sh_size == 0`）MUST **不影响**装载（⇒ ⇒ 拒绝条件 MUST 是「非空」）

#### Scenario: 入口 MUST 等于 PC 复位值

- **WHEN** 输入 ELF 的 `e_entry` ≠ I-TCM 的 `base`
- **THEN** 装载 MUST **报错**（诊断点名「入口」）
- **AND** 同一内容的 ELF 在入口改对后 MUST **成功**（⇒ ⇒ 拒绝条件 MUST 是「入口」而不是别的）
