# examples-rtl-sim-coverage Specification

## Purpose
定义 CppHDL **examples 的 RTL 仿真产品面**的登记与自证契约 —— 一个产品单元
（example / 顶层 DUT）要成为「已验证的 RTL 产品」，MUST 满足什么：

- **登记**：每个单元 MUST 在 `.rddf/checks/examples-rtl-sim-targets.tsv` 登记
  （`kind` / `emitter` / `emitter_args` / `tb` / `status` / `generated` /
  `min_checks` …）⇒ ⇒ 「覆盖度」才有唯一真相（而不是散落在各处的人工名单）
- **列号推导**：清单的列号 MUST 从**机器可读表头**（`#COLUMNS`）推导
  ⇒ ⇒ 中间插列不得静默错列
- **三方等式**：断言条数 MUST 满足「**推导层登记** == **验证源静态展开** ==
  **运行期 `checks`**」⇒ ⇒ 「生成器少发断言」这类缺陷抓得住（单靠下界抓不到）
- **新鲜度 / 双向键 / 无残留 / 可判定生成物**：产物与验证源之间 MUST 有
  **可机械判定**的关系 ⇒ ⇒ 「TB 是手抄的」不再是「看不出来」

**为什么需要这条契约**：RTL 仿真产品面此前**只有名字**（一个 example 目录），
没有「它到底验了什么、验到什么程度」的**可机械判定**表述 ⇒ ⇒ 覆盖度只能靠人
相信。本 capability 把「产品面」变成**可统计、可复算、可自证**的东西
（载体 = `scripts/verify_examples_rtl_sim.sh` + `scripts/verify_examples_rtl_sim_coverage.sh`）。

## Requirements

### Requirement: 断言条数 MUST 为三方等式

每个产品单元的断言条数 MUST 满足**三方等式**：

1. **推导层登记条数**（`.rddf/checks/*.md` 中由**人**登记，MUST NOT 由生成器输出回填）
2. **验证源静态展开条数**（`.verif` 的 `EXPECT` 经 `REPEAT` 展开后的计数）
3. **运行期条数**（TB 打印的 `RESULT: checks=<N>`）

⇒ ⇒ 三者 MUST **相等**（等式，**不是下界**）。

⇒ ⇒ ⚠️ 若取值由生成器输出反填 ⇒ ⇒ 生成器少发断言时两侧同时变化 ⇒ ⇒ 判据**恒绿** ⇒ ⇒
本条的**唯一合法取值来源**是推导层登记。

#### Scenario: 任一方向偏差即红

- **WHEN** `.verif` 少一条 `EXPECT`（推导层未变）
- **THEN** 判据 MUST 转红（三方等式破裂）
- **AND** 当运行期 `checks` 因提前 `$fatal` 而变小 ⇒ ⇒ 同样 MUST 转红

### Requirement: 生成物 MUST 有新鲜度判据

由验证源生成的 TB MUST 可**逐字节重现** ⇒ ⇒ 对同一验证源 + 同一生成器版本重新生成，
产物 MUST 与原仓内文件**逐字节相同**。

⇒ ⇒ 违反 MUST 报错并提示「生成物与源不一致」（⇒ ⇒ 防「人改生成物忘改源」）。

#### Scenario: 手改生成物即红

- **WHEN** 人工修改仓内生成的 `.sv` 而未改 `.verif`
- **THEN** 新鲜度判据 MUST 转红
- **AND** 由生成器自身 bug 导致的「两侧同错」**不受本条保护** ⇒ ⇒ 由**条数三方等式**承担

### Requirement: 期望源与验证源 MUST 双向键集合一致

推导层（`.rddf/checks/*.md`）的 `scenario` 锚点与验证源（`.verif`）的 `scenario`
MUST **双向 1:1** ⇒ ⇒ 缺任一侧即违反。

⇒ ⇒ ⚠️ **粒度 = `scenario`**（不是逐条 `EXPECT`）⇒ ⇒ 否则 `.md` 退化成 key-value 镜像，
推导层名存实亡。

