# Capability: rtl-hierarchical-output

## Purpose

定义 CppHDL **层次化 RTL 输出**的契约：哪些组件成为独立 Verilog module（**层级粒度由用户
显式声明**，默认 inline）、模块边界如何划分（= 所有权边界）、命名权威在哪（= ctx 级全局表）、
跨层连线如何处理（隐式端口 / 悬空输入），以及与**扁平输出**的**逐拍等价性**如何判定。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 成层组件 MUST 形成独立 module，且 module 树等价于成层组件子树

层次化产物的 module 树 MUST 与「**被声明为成层的**」组件子树**一一对应**
（节点数、父子关系、实例名）⇒ ⇒ 被 inline 的组件 MUST NOT 出现在 module 树里。

#### Scenario: 三层设计的 module 数量

- **WHEN** 一个三层设计里三个组件**全部**被声明为成层
- **THEN** 产物 MUST 包含 **3 个 module** 与 **2 条实例化语句**
- **AND** MUST NOT 存在任何跨 module 边界的 `assign`

#### Scenario: 计数对但树形错 MUST 不通过

- **WHEN** module 数量与预期相同、但父子关系与组件子树不一致
- **THEN** 该产物 MUST 判定为**不通过**（结构同构判据）

### Requirement: 跨层非端口边 MUST 按方向合成隐式端口（默认告警）

跨模块但**不落在端口上**的边（bundle 成员跨层 / 父层改子层状态 / 子端口由父层表达式驱动）
MUST 按**边方向**合成隐式端口（父→子 = 子侧 input；子→父 = 子侧 output）⇒ ⇒ 默认 MUST
有 `CHWARN` 与生成文件内注释 ⇒ ⇒ `strict` 策略 MUST 改为抛异常。

#### Scenario: 子→父方向 MUST 合成子侧 output

- **WHEN** 父层的输出由**子组件的成员寄存器**驱动
- **THEN** 层次化 MUST 在**子侧**合成一个 **output** 隐式端口
- **AND** 父层 MUST 通过该实例的输出取数

#### Scenario: 隐式端口的实现 MUST NOT 改写 IR

- **WHEN** 一个跨模块边需要「把驱动搬进子模块」
- **THEN** 实现 MUST 只在**发射期**做虚拟重定向
- **AND** MUST NOT 调用 `set_src` / `set_next` 改写 `lnodeimpl`
      （⇒ 否则 `toVerilog()` 产物、解释器行为、两次生成的确定性**全部受影响**）

#### Scenario: 跨模块存储器 MUST 响亮拒绝

- **WHEN** 父层直接读取**子组件**持有的 `ch_mem`
- **THEN** 层次化 MUST **拒绝**（抛异常）并给出「存储器与使用者同层」的替代路径
- **AND** MUST NOT 静默产出跨模块的存储器引用

#### Scenario: 驱动源在更上层时 MUST 逐级合成 pass-through 输入端口

- **WHEN** 一个成层 module 的输入端口，其驱动节点的归属落在**更上层**的 module
      （含「不属于任何组件」⇒ 归顶层）
- **THEN** MUST **沿祖先链逐级**合成同名的 **input** pass-through 端口（每级一个）
- **AND** MUST NOT 拒绝（⇒ ⇒ 否则父 module 引用不到顶层网 ⇒ ⇒ 该输入**无源**）
- **AND** MUST NOT 把该输入直接接**表达式**（⇒ ⇒ 跨 module 的 `assign` 是非法 Verilog）

> ⚠️ **本条是「按方向分列」矩阵的第三轴**（ADR-060 §2.1 v1.4）⇒ ⇒
> **缺了它 ⇒ ⇒ 驱动源在顶层的输入会在中间层丢失 ⇒ ⇒ 行为分叉且无告警**。

### Requirement: 悬空子输入 MUST 向上冒泡为顶层输入

无驱动源的子模块输入端口 MUST 逐级向上冒泡，最终成为**顶层 input**
（与扁平产物行为一致）⇒ ⇒ MUST 有 `CHWARN` 点名路径 ⇒ ⇒ `strict` 策略 MUST 改为抛异常。

#### Scenario: 悬空输入在两份产物中都可驱动

- **WHEN** 一个子模块的输入端口未被驱动
- **THEN** 扁平与层次化两份产物 MUST 都把它暴露为**顶层可驱动输入**
- **AND** 层次化侧该端口名 MUST **与扁平产物逐字相同**（= 该输入节点的 ctx 级全局名）
      ⇒ ⇒ MUST NOT 加任何框架前缀
- **AND** 该端口旁 MUST 有注释标明它来自哪个组件的未驱动输入

### Requirement: 时序模块 MUST 有 clock/reset 端口

