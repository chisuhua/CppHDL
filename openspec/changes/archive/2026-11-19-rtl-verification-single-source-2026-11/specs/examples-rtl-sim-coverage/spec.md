# Capability: examples-rtl-sim-coverage

## ADDED Requirements

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