⇒ ⇒ ⚠️ **本条只判「键集合」** ⇒ ⇒ **数值一致性 MUST NOT** 被本条的通过状态所承诺。

#### Scenario: 悬空锚点 / 悬空 scenario 各自转红

- **WHEN** `.md` 有一个 `scenario` 锚点但 `.verif` 无对应 scenario
- **THEN** 判据 MUST 转红
- **AND** 反向（`.verif` 有 scenario 但 `.md` 无锚点）MUST 同样转红

### Requirement: TB MUST 无未完成残留

生成的 TB MUST 无未完成残留 ⇒ ⇒ ① `// TODO:` 计数 MUST 为 **0**；② 场景数 MUST ≥ **1**。

⇒ ⇒ 两条都 MUST 具备 ⇒ ⇒ 只判「无 TODO」时，**完全未生成**的产物会因「没有 TODO」而蒙混。

#### Scenario: 空 TB 不得通过

- **WHEN** 生成器产出仅含端口连线与时钟、无任何断言的 TB
- **THEN** 判据 MUST 转红（场景数 < 1）
- **AND** 产物内每个断言行 MUST 带 `check_id`

### Requirement: TB MUST 可判定为「生成物」

TB MUST 可按**产物内容**判定其来自生成器 ⇒ ⇒ 判据：① 文件内 MUST NOT 出现
`<根 module 名>_unnamed_*` 形态的字面量；② DUT 实例连线数 MUST 等于
`manifest.port_table[]` 的条目数，且逐条端口名 MUST ∈ `port_table[]`。

⇒ ⇒ 两条 MUST 同时满足 ⇒ ⇒ 只判「无未命名」时，「什么都没连」也会通过。

#### Scenario: 手写映射表残留即红

- **WHEN** TB 内出现 `top_unnamed_input_3` 这类字面量
- **THEN** 判据 MUST 转红
- **AND** 当 DUT 实例连线数少于 `port_table[]` 条目数时 MUST 同样转红

### Requirement: 生成覆盖度 MUST 登记且三方一致

清单（`.rddf/checks/examples-rtl-sim-targets.tsv`）MUST 登记每个产品单元的
「TB 是否由生成器产出」⇒ ⇒ 该登记 MUST **三方一致**：

1. 清单列值
2. TB 内**生成器标记**是否存在
3. 对应 `.verif` 验证源是否存在

⇒ ⇒ 任一侧单独变化 MUST 转红（⇒ ⇒ 这是「手写 TB 逃生口」的**唯一机械抓手**）。

⇒ ⇒ 逃生（`generated=no`）MUST 在清单的 `note` 列写明**非占位符**原因。

#### Scenario: 逃生口必须有理由

- **WHEN** 某行的 `generated=no`
- **THEN** 其 `note` MUST 含非占位符的具体原因
- **AND** 当该行 `generated=no` 但对应 `.verif` 存在 ⇒ ⇒ MUST 转红（应改为生成）

### Requirement: 清单列号 MUST 从机器可读表头推导

清单的消费方 MUST 从 `#COLUMNS:` 行**推导列号** ⇒ ⇒ **MUST NOT** 按位置绑定
（`read -r a b c … note` 之类）。

⇒ ⇒ 理由：按位置绑定时，**在中间插入一列**会静默错列（末列吸收剩余字段）⇒ ⇒ 判据
「看起来还在跑，实际读错了列」。

#### Scenario: 中间插列仍须正确

- **WHEN** 在清单中间插入一列并同步 `#COLUMNS`
- **THEN** 全部消费方 MUST 仍读到正确字段（判据 MUST 保持绿）
- **AND** 若某消费方按位置绑定 ⇒ ⇒ 该实现 MUST 判定为不满足本条

### Requirement: C++ 侧 MUST 与生成式 TB 共享同一验证源

**由 `.verif` 驱动**的产品，其 **C++ 侧（解释器路径）** 与 **Verilog 侧（渲染式 TB）** MUST 由
**同一份**验证源（`.verif`）驱动 ⇒ ⇒ 两侧断言**不可能**漂移。

