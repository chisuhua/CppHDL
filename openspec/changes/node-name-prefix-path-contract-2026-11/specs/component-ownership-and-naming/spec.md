# Capability: component-ownership-and-naming

## ADDED Requirements

### Requirement: 组件路径与节点名的前缀拼接 MUST 同口径

组件实例的**子**路径 MUST 由「父路径 + `.` + 局部名」构成
⇒ ⇒ MUST NOT 因父路径的**字面值**而单方面剥离路径段
⇒ ⇒ 节点名的前缀拼接 MUST 与组件路径拼接**同口径**
⇒ ⇒ 「无名」是**普通名字**，不是「无路径段」标记。

#### Scenario: 有父的组件实例路径由父路径与局部名构成

- **WHEN** 一个组件实例的**有父**、且其父组件的 `name()` 为 `<P>`（非空）、其局部名为 `<L>`
- **THEN** 该实例的 `name()` MUST 等于 `<P> + "." + <L>`
- **AND** MUST NOT 因 `<P>` 的字面值而剥离该路径段

#### Scenario: 无父的组件实例路径等于其局部名

- **WHEN** 一个组件实例**没有父组件**（`parent == nullptr`）
- **THEN** 该实例的 `name()` MUST 等于其 `local_name()`
- **AND** 该实例的**子**节点 MUST 以 `<local_name> + "."` 为前缀（与上一条同口径）