**根 module** MUST **无条件**发射 `default_clock` / `default_reset`
（⇒ 与扁平产物一致：`context::init()` 恒定创建这对节点，扁平 `print_header` 无条件发射）。
**非根** module MUST **把 clock 与 reset 分开判**（⇒ 一元 `has_seq` 不够）。
`default_clock` ⇒ 含 `ch_reg` ∨ mem 写端口 ∨ `sread`；
`default_reset` ⇒ 含 `ch_reg` ∨ `sread`（⇒ mem 写端口的 `always_ff` **没有 reset 分支**）。

#### Scenario: 纯组合设计的扁平与全 inline 产物端口数一致

- **WHEN** 一个设计**不含**任何 `ch_reg` / `ch_mem`
- **THEN** 扁平与全 inline 层次化产物 MUST **都**含 `default_clock` / `default_reset` 两个端口
- **AND** 两份产物 MUST 逐字节相同

#### Scenario: 只有 RAM 写端口的模块 MUST 有时钟

- **WHEN** 一个成层 module 的体内只含 `ch_mem` 的**写端口**（无 `ch_reg`、无 `sread`）
- **THEN** 该 module MUST 发射 clock 端口
- **AND** 它 MUST **NOT** 发射 reset 端口（⇒ mem 写端口的 `always_ff` 没有 reset 分支）
- **AND** 产物 MUST 通过 `verilator --lint-only -Wall`（MUST NOT 引用未声明信号）

### Requirement: 成层且非根的 module 端口 MUST 覆盖自身 `ch_in` / `ch_out`，且每个端口可追溯

**成层且非根**的 module，其端口集合 MUST **⊇** 它自己的 `ch_in` ∪ `ch_out`
⇒ ⇒ **是否已被父层驱动不影响它是否成为端口**（emitter MUST 忽略 `inputimpl::driver()`）。
且每个端口 MUST 可追溯到五类来源之一：自身端口 · inline 后代的 `ch_out`（**仅根允许**）·
隐式端口 · 冒泡端口 · **时序基础设施**（clock/reset）。

⇒ ⇒ **RTL 顶层 module MUST 走扁平规则**，MUST NOT 套用本条。
⇒ ⇒ **自驱动**：成层组件自己驱动自己的 `ch_in` ⇒ MUST 响亮拒绝。

#### Scenario: 已被父层驱动的 `ch_in` MUST 仍是 module 端口

- **WHEN** 一个成层组件的 `ch_in` 由父层表达式驱动
- **THEN** 该 `ch_in` MUST 出现在该组件的 module 端口表里
- **AND** 子 module 内 MUST **NOT** 出现 `assign <该输入> = ...`
- **AND** 父层的驱动 MUST 表现为**实例端口连接**

#### Scenario: 父层消费子输出 MUST 有连接网

- **WHEN** 父 module 消费一个成层子组件的 `ch_out`
- **THEN** 父 module MUST 为该端口声明一根 `logic` 网
- **AND** 由实例化语句的 `.port(net)` 驱动（MUST NOT 用 `assign` 接表达式）
- **AND** ⚠️ 该「必须造网」的要求 MUST NOT 施加到**接 input 的** `.port(...)`（可为表达式）

#### Scenario: 自驱动 ch_in MUST 被拒绝

- **WHEN** 一个成层组件在自己的体内驱动自己的 `ch_in`
- **THEN** emitter MUST **抛异常**
- **AND** MUST NOT 在子 module 内对该 `input` 发 `assign`

#### Scenario: RTL 顶层 module MUST 走扁平规则

- **WHEN** 一个设计里 RTL 顶层 module 的下方有被 inline 的子组件
- **THEN** 顶层端口表 MUST 是 clock → reset → 无 driver 的 `input` → **全部** `ch_out`
      （⇒ 含 inline 后代的 `ch_out`）
- **AND** 该端口表 MUST 与扁平产物**逐字逐序相同**

#### Scenario: 非根 module MUST NOT 抬 inline 后代的 `ch_out`

- **WHEN** 一个**非顶层**成层 module 内含被 inline 的子组件
- **THEN** 该子组件的 `ch_out` MUST 是该 module 体内的普通信号
- **AND** MUST NOT 出现在该 module 的端口表里

### Requirement: 单元体内 MUST NOT 引用「非自己、非祖先」的端口节点

一个组件的体内节点 MUST NOT 以某个端口节点为源，而该端口节点的 `owner`
**既不是它自己、也不是它的祖先** ⇒ ⇒ 这是「**值经 host 侧通道（键值总线）进来**」的
可机械判据（⇒ 那种通道**不产生端口、也不构成接口声明**）。

