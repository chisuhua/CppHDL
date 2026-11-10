# Capability: rtl-hierarchical-output

## ADDED Requirements

### Requirement: 顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定

顶层 module 端口表中的端口，MUST 按其**所属类别**分别满足该类别的 `cpp_member` 义务
⇒ ⇒ **B 类 MUST 提供 · A 类与 C 类 MUST NOT 被要求提供**
⇒ ⇒ 载体 = manifest 的 `cpp_member` 字段（schema 归 `rtl-emit-portability`）
⇒ ⇒ 下游工具 MUST NOT 依赖「Verilog 端口名 == C++ 成员名」这一**当前不成立**的假设。

#### Scenario: 端口的三类互斥划分与各自的 `cpp_member` 义务

- **WHEN** 一个设计发射出顶层 module 的端口表
- **THEN** 端口表中的每个端口 MUST 落入下列**恰好一类**
- **AND** 分类 MUST 按下列**判定顺序**执行（**先命中者为准** ⇒ ⇒ 顺序本身保证互斥）
- **AND** 分类 MUST 由 manifest 字段与命名棘轮登记表判定（MUST NOT 依赖人工判断）

| 序 | 类 | 判定（**按序判定，先命中者为准**） | `cpp_member` 义务 |
|---|---|---|---|
| 1 | **A · 基础设施** | `role` 为 `clock` 或 `reset` | **MUST NOT** 被要求提供 |
| 2 | **B · 显式命名** | **非** A 类，且该端口在**命名棘轮登记表**中有条目（⇒ ⇒ 判据源 = `port-naming-ratchet` N2 的登记表 + 护栏 P1「标识符 == 显式名字符串」） | **MUST** 提供，且其值 MUST 等于登记表中的显式名 |
| 3 | **C · 未显式命名** | **非** A 类，且**非** B 类 | **MUST NOT** 被要求提供（⇒ ⇒ 走兼容去重路径） |

- **AND** A 类 MUST NOT 被登记在命名棘轮的豁免表（⇒ ⇒ 它**不是** C 类 ⇒ ⇒ 无登记义务）
- **AND** C 类中**属于 product 侧**（`include/**` + `examples/**`）的端口 MUST 被登记在命名棘轮的豁免表
  （⇒ ⇒ `tests/**` 侧的端口不适用本条 ⇒ ⇒ 与棘轮扫描面一致）
- **AND** 下游工具 MUST NOT 把 A 类或 C 类「缺 `cpp_member`」当作**违规**
- **AND** 本 Requirement MUST NOT 适用于 `owner == nullptr` 且 `role` 非 `clock`/`reset` 的端口
  （⇒ ⇒ 此类由层次化分析层的 `CHWARN` 单独处置）
- **AND** 不存在既非 A/B/C 三类、也未被本 Requirement 显式排除的端口

#### Scenario: 未显式命名者（C 类）走兼容去重路径

- **WHEN** 一个端口落入 C 类
- **THEN** 它 MUST 走**兼容去重路径**（旧的去重规则保留为兼容模式）
- **AND** 它 MUST NOT 被要求提供 `cpp_member`

#### Scenario: 字面相同的成立条件与不可能条件

- **WHEN** 一个端口同时满足 ① 它是**根组件自有**的端口（manifest 的 `is_top_io == true`）
  ② 该组件实例名在其父下**唯一** ③ 该端口名与根组件自有内部节点名**不撞**
  ④ 其 C++ 成员名本身是**合法 Verilog 标识符**（`[A-Za-z_][A-Za-z0-9_]*`）
- **THEN** 该端口的 Verilog 端口名 **SHOULD** 与其 C++ 成员名逐字相同
- **AND** 同名兄弟实例的端口、子组件的端口、Verilog 关键字与数字开头的成员名
  **MUST NOT** 被承诺逐字相同
