# Capability: hierarchical-port-naming-guard

## ADDED Requirements

### Requirement: 端口名的 C++ 成员可解析性 MUST 由 1:1 join 静态核验

护栏 MUST 以**静态**方式核验「产品端口的 `cpp_member` 可解析」⇒ ⇒ 判据 =
manifest 的 `port_table[].name` 与 `ports[].cpp_member` 之间建立 **1:1 唯一映射**
（join 键 = `name`）。

⇒ ⇒ 判定域 MUST 限于 `role ∉ {clock, reset}` 且 `is_top_io == true` 的端口
（A 类基础设施端口 MUST NOT 被要求提供 `cpp_member` ⇒ ⇒ 与
`rtl-hierarchical-output` 的三类划分一致 ⇒ ⇒ 本条 **MUST NOT** 复述三类定义）。

⇒ ⇒ 下列任一情形 MUST 以**非零退出**报错，且 MUST **点名**违规端口：
① `port_table[]` 的元素在 `ports[]` 中**无匹配** ② 匹配**多于一个**（`name` 不唯一）
③ 匹配到的 `cpp_member` 是占位值（`unnamed_input` / `unnamed_output`）。

⇒ ⇒ ⚠️ 护栏 MUST NOT 用字符串拼接推导端口名（形如 `"top_" + member`）⇒ ⇒ 根 module
名是可配置的 ⇒ ⇒ 端口名 MUST 从 `port_table[].name` **读取**。

#### Scenario: 缺映射 / 多义 / 占位值各自转红且文本可区分

- **WHEN** manifest 里某产品端口的 `name` 在 `ports[]` 中无匹配
- **THEN** 护栏 MUST 以非零退出并报告「无匹配」并点名该端口
- **AND** 当匹配多于一个时 MUST 报告「多义」（与「无匹配」文本可区分）
- **AND** 当 `cpp_member` 为占位值时 MUST 报告「未具名」（与上两者文本可区分）

#### Scenario: 用拼接实现即违反

- **WHEN** 护栏以 `"top_" + cpp_member` 生成期望端口名并与产物比对
- **THEN** 该实现 MUST 被判定为**不满足**本条 ⇒ ⇒ 当根 module 名配置为非 `top` 时它会静默错判