⇒ ⇒ **作用域限定**：本条只约束**已迁移**（`.verif` 驱动）的产品 ⇒ ⇒ 仍在手写激励的产品
**不受**本条约束（迁移 = 独立 change）。

⇒ ⇒ **MUST NOT** 在 C++ 侧另写一份激励 / 断言 / 期望值（第二份期望载体）；
**MUST NOT** 存在「只打印不判定」的检查器（打印期望值但不影响退出码）。

⇒ ⇒ **MUST fail-closed**：验证源打不开 / 扫描器报错 / 端口清单有诊断 ⇒ **非零退出** +
stderr 说明，且 **MUST NOT** 打印结论行。

⇒ ⇒ 「断言条数 MUST 为三方等式」归本 capability 的既有 requirement ⇒ ⇒ 本条**不重述**。

#### Scenario: 期望载体只有一份

- **WHEN** 检视某产品的 C++ 侧实现
- **THEN** 它的激励与断言 MUST 来自 `.verif`（逐字同一文件）
- **AND** MUST NOT 存在第二份期望值载体（手写检查器 / 独立断言表）
- **AND** 该实现 MUST NOT 访问端口（无 `set_input_value` / `get_port_value` / `.io().`）
- **AND** 运行 MUST NOT 落任何文件（零 `.v` / 零 `.vcd`）

#### Scenario: 只打印不判定的检查器视为违规

- **WHEN** C++ 侧某项检查只输出文本、其结论不影响进程退出码
- **THEN** 该实现 MUST 判为**不满足**本条
- **AND** 该检查 MUST 改为断言（失败 ⇒ 非零退出）

#### Scenario: 验证源不可用必须响亮失败

- **WHEN** 验证源文件不存在 / 扫描器报错 / 端口清单有诊断
- **THEN** 退出码 MUST **非零**
- **AND** stderr MUST 有可行动说明
- **AND** stdout MUST NOT 含结论行（不得出现「0 条断言且退出 0」）

### Requirement: 由验证源驱动的 example 断言失败 MUST 以非零退出码结束

**由 `.verif` 驱动**的 example，其退出码 MUST 反映验证结论：断言失败或执行不完整 ⇒
**非零**；全部通过且条数自洽 ⇒ **零**。

⇒ ⇒ **作用域限定**：本条**只**约束 `.verif` 驱动的 example ⇒ ⇒ 已登记但**尚未迁移**的
example 产品**不受**本条约束（迁移 = 独立 change）。

⇒ ⇒ **MUST** 打印机器可读结论行（`RESULT: checks=<N> fails=<M>`）⇒ ⇒ **十进制**、
单行、无 ANSI 颜色 ⇒ ⇒ 与 Verilog 侧同协议，两后端结论可直接比对。

⇒ ⇒ 「条数自洽」= 运行期条数 MUST 等于扫描器静态展开的条数（防提前中止/少跑）。

#### Scenario: 断言失败 ⇒ 非零退出

- **WHEN** 验证源里存在一条**必然失败**的断言
- **THEN** 退出码 MUST **非零**
- **AND** 结论行 MUST 报出失败的 `check_id`

#### Scenario: 断言全过 ⇒ 零退出

- **WHEN** 验证源的全部断言通过
- **THEN** 退出码 MUST 为 **0**
- **AND** 结论行的 `fails` MUST 为 **0**
- **AND** 结论行的 `checks` MUST 等于扫描器静态展开的条数

#### Scenario: 退出码契约有判别力

- **WHEN** 把验证源的断言改成 1 条且全过
- **THEN** 以「结论行条数 == 登记条数」为判据的护栏 MUST **转红**
- **AND** 仅判退出码的护栏 MUST NOT 被视为充分

### Requirement: 发射器调用 MUST 支持参数化调用

清单 MUST 能表达「**发射器 target** + **调用参数**」⇒ ⇒ MUST 有一个 `emitter_args` 列。

