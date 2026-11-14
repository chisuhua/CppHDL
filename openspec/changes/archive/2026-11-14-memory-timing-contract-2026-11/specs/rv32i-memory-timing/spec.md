# Capability: rv32i-memory-timing

## ADDED Requirements

### Requirement: 存储器的读 MUST 是**固定 1 拍的寄存读**，且 MUST NOT 是组合读

RV32I SoC 的 **I-TCM** 与 **D-TCM** 的 **load 数据通路** MUST 是**同一套时序语义**：
**固定 1 拍寄存读** ⇒ ⇒ 地址在第 N 拍给出 ⇒ ⇒ 数据在第 N+1 拍可用
⇒ ⇒ **MUST NOT** 是「地址与数据同拍」的组合读。


⇒ ⇒ ⚠️ **字节写的读-改-写是例外**（见下方 Scenario）—— 它 MUST 保持**同拍组合读**。

⇒ ⇒ 理由：组合读在产物里退化为「容量:1 组合 mux」⇒ ⇒ **无法推断 BRAM**
⇒ ⇒ 上板只能实现成分布式寄存器阵列 ⇒ ⇒ 面积与时序双输。

⇒ ⇒ ⚠️ 「固定 1 拍延迟」**不是握手**：读口**不等待**任何就绪信号
⇒ ⇒ MUST NOT 把本契约表述为「握手 / 等待 / 往返」。

#### Scenario: D-TCM 的读产物 MUST NOT 是组合 mux

- **WHEN** 一个含 D-TCM 的设计被发射成 Verilog
- **THEN** D-TCM 的 **load 数据通路** MUST 来自**寄存器**（`sread` 形态）
- **AND** 产物中 MUST NOT 存在「容量:1 组合读」（即 MUST NOT 出现把整个存储体
  按地址组合选通的连续赋值）
- **AND** 该形态 MUST 是 **BRAM 可推断**的前提
- **AND** ⚠️ **字节写的 RMW 读口** MAY 仍是组合读（**同拍**合并的必然要求）⇒ ⇒
  ⇒ ⇒ **判据 MUST 只看 load 通路**，不得据此要求整个 D-TCM 无组合读

#### Scenario: 字节写的读-改-写 MUST 保持同拍组合读

- **WHEN** `sb` / `sh` 在 D-TCM 上执行
- **THEN** 读-改-写的**读口** MUST 是 **0 拍组合读**（组合读返回**边沿前**的旧值
  ⇒ ⇒ 合并结果正是「旧字 + 新字节」）
- **AND** 同一个 `ch_mem` MUST **并存两个读口**（`aread` 给 RMW + `sread` 给 load 通路）
- **AND** ⚠️ 「把 `aread` 换成 `sread`」的单行改法 MUST 被判据**抓红**
  （⇒ ⇒ 它会**静默破坏** `sb` / `sh`：合并结果变成上一拍的值）

#### Scenario: I-TCM 与 D-TCM 的读时序 MUST 一致

- **WHEN** 取指（I-TCM）与数据访问（D-TCM）各自发起一次读
- **THEN** 两者 MUST 都返回**第 N+1 拍**的数据
- **AND** 取指侧的 `pc_lag` 延迟 MUST 与数据侧的装载延迟**同拍数**
- **AND** MUST NOT 出现「取指 1 拍 / 数据 0 拍」这类**不一致**的组合

### Requirement: 流水的 load 数据入端 MUST 是「**上一拍地址**所指字」的 **1 拍延迟读**

流水线的 `data_read_data`（MEM 区的 load 数据入端）MUST 是**固定 1 拍延迟读**的产物
⇒ ⇒ 在拍 **N+2** 采样时，其值 MUST = **拍 N+1 呈现的 `data_addr`** 所指的字
⇒ ⇒ 该值 MUST 与 **M2 元数据**（addr / funct3 / rd，全部是拍 N+1 的快照）**同拍配对**。

⇒ ⇒ 理由：M2 元数据在 posedge N+2 采「拍 N+1」的值；`sread` 输出寄存器**在同一条沿**
更新为 `mem[拍 N+1 的地址]` ⇒ ⇒ 二者是**同拍快照的同构投影** ⇒ ⇒ 配对是**结构性**的。

⇒ ⇒ ⚠️ **MUST NOT** 用「在 M2 里再插一个原始字寄存器」来满足本条
⇒ ⇒ 那会采到 `mem[拍 N 的地址]`（上一拍）⇒ ⇒ 与 M2 元数据**错配一拍**
⇒ ⇒ 后果 = `LB`/`LH` **静默错车道** · `LW` **静默读回别人的字** · **零报错**
⇒ ⇒ 依据 = `docs/adr/ADR-069-mem-data-alignment.md` §7.6（dated addendum）。

#### Scenario: 换存储体 MUST NOT 破坏 1 拍契约

- **WHEN** 把 `data_read_data` 的驱动从 TCM 的 `sread` 换成**任何**别的存储体
  （cache / 片外存储 / 可变延迟存储）
- **THEN** 新驱动 MUST 仍满足「上一拍地址所指字」的 **1 拍固定延迟**
- **AND** **MUST NOT** 是 0 拍组合读
- **AND** **MUST NOT** 是**可变延迟**（握手式 / 非确定拍数）
- **AND** 若不满足 ⇒ ⇒ MUST **显式重构**（例如加寄存器并**同步延后元数据**）
  ⇒ ⇒ ⚠️ **MUST NOT** 只加级而不延后元数据