#### Scenario: 只从 host 侧键值通道读的单元 MUST 被判定为接口不完整

- **WHEN** 一个单元的输入**只**从 host 侧键值通道读取，**没有**对应的 `ch_in` 端口
- **THEN** 上述判据 MUST **转红**
- **AND** MUST NOT 改用「module 的端口数 > 0」当判据
      （⇒ 输出方向必须先有 `ch_out` 作源 ⇒ 端口数可能 > 0 ⇒ 该判据**没有判别力**）

### Requirement: 层次化单元的接口类型 MUST 限于白名单

一个**成层**单元的接口端口类型 MUST 限于 `ch_uint<N>` / `ch_bool`
⇒ ⇒ MUST NOT 出现 `ch_mem` · `ch_reg` · bundle / `ch_stream` · clock / reset · `inout`
（⇒ ⇒ 理由分别是：数组无端口形态 · 状态不是接口 · 成员不是端口 · 由框架供给以免重复端口 · 单方向模型）。

⇒ ⇒ **数组字段**（`ch_out<T> x[N]`）MUST 允许，但端口名 MUST 是**确定性派生**（`x_<i>`）
⇒ ⇒ 否则「同一设计两次生成字节相同」不成立。

⇒ ⇒ ⚠️ **义务方**：本条约束**上游**（emitter / 接口声明方）；
「接口字段名与顺序 MUST 稳定」（ADR-062 **N-3**）**不在**本条范围 ⇒ ⇒ 但 N-3 MUST 另补载体。

#### Scenario: 白名单外的接口类型 MUST 被拒绝

- **WHEN** 一个成层单元的接口端口类型是 `ch_mem` / `ch_reg` / bundle / `ch_stream` / `inout`
- **THEN** emitter MUST **响亮拒绝**并点名该端口
- **AND** MUST NOT 静默产出（⇒ ⇒ 「数组无端口形态」⇒ ⇒ 发射器**无法**表达它）

### Requirement: 层次化 MUST 有独立的公开入口，既有路径 MUST 逐字节不变

层次化输出 MUST 走**独立的新入口**（新函数 + 新选项结构 + 新错误类型）⇒ ⇒
既有 `toVerilog()` 的**签名**与**行为** MUST **逐字节不变**（⇒ ⇒ 既有调用点与 Verilator backend
**零风险**）⇒ ⇒ Verilator backend / Simulator / JIT 的**既有入口**一律不动。

⇒ ⇒ **错误模型**（MUST）：
① 所有校验 MUST 在**第一次 I/O 之前**完成（⇒ ⇒ 否则留下**半套产物**）；
② 写出 MUST 走「**临时目录 → 成功后 rename**」（⇒ ⇒ 失败**不留下**任何新文件）；
③ 校验 MUST NOT 落在任何 `catch(...)` 作用域内（⇒ ⇒ 否则响亮失败会被**静默吞掉**）；
④ 静态析构期 MUST 另有**返回码**兜底。

⇒ ⇒ 选项 MUST 用**线性枚举**表达（MUST NOT 用布尔开关组合 ⇒ ⇒ 组合语义重叠 ⇒ ⇒ 未定义态留给实现者）。

#### Scenario: 既有扁平入口 MUST 不受影响

- **WHEN** 同一个设计用既有 `toVerilog()` 生成
- **THEN** 产物 MUST 与引入层次化能力**之前逐字节相同**
- **AND** 该入口的签名 MUST NOT 变化（⇒ ⇒ 既有调用点零改动）

#### Scenario: 校验失败 MUST 不留下半套产物

- **WHEN** 层次化生成在**写出之前**就因校验失败
- **THEN** MUST **不产生任何**新文件
- **AND** 错误 MUST **可被调用方观测**（⇒ ⇒ 返回错误类型，MUST NOT 只打日志）

#### Scenario: 写出过程中失败 MUST 不留下半套产物

- **WHEN** 层次化生成已开始写出，但在**完成之前**失败
- **THEN** 输出路径 MUST **不留下**任何新文件（⇒ ⇒ 半套产物会让 `cmp` / lint 读到它）
- **AND** 临时目录 MUST 被清理
- **AND** 既有的同名文件 MUST NOT 被破坏（⇒ ⇒ 写出 MUST 走「**临时目录 → 成功后 rename**」）

#### Scenario: 静态析构期的失败 MUST 以返回码呈现

- **WHEN** 层次化入口在**静态析构期**被调用且校验失败
- **THEN** 失败 MUST 以**返回码 / 错误类型**呈现
- **AND** MUST NOT 依赖抛异常（⇒ ⇒ 该期抛异常会 `std::terminate`；既有宏在该期**不抛**）
- **AND** 退出路径 MUST **不泄漏**资源