⇒ ⇒ **列语义 MUST 唯一**：
· `emitter` = target 名（**单个** token ⇒ MUST NOT 含空格）
· `emitter_args` = **空格分隔**的参数 token 序列 ⇒ ⇒ **无参 MUST 写 `-` 哨兵**
· 单个参数 token MUST NOT 含空白（需要含空白的参数 ⇒ 另立列，不在本条范围）

⇒ ⇒ `-` 哨兵 MUST NOT 被**原样透传**给发射器；**每行字段数 MUST 等于 `#COLUMNS` 词数**
（否则 TAB 折叠会让字段左移 ⇒ 报具名错）。

⇒ ⇒ 参数**位置 MUST 无关**（调用方剥离式组装）；`#COLUMNS` 推导与「中间插列」两条
MUST 遵循本 capability 的既有 requirement ⇒ ⇒ 本条**不重述**。

#### Scenario: 参数缺失 ⇒ 以 fail-closed 结束

- **WHEN** 某**需要**参数的行被改成 `-`
- **THEN** 该次发射 MUST 以**非零**结束（默认模式收到未知参数 ⇒ 退出码 2，或产不出产物）
- **AND** 护栏 MUST 报「发射器调用」这一类错误
- **AND** 该负向对照 MUST 钉在**需要参数的行**上（无参行掉参数仍绿）

#### Scenario: 无参哨兵不得成为实参

- **WHEN** 某行参数列为 `-`
- **THEN** 该次调用的**命令行** MUST NOT 含由该列透传而来的 `-`
- **AND** 该次调用 MUST 与「不带该列参数」等价

#### Scenario: 字段数守恒可归因

- **WHEN** 某行字段数 ≠ `#COLUMNS` 词数（例如中段留空）
- **THEN** 护栏 MUST **具名**报错（指出行号与期望字段数）
- **AND** MUST NOT 以无名 `unbound variable` 中止整轮

#### Scenario: 只读 target 名的消费方零改动

- **WHEN** 清单新增一列（参数列）
- **THEN** 只读 target 名的消费方 MUST 仍绿且**无需**改代码

### Requirement: C++ 侧入口的断言条数 MUST 登记且被守护

清单 MUST 为每个「自发射」产品单元登记其 **C++ 侧（解释器路径）** 的期望断言条数
⇒ ⇒ 该值 MUST 由**列**承载（`interp_checks`）⇒ ⇒ MUST NOT 由自由文本承载。

⇒ ⇒ **自发射** = `emitter` 列 == `target` 列 ⇒ ⇒ 非自发行的行 MUST 写 `-` 哨兵。

⇒ ⇒ **判据 = 等式（不是下界）**：C++ 侧输出的 `RESULT: checks=<N> fails=0` MUST 满足
`N == interp_checks` ⇒ ⇒ MUST NOT 用 `.sv` 侧的 `min_checks` 代用（两者语义不同
⇒ ⇒ 手写激励的产品 MUST 各自登记）。

#### Scenario: 自发射行缺登记

- **WHEN** 某行满足 `emitter == target` 而 `interp_checks` 为 `-` 或 0
- **THEN** 护栏 MUST 判定为不满足本条
- **AND** MUST 指出该行的 target 名

#### Scenario: C++ 侧条数被删

- **WHEN** 该行 C++ 侧入口输出的 `checks` 与 `interp_checks` 不等
- **THEN** 护栏 MUST 判定为不满足本条
- **AND** MUST 报告期望值与实际值

#### Scenario: 非自发行的行

- **WHEN** 某行满足 `emitter != target`
- **THEN** 该行 MUST 把 `interp_checks` 写 `-` 哨兵
- **AND** 护栏 MUST NOT 对该行执行 C++ 侧条数判据

#### Scenario: 自由文本承载判据

- **WHEN** 判据的适用范围由 note 列的自由文本标记决定
- **THEN** 该实现 MUST 判定为不满足本条
- **AND** 该约定 MUST 已迁入 `interp_checks` 列
