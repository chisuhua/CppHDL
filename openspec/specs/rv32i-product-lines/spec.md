# rv32i-product-lines Specification

## Purpose
定义 RV32I 平台上**产品线**（product line）的**身份、归属与覆盖**契约。

平台可同时存在**多条产品线**（当前：`riscv-mini` = TCM 取指形态 · `riscv-full` = 总线化取指 +
cache 形态，见 ADR-078）。每条线在**自己的目录**里带一个 marker，作为该线的**唯一真相**
（标识 / 成员目录 / 关联 ADR / 关联 capability）；中央只有**派生物**（登记表），由 marker 生成。

本 capability 存在的原因是一个**具体的失败族**：纯自动发现（`examples/*/` glob）会让
「**新建了一条产品线却没登记**」变成**静默不覆盖** —— 新线的测试不被任何护栏扫到，而
「加线漏登记」在没有任何报错的情况下悄悄发生（与仓内 `VERILATOR_E2E_TOTAL` 等 CI 锚点
所防的「双后端覆盖静默缩面」同族）。因此本 capability 把「产品线的身份」写成
**可验证的 Requirement**（marker 完备性 / 归属完备性 / 覆盖完备性），而不是留给实现自由裁量。

适用对象：

- `examples/*/` 下**每条产品线**的 marker（`examples/<line>/.rddf-line.yaml`）
- 产品线元护栏（`scripts/verify_product_lines.sh` + `scripts/lib/product_lines.py`）
- 派生物登记面（`.rddf/checks/product-lines.md`）+ 生成器（`scripts/gen_product_lines.py`）

与既有 capability 的分工：

- `rv32i-isa-semantics` / `rv32i-data-forwarding` 等管辖**各产品线内部**的行为契约
  （本 capability **不**规定某条线的功能语义）；
- 本 capability 只管**线与线之间**的**身份 / 归属 / 覆盖**面 ⇒ ⇒ 一条线的功能判据
  （如 I-L1 的停顿契约）仍归该线自己的 capability，且**适用范围 MUST 显式标注**
  （见 `rv32i-data-forwarding` 的适用范围声明）。


## Requirements

### Requirement: 每条产品线 MUST 有 per-line marker 作为唯一真相

平台上的**每条产品线**（如 `examples/riscv-mini` 与 `examples/riscv-full-soc`）MUST 在其目录内
携带一个 marker 文件，记录该线的**唯一真相**：

- marker MUST 包含：标识（`line`）· 层（`layer` ∈ {`ip`, `soc`}）· 关联 ADR 列表 · 关联 capability 列表。
- `line` MUST 等于所在目录名 ⇒ ⇒ 目录改名 MUST 同步改 marker ⇒ ⇒ 改名**不可能静默发生**。
- 关联 ADR 列表中的每个 ADR MUST 对应**实际存在的文件**。
- 产品线的**身份**、**范围**、**关联决策** MUST NOT 由**任何集中式手写清单**承载 ⇒ ⇒
  集中面 MUST 是从 marker **派生**的。

#### Scenario: 目录改名而未改 marker

- **WHEN** 某产品线目录被改名而其 marker 的 `line` 仍为旧名
- **THEN** 产品线元护栏 MUST 报红
- **AND** MUST NOT 出现「marker 与目录名不一致但无任何护栏发现」的情形

#### Scenario: marker 引用不存在的 ADR

- **WHEN** 某 marker 的 `adrs` 列出一个**不存在**的 ADR 文件
- **THEN** 产品线元护栏 MUST 报红

### Requirement: 产品线登记面 MUST 是派生物且 MUST 有独立元护栏

产品线的**登记面**（产品线清单 · 锚点分组 · 脚本处置）MUST 是从 marker 与处置表**生成**的，
且 MUST 有**独立的元护栏**守护：

- 元护栏 MUST 判定「**发现集 == 登记集**」（marker 缺失 / 多出 / 生成物陈旧 ⇒ 红）。
- 元护栏 MUST 判定 marker 的**字段完备性**。
- 元护栏 MUST 判定**覆盖完备性**：每条产品线 MUST 被**至少一个**护栏登记面引用
  ⇒ ⇒ 「登记了但不覆盖」MUST 红 ⇒ ⇒ **半创建 / 忘登记的产品线 MUST 当场红**。

#### Scenario: 新建产品线目录但漏写 marker

- **WHEN** `examples/` 下出现一个产品线目录而无 marker
- **THEN** 产品线元护栏 MUST 报红（不得静默不覆盖）

#### Scenario: 登记面被手工编辑

- **WHEN** 登记面（生成物）的内容与 marker 派生的结果不一致
- **THEN** 生成器的 `--check` 模式 MUST 报红

### Requirement: 加一条产品线的成本 MUST 是「自有文件 + 登记条目 + 锚点归组重算」

平台 MUST NOT 存在「每加一条产品线就 MUST 手改某处集中式硬编码」的形态：

- **护栏脚本**的单线假设 MUST 被**逐个显式处置**（参数化 / 登记为「该线专属，按设计不改」）⇒ ⇒
  处置表 MUST **逐个列出**每个写死产品线路径的脚本与其处置 ⇒ ⇒ 漏列 MUST 红。
- **全局 CI 整数锚点**（测试总数 / 双后端覆盖数 / parity 规模 / 测试源数）MUST 按**产品线分组** ⇒ ⇒
  加一条线 MUST 只影响**自己那组**。

#### Scenario: 处置表漏掉一个写死路径的脚本

- **WHEN** 某个脚本仍写死某产品线路径而处置表未列出它
- **THEN** 产品线元护栏 MUST 报红

#### Scenario: 新增一条产品线

- **WHEN** 新增一条产品线并为其写 marker
- **THEN** 护栏脚本 MUST **零改动**即可正确处理该线
- **AND** MUST NOT 需要修改任何集中式硬编码清单

### Requirement: 每个测试目标 MUST 归属唯一一条产品线

CI 的**测试目标**（ctest target）MUST 归属**唯一**一条产品线：

- 归属 MUST 是**可机械判定**的（由 marker 派生的「该线拥有的测试目标清单」）。
- 未归属或归属多条的目标 MUST 报红 ⇒ ⇒ 「新测试忘了归线」MUST 当场红。
- 锚点分组 MUST 以**归属关系**为唯一计数口径 ⇒ ⇒ 分组值 MUST 与实际 `ctest -N` 一致。

#### Scenario: 新增测试目标但未归属产品线

- **WHEN** 新增一个 ctest target 且未在任何产品线的目标清单中登记
- **THEN** 产品线元护栏 MUST 报红
- **AND** 锚点分组校验 MUST 报红

### Requirement: 治理条款 MUST 标注其适用产品线

凡**仅对特定产品线成立**的治理条款（实现红线 · 能力矩阵 · 契约表）MUST 显式标注适用产品线：

- 标注 MUST 可被**机械检索**（grep 可判）。
- 平台引入第二条产品线时，既有「线无关」写法 MUST 被**逐处**补标注 ⇒ ⇒
  新会话读到该条款时 MUST 能判定它**是否适用于当前产品线**。

#### Scenario: 既有能力矩阵实为单线专属

- **WHEN** 某既有 capability 的矩阵 / 红线只对某条产品线成立
- **THEN** 该 capability MUST 携带产品线适用范围声明
- **AND** 平台 MUST NOT 让它以「线无关权威」的措辞存在
