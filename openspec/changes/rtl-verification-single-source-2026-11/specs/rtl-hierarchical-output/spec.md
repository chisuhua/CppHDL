# Capability: rtl-hierarchical-output

## ADDED Requirements

### Requirement: 产品面交付单元的顶层端口 MUST 具名，且 MUST NOT 留在命名棘轮豁免表

被登记为**产品面交付单元**的顶层组件，其 `__io` 端口 MUST 使用**显式命名构造**
（`ch_in<ch_bool> start{"start"};` ⇒ 名字串与标识符**逐字相同**）⇒ ⇒ 这些端口
MUST NOT 出现在命名棘轮豁免表（`.rddf/checks/port-naming-exempt.tsv`）中。

⇒ ⇒ **首批范围（本 change 登记 · MUST 逐条内联，禁写成「产品面顶层全量」）**：

| 组件 | 文件 | 端口数 |
|---|---|---|
| `AxiLiteTop` | `examples/axi4/axi4_lite_example.cpp` | **20** |
| `AxiSocTop` | `examples/axi4/axi_soc_example.cpp` | **12** |

⇒ ⇒ ⚠️ **同一文件里的内层组件端口不在本条范围内**（`AxiLiteTestMaster` 的 **23** 个
`ch_in`/`ch_out` 是**成层后的子 module 端口**）⇒ ⇒ 它的具名化属**另一类产品可见变更**。

⇒ ⇒ ⚠️ **不重述三类划分**：A/B/C 三类的 `cpp_member` 义务归本 capability 的
「顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定」⇒ ⇒ 本条只增加
「**具体哪 32 个端口**必须离开 C 类」这一可机械判定的增量。

#### Scenario: 32 个产品顶层端口全部离开 C 类

- **WHEN** 检查 `AxiLiteTop`（20）与 `AxiSocTop`（12）的 `__io` 声明
- **THEN** 每个端口的声明 MUST 是显式命名构造 ⇒ ⇒ 其 manifest `cpp_member` MUST 不等于 `unnamed_input` / `unnamed_output`
- **AND** 这 32 个端口 MUST NOT 出现在 `.rddf/checks/port-naming-exempt.tsv`
- **AND** 豁免表的数据行 MUST 由 1447 降至 **1415**（= 1447 − 32）
- **AND** `AxiLiteTestMaster` 的 23 个端口 MUST 保持原形态（不在本条范围内）

#### Scenario: 范围写宽即违反

- **WHEN** 把本条实现成「所有产品面顶层端口（含其余 example）MUST 具名」
- **THEN** 该实现 MUST 被判定为**不满足**本条 ⇒ ⇒ 豁免表现存 1415 条中大量是其它 example 的顶层端口 ⇒ ⇒ 全量版一落盘即全仓恒红
