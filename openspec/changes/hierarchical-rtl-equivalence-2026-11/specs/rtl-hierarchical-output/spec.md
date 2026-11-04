# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 默认（全 inline）产物 MUST 与扁平产物逐字节一致

当**没有任何**组件被声明为成层时，层次化产物 MUST 与扁平产物**逐字节一致**
（同名、同序、同端口表）⇒ ⇒ **允许差异 = 空集**（可执行判据：`cmp` 退出码 0）。

#### Scenario: 全 inline 时零差异

- **WHEN** 一个设计未声明任何成层组件
- **THEN** 层次化产物与扁平产物 MUST **逐字节相同**（`cmp` 退出码 0 ⇒ 无任何允许差异）
- **AND** 该产物 MUST NOT 包含任何实例化语句

### Requirement: 层次化产物 MUST 与扁平产物等价（可机械判定）

等价性 MUST 由以下四条同时判定，且判据 MUST **可机械执行**：

① **映射表** MUST 被产出，且 MUST 是**四元组**：「`node_id` → 设计级信号名 →
扁平位置 → 层次化位置集合（含 `hier_top_port_name`）」；
② **输入端口集合 MUST 相等**（名字 + 宽度逐项配对）；
③ **输出端口集合 MUST 等于精确期望集** = 扁平输出集 − {**`ch_out` 的最近成层祖先 ≠ 根** 的全部节点}
   （⇒ 含**成层子树里被 inline 的后代**的 `ch_out` ⇒ 漏减它们 ⇒ 正确实现必红）；
④ 对**显式声明**的观测集合 `O`（**非空**且**至少一个随激励变化**）⇒ `flat[O] ≡ hier[O]`
**逐拍全等**。

#### Scenario: 漏发端口 MUST 被抓到

- **WHEN** 层次化产物少发了某个输入端口
- **THEN** 判据 ② MUST **转红**
- **AND** MUST NOT 用「`hier.top_ports ⊆ flat.top_ports`」代替 ②（⇒ 丢端口正是子集方向）

#### Scenario: 判据 MUST 有判别力

- **WHEN** 故意破坏层次化产物（删一条连线 / 改一个隐式端口方向）
- **THEN** 等价性测试 MUST **转红** ⇒ ⇒ 还原后 MUST 复绿
