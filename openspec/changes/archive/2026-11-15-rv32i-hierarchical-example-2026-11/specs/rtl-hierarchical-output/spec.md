## ADDED Requirements

### Requirement: 同一子输出被「兄弟子模块」与「父体表达式」同时消费时 MUST 可发射（双类消费者）

同一个子模块的输出端口可能同时有**两类**消费者：
① 兄弟子模块的**输入端口** ② 父 module 体内的**运算表达式 / 父 `output` 端口**。

发射器 MUST 把该形态发射成合法产物，而 MUST NOT 判为「双驱动」：

- **pin 的实参 MUST 是承载该值的网名**，MUST NOT 是分析层记的**消费点名字**
  （消费点不是网 ⇒ 两者**必然**不等 ⇒ 用消费点判冲突必然误拒）。
- 兄弟边 MUST 在**共同祖先 module** 内声明**一根中转网**（`logic`），驱动端实例 pin 接它、
  被驱动端实例 pin 也从它取 ⇒ ⇒ 该 pin 在实例语句里 MUST **恰好出现一次**。
- 父侧消费点为**父 `output` 端口**时 MUST 由显式 `assign` 取该网的值（决策⑤：同一输出
  扇出**同一根**网），MUST NOT 丢连接、MUST NOT 产生重复 pin。
- 「哪些 pin 的**分析层连接行被改接**到父侧落点网」MUST 由 `HierarchyEmitResult` 的
  **结构化字段**（`rewired_pins`）给出 ⇒ ⇒ MUST NOT 只能靠日志（`CHWARN`）观测。

#### Scenario: 兄弟边 + 父 `output` 端口共享一个子输出

- **WHEN** 子 `A` 的输出既驱动兄弟 `B` 的输入，又被父 `output` 端口观测
- **THEN** 产物 MUST 成功生成
- **AND** `A` 的该 pin 在实例语句里 MUST 恰好出现**一次**（接中转网）
- **AND** 父 `output` 端口 MUST 由 `assign <port> = <中转网>;` 取值
- **AND** `rewired_pins` MUST 点名该 pin

#### Scenario: 子输出 fan-out 到**不同共同祖先**下

- **WHEN** 同一子输出被**不同 LCA** 下的两个兄弟消费
- **THEN** 发射 MUST **失败**（该形态不在本机制的适用面内）
- **AND** 诊断 MUST **点名**涉及的边（MUST NOT 只给计数）

#### Scenario: 反向不变式 —— 父驱动子输入的双驱动 MUST 永远失败

- **WHEN** 同一子 **input** pin 出现两个不同源的驱动（`into_child == true` 的冲突）
- **THEN** 发射 MUST **失败**
- **AND** 该判据 MUST NOT 因「方向感知」的放宽而被跳过
  ⇒ ⇒ IR 的源赋值是 last-wins ⇒ 放行等于**静默丢掉一个驱动**

### Requirement: RV32I 层次化交付物的成层粒度与模块类型名 MUST 是产物级契约

RV32I 流水线的**交付级**层次化产物 MUST 由**声明式**粒度产生（MUST NOT 依赖
`Granularity::ForceLayer` 这类逃生门）：

- `Rv32iPipeline` 的 **7 个 stage 成员** MUST 用**实例级**成层声明（`ch_layer` +
  显式模块类型名）⇒ ⇒ 成层集合 MUST NOT 由框架按端口数自动推断。
- `load_lane`（B3 之后新增的选择器）**不**在该 7 个之内 ⇒ MUST 保持 inline
  （把成层集合扩到它 = 新增第 8 个契约名 ⇒ 属**决策**变更）。
- **交付粒度**下产物 MUST 含 **8 个 module**（7 stage + 顶层 `top`）。
- 下列模块类型名 MUST 是**产物级契约名**（改名 = 产物级破坏性变更）：
  `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` ·
  `ForwardingMux<32>`（经 `sanitize_verilog_name` ⇒ 产物中为 `ForwardingMux_32_`）。
  ⚠️ 该「契约名 vs 明文非契约」的**强度**在 ADR-070 §6 ⑦ 仍是待裁决项
  ⇒ ⇒ 若改判为「非契约」⇒ 本节 MUST 同步写明「这些名字不是契约」。
- **扁平产物 MUST 零扰动**（成层声明只写标志位 ⇒ 结构性恒真 ⇒ 该判据只作跳线，
  MUST NOT 被当作承重证据）。

#### Scenario: 交付粒度下的模块面

- **WHEN** 以默认粒度（`RespectDeclarations`）发射 RV32I 顶层
- **THEN** 发射 MUST 成功且产物含 8 个 `module`
- **AND** 产物 MUST 含 `IfStage` / `IdStage` / `ExStage` / `MemStage` / `WbStage` /
  `HazardUnit` / `ForwardingMux_32_` 七个 module
- **AND** MUST NOT 含 `LoadLaneSelector` module（它是 inline）

#### Scenario: 顶层端口表与扁平产物的关系

- **WHEN** 对比层次化产物与扁平产物的顶层端口表
- **THEN** **根 `io()` 端口段** MUST **逐字逐序**相同
- **AND** 层次化侧**其余**顶层端口 MUST 在扁平侧**逐字存在**（集合 ⊆）
- **AND** 扁平侧 MAY 多出**成层子组件**的 `ch_out`（它们在层次化顶层**不可观测**）

#### Scenario: 静态检查门

- **WHEN** 对交付产物跑 `verilator --lint-only --top-module top`
- **THEN** MUST 退出码 0 且 MUST NOT 出现 `%Error`
- **AND** MUST NOT 使用 `-Wall`、`-Wno-fatal` 或任何 `-Wno-<单项>` 规避
  ⇒ ⇒ 产物 MUST 自身 lint 干净
