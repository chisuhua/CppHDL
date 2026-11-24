## ADDED Requirements

### Requirement: 核 MUST 以 AXI4-Lite 主口作为 MMIO 通道

RV32I 核复合体 MUST 暴露一条 **AXI4-Lite 主口**（19 根）作为它的 **MMIO 通道**：

- 端口集合 MUST 是 AXI4-Lite 的 **m0 面**（与 `AxiLiteMatrix2x2` 的 `m0_*` 同型）
  ⇒ ⇒ `awaddr` / `awprot` / `awvalid` / `awready` · `wdata` / `wstrb` / `wvalid` / `wready`
  · `bresp` / `bvalid` / `bready` · `araddr` / `arprot` / `arvalid` / `arready`
  · `rdata` / `rresp` / `rvalid` / `rready`
- MUST **无 ID**（Lite 无 ID）· MUST **无 `awlen` / `arlen`**（Lite 无 burst）
  · MUST **无 `rlast` / `wlast`**（Lite 无）
- `awprot` / `arprot` MUST **恒为 0**（平台无特权区分）
- `bready` / `rready` MUST **恒为高**（核侧 master 无背压需求）
  ⇒ ⇒ **无背压**是**契约面**（不是实现细节）⇒ ⇒ 若平台将来要背压
  ⇒ ⇒ MUST 改核侧 master 的 `ready` 语义
- 端口名 MUST 带 `axil_` 前缀（`axil_awaddr` …）⇒ ⇒ 产物名 = `top_core_axil_*`

#### Scenario: 核端口面随通道一起纳入契约

- **WHEN** 核暴露 MMIO 通道
- **THEN** 核的产品端口面 MUST 从 **34** 变为 **53**（框架 2 + 核自有 51 = 12 IN + 39 OUT）
- **AND** 该端口面 MUST 由核侧的**端口契约表**钉住
  （⇒ ⇒ 契约表、C6 快照、护栏期望表 MUST **同批**重算）

#### Scenario: 通道地址 MUST 是**绝对地址**（不重定基）

- **WHEN** 核发起 MMIO 访问
- **THEN** `axil_awaddr` / `axil_araddr` MUST 是**绝对字节地址**
- **AND** 外设看到的 MUST 是同一地址（⇒ ⇒ 与 Lite 互联的「不重定基」语义一致
  ⇒ ⇒ 与外设自身的低位译码兼容）

### Requirement: MMIO 段 MUST NOT 由 D-L1 服务

数据访问落在 **MMIO 段**时 MUST **绕开** D-L1：

- 理由（**硬要求**，非优化）：D-L1 的 fill 完成条件 = `r_fire & fill_active & r_last`
  ⇒ ⇒ **必须**收到 `r_last`；而 AXI4-Lite **没有 `rlast`**
  ⇒ ⇒ MMIO 若走 D-L1 ⇒ fill 永不完成 ⇒ `ready` 恒 0 ⇒ **流水线永久冻结**
  （⇒ ⇒ **死锁**，不是「读到陈旧值」）
- ⇒ ⇒ **MMIO 段的访问 MUST NOT 发起 cache fill**

#### Scenario: MMIO 读 MUST 走旁路（否则挂死）

- **WHEN** 一条 load 的地址落在 MMIO 段
- **THEN** 该 load MUST NOT 进入 D-L1 的 fill 流程
- **AND** MUST 在 MMIO 通道上完成 ⇒ ⇒ 程序 MUST 继续执行
  （⇒ ⇒ 挂死 ⇒ 「程序不再前进」⇒ ⇒ 判据可观测）

#### Scenario: MMIO 写 MUST 走旁路

- **WHEN** 一条 store 的地址落在 MMIO 段
- **THEN** 该 store MUST 在 MMIO 通道上完成
- **AND** MUST NOT 触发 D-L1 的行分配（⇒ ⇒ **不被缓存**）

### Requirement: 分流判据 MUST 派生自库层地址表

核内的 **pre-cache 分流**判据 MUST **派生自**库层地址表
（`include/cpu/riscv/rv32i_mem_map.h`），MUST NOT 手写字面量：

- 判据 MUST 是「**地址 ∈ MMIO 段**」（MMIO 段的并集区间）⇒ ⇒ 是 ⇒ **旁路**
  · 否 ⇒ **现有 D-L1 通路**
- ⚠️ **判据方向 MUST 是「MMIO 段」而不是「TCM 段」**
  ⇒ ⇒ 取反形式会让**不在 TCM 窗口内**的地址（例如字节地址 0）
  落进 MMIO 旁路 ⇒ ⇒ **无 MMIO 通路的核级环境会永久挂死**
- ⇒ ⇒ **非 MMIO 段的一切访问（含 TCM 段、含越界访问）MUST 保持原有路径**
  ⇒ ⇒ **分流 MUST NOT 改变任何既有访问的行为**

#### Scenario: 非 MMIO 段的访问 MUST 逐拍不变

- **WHEN** 一条访存落在 MMIO 段**之外**
- **THEN** 它 MUST 走**与分流前完全相同**的通路与拍数
- **AND** 该设计的既有判据 MUST **逐条不变**

#### Scenario: 判据 MUST 单一真相

- **WHEN** 库层地址表的 MMIO 段发生变化
- **THEN** 核内的分流判据 MUST **自动跟随**（⇒ ⇒ 禁手写字面量）

### Requirement: MMIO 通道 MUST 是**单拍语义**的 AXI4-Lite 事务

- MMIO 通道上的每个事务 MUST 是 **AXI4-Lite 单拍**（无 burst）
  ⇒ ⇒ 平台侧即使有 full↔lite 桥（强制 `LEN=0` / `BURST=INCR`）也天然单拍
- ⇒ ⇒ **通道本身 MUST NOT 承载 burst**

#### Scenario: MMIO 路径的 `data_ready` = 「事务完成的那一拍」

- **WHEN** 一条访存走 **MMIO 旁路**
- **THEN** `data_ready` MUST 在**该事务完成的那一拍**为 1
- **AND** MUST NOT 在事务完成前为 1（⇒ ⇒ 否则流水线会**读到陈旧数据**）
- **AND** 该口径 MUST 与 **D-L1 路径**的 `data_ready` **同口径**
  （⇒ ⇒ D-L1 路径的 `data_ready` = `!data_req | cache.ready`（`rv32i-dcache-timing`）
  ⇒ ⇒ 它的「完成拍」语义在 miss 时同样会拉低多拍
  ⇒ ⇒ ⇒ 两条路径**零口径差异** ⇒ ⇒ 流水线侧的冻结逻辑**无需分支**）

#### Scenario: 单拍事务的可观测后果

- **WHEN** 核发起一次 MMIO 读
- **THEN** 该事务 MUST 产生**一次**读响应（`rvalid` 一个上升沿）
- **AND** MUST NOT 产生 burst 的多个 beat