#### Scenario: 判别力 MUST 被固件级用例钉住

- **WHEN** 有人把「原始字」改成从**滞后一拍**的源取值
- **THEN** 字节道 / 半字道的判别性用例 MUST **转红**
- **AND** 该用例 MUST 使用**非回文**的地址/数据（禁对称值）

### Requirement: MEM 级的访存数据 MUST 与访存地址**对齐**（决策 = **A0**）

MEM 级对访存结果的采样 MUST 与**访存地址的呈现拍**对齐
⇒ ⇒ **决策 = A0**：**访存地址的呈现拍照旧**（仍取自 EX/MEM 级寄存器）
⇒ ⇒ **MEM/WB 的装载点延后 1 拍**（EX/MEM → **M2** → MEM/WB）
⇒ ⇒ ⇒ **load-use 停顿由 3 拍修订为 4 拍**（EX→WB 级数 2 → 3 的结果值）。

⇒ ⇒ ⚠️ 停顿拍数契约 MUST 与「数据前推」capability 的 dated addendum 一致（**3 拍 → 4 拍**）
⇒ ⇒ **MUST NOT** 保留「3 拍」的旧表述。

#### Scenario: 三个强制结构变更 MUST 同批落地

- **WHEN** 本契约落地
- **THEN** 下列三项 MUST **同批**改动（漏任一 ⇒ ⇒ **静默错值**，且无任何报错）：

  | # | 强制项 |
  |---|---|
  | ① | 前推**档位 2** 的数据源 MUST 由 `memwb_*` 改指 **M2** |
  | ② | **WB→ID 组合旁路**（D11）的源 MUST 改指「下一提交者值」 |
  | ③ | **停顿链加一级** ⇒ ⇒ `hazard_unit` 组件内部 MUST 有对应改动 |

- **AND** 每项 MUST 各配**一条判别性用例**（⇒ ⇒ 漏改不可见）

#### Scenario: 地址呈现拍 MUST 保持不变

- **WHEN** 一条访存指令进入 MEM 级
- **THEN** 访存地址 MUST 仍取自 **EX/MEM 级寄存器**
- **AND** `obs_data_addr` 的**呈现拍** MUST NOT 因本契约而改变

#### Scenario: MEM/WB 的装载点 MUST 延后 1 拍

- **WHEN** MEM 级在第 N 拍发出访存地址
- **THEN** MEM/WB 的数据装载 MUST 发生在第 **N+1** 拍（经 M2 中转）
- **AND** 前推档位 2 的数据源 MUST 是 **M2** 的输出 ⇒ ⇒ MUST NOT 是 `memwb_*`
- **AND** WB→ID 组合旁路的源 MUST 是「下一提交者值」

#### Scenario: 停顿拍数 MUST 是 4 拍

- **WHEN** 一条 `load` 紧接着被下一条指令依赖
- **THEN** 停顿 MUST 是 **4** 拍
- **AND** MUST NOT 是 3 拍（那是旧基线）

### Requirement: 字节 / 半字写的**相位** MUST 显式定义

在寄存读时序下，写使能 / 写地址 / 写数据的**相位** MUST 显式定义：
**写使能 MUST 与被覆盖的那一拍地址对齐**。

⇒ ⇒ 判据 MUST 覆盖 `sb` / `sh` 的**四个字节车道 × 两个半字车道**组合
⇒ ⇒ 「只覆盖 2 个用例」**判别力不足**。

#### Scenario: 写使能 MUST 与被覆盖的地址同拍

- **WHEN** 一条 `sb` / `sh` 在第 N 拍写地址
- **THEN** 写使能 MUST 在**同一拍**有效
- **AND** 被写入的车道 MUST 由**该拍**的 `addr[1:0]` 决定

#### Scenario: 相位错位 MUST 被判据抓到

- **WHEN** 实现把写使能相对地址**错开一拍**
- **THEN** 字节道写相位判据 MUST **失败**（证明该判据有判别力）

### Requirement: MEM 级的反压接口 MUST 有**可观测**的语义

MEM 级暴露给上级的就绪信号 MUST 是一个**真实**的输入
⇒ ⇒ 上游拉低它时，MEM 级 MUST 冻结其装载寄存器并向前级发出 stall
⇒ ⇒ **有效链 MUST 保持**（不产生气泡误写）。

⇒ ⇒ ⚠️ 「固定 1 拍延迟」的读接口 **MUST NOT** 被表述为「握手」。

#### Scenario: 反压期间有效链 MUST 保持

- **WHEN** MEM 级被反压（就绪信号被拉低）且当前有效链非空
- **THEN** MEM/WB 寄存器 MUST 被冻结
- **AND** 有效链 MUST 保持自洽（不产生误写）
- **AND** 向前级 MUST 发出 stall

#### Scenario: 反压 MUST 有非零覆盖

- **WHEN** 判据集被检查
- **THEN** MUST 存在**能拉低该就绪信号**的 DUT 与用例
- **AND** MUST NOT 存在「该通路零覆盖但仍判绿」的状态
