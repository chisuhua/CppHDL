## REMOVED Requirements

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

## ADDED Requirements

### Requirement: RV32I 层次化交付物的成层粒度 MUST 是声明式的（模块类型名**不是**契约）

RV32I 流水线的**交付级**层次化产物 MUST 由**声明式**粒度产生（MUST NOT 依赖
`Granularity::ForceLayer` 这类逃生门）：

- `Rv32iPipeline` 的 **7 个 stage 成员** MUST 用**实例级**成层声明（`ch_layer` +
  显式模块类型名）⇒ ⇒ 成层集合 MUST NOT 由框架按端口数自动推断。
- `load_lane`（B3 之后新增的选择器）**不**在该 7 个之内 ⇒ MUST 保持 inline
  （把成层集合扩到它 = 新增第 8 个契约名 ⇒ 属**决策**变更）。
- **交付粒度**下产物 MUST 含 **8 个 module**（7 stage + 顶层 `top`）。
- 下列模块类型名**不是契约**（改名 **不**构成破坏性变更）：
  `IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` ·
  `ForwardingMux<32>`（经 `sanitize_verilog_name` ⇒ 产物中为 `ForwardingMux_32_`）。
  ⚠️ **裁决（用户 · 2026-11-15 · ADR-070 §6 ⑦ 的待裁决项）** = **明文非契约**
  ⇒ ⇒ 本 capability MUST 写明这一点；改名的**唯一**义务 = 让产物**重新 lint 干净**
  （它们仍 MUST 是合法 Verilog 标识符）。
  ⚠️ 稳定性来源是**成层粒度**（`ch_layer` 7 处）而**不是**名字：名字可随重构变化。
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
