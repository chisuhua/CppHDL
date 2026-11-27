## ADDED Requirements

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
