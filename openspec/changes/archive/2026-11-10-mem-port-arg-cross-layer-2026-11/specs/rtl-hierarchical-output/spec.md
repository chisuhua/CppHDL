## MODIFIED Requirements

### Requirement: 跨层非端口边 MUST 按方向合成隐式端口（默认告警）

- **MODIFIED**：⚠️ `PortPolicy::Reject` 与 `UndrivenPolicy::Reject` **同名** ⇒ ⇒ 本条
  引用时 MUST 写**全限定名**（`UndrivenPolicy::Reject` 管的是**悬空 `ch_in`**）。

跨模块但**不落在端口上**的边（bundle 成员跨层 / 父层改子层状态 / 子端口由父层表达式驱动）
MUST 按**边方向**合成隐式端口（父→子 = 子侧 input；子→父 = 子侧 output）⇒ ⇒
默认（`PortPolicy::SynthesizeWarn`）MUST 有 `CHWARN` 与生成文件内注释；
**`PortPolicy::SynthesizeFatal`** MUST **先合成完再失败**（诊断含**全部**被合成的边与端口名）；
**`PortPolicy::Reject`** MUST **首错即返**（同样 `ok = false` + `error` 点名）。

⚠️ **跨模块 `ch_mem` 的闸门 MUST 按三桶判定**（本条新增 ⇒ 取代原「跨模块存储器 MUST
响亮拒绝」的**单一**表述）⇒ ⇒ 判定 MUST 用**节点级归属比对**
（**持有 module** = `ch_mem` 本体的 inline 折叠后归属 · **使用 module** = mem 端口节点自身的
折叠后归属）⇒ ⇒

- **(i)** 持有 module ≠ 使用 module（= **mem 本体跨 module**）⇒ ⇒ MUST **响亮拒绝**
  （⇒ ⇒ 本条的「跨模块存储器 MUST 响亮拒绝」scenario 即**这一桶**）；
- **(ii)** 持有 module == 使用 module（= mem 本体与使用者同 module）**且** 跨 module src
  **仅**落在 `addr` / `en` / `wdata` 三个实参槽 **且** 该 src 的 owner 与使用 module **相邻**
  ⇒ ⇒ **MUST 支持**（⇒ ⇒ 见 ADDED 的「mem 端口实参跨层」条）；
- **(iii)** 持有 module == 使用 module 但 src owner **非相邻**（孙 / 兄弟）⇒ ⇒ MUST 响亮拒绝。

⇒ ⇒ 三桶 MUST **互斥且穷举** ⇒ ⇒ (ii) MUST 带**相邻性**约束（否则「mem 本体同 module ·
实参来自孙」的形态会同时满足 (ii) 与 (iii)）。

⚠️ **诊断 MUST 按桶给出正确归因** ⇒ ⇒ (i) 桶 MUST 说「mem 本体与使用者 MUST 同 module」
⇒ ⇒ **(iii) 桶 MUST NOT 说该句**（mem 本体**已在**同层 ⇒ ⇒ 说它会把用户引向**错误**改法）
⇒ ⇒ 两桶 MUST **分开**判（MUST NOT 一刀切删除 (i) 桶的归因）。

#### Scenario: 子→父方向 MUST 合成子侧 output

- **WHEN** 父层的输出由**子组件的成员寄存器**驱动
- **THEN** 层次化 MUST 在**子侧**合成一个 **output** 隐式端口
- **AND** 父层 MUST 通过该实例的输出取数

#### Scenario: 隐式端口的实现 MUST NOT 改写 IR

- **WHEN** 一个跨模块边需要「把驱动搬进子模块」
- **THEN** 实现 MUST 只在**发射期**做虚拟重定向
- **AND** MUST NOT 调用 `set_src` / `set_next` 改写 `lnodeimpl`
      （⇒ ⇒ 否则 `toVerilog()` 产物、解释器行为、两次生成的确定性**全部受影响**）

#### Scenario: 跨模块存储器 MUST 响亮拒绝

- **WHEN** 父层直接读取**子组件**持有的 `ch_mem`
- **THEN** 层次化 MUST **返回 `ok = false`**，`error` MUST 给出「存储器与使用者同层」的两条替代路径
- **AND** `files` MUST 为空（MUST NOT 留下跨模块存储器引用的产物）
- **AND** MUST NOT 静默产出跨模块的存储器引用

#### Scenario: 驱动源在更上层时 MUST 逐级合成 pass-through 输入端口

- **WHEN** 一个成层 module 的输入端口，其驱动节点的归属落在**更上层**的 module
      （含「不属于任何组件」⇒ 归顶层）
- **THEN** MUST **沿祖先链逐级**合成同名的 **input** pass-through 端口（每级一个）
- **AND** MUST NOT 拒绝（⇒ ⇒ 否则父 module 引用不到顶层网 ⇒ ⇒ 该输入**无源**）
- **AND** MUST NOT 把该输入直接接**表达式**（⇒ ⇒ 跨 module 的 `assign` 是非法 Verilog）

> ⚠️ **本条是「按方向分列」矩阵的第三轴**（ADR-060 §2.1 v1.4）⇒ ⇒
> **缺了它 ⇒ ⇒ 驱动源在顶层的输入会在中间层丢失 ⇒ ⇒ 行为分叉且无告警**。

## ADDED Requirements

### Requirement: mem 端口实参跨层 MUST 支持且走父侧落点网

`ch_mem` 读 / 写端口的 `addr` / `en` / `wdata` **实参槽**取自成层子组件的 `ch_out` 时
（= 上条的 **(ii)** 桶）⇒ ⇒ 层次化 MUST 支持该形态 ⇒ ⇒

- **落点网**：MUST **复用**「父侧消费子 `ch_out`」的既有落点网机制（父 module 声明一根
  `logic` 网 + 子 output 以 `.port(net)` 连接 + 父侧读该网）⇒ ⇒ MUST NOT 为 mem 端口合成
  **专用端口**（mem 端口**不是**一根信号）⇒ ⇒ MUST NOT 引入 mem 专用的第二套命名；
- **命名**：落点网的网名 MUST = **驱动端点的 ctx 全局名** ⇒ ⇒ mem 端口读到的名字与网名
  **恒等** ⇒ ⇒ MUST NOT 为 mem 实参新增「重定向表查表」（恒等映射 ⇒ 死代码）；
- **宽度**：MUST NOT 让父 module 出现 **implicit wire**（1-bit ⇒ 宽度截断 ⇒ 值错）⇒ ⇒
  产物内 mem 实参引用的名字 MUST 在**父 module 作用域**有**显式 `logic` 声明**；
- **`en` 槽**：`enable` 缺省（无实参）⇒ MUST 取 `1'b1`（恒使能）；`enable` **有实参**但
  **取不到名字**⇒ ⇒ MUST **fail-closed**（诊断点名）⇒ ⇒ MUST NOT 回退 `1'b1`
  （⇒ ⇒ 读侧恒使能 / 写侧恒写 ⇒ ⇒ **值错且 lint 绿**）。

#### Scenario: (ii) 形态 MUST 发射成功且父侧有显式声明

- **WHEN** mem 本体与使用者同 module，且 `addr` 取自成层子组件的 `ch_out`
- **THEN** 层次化 MUST 返回 `ok = true`
- **AND** 父 module 作用域 MUST 有该实参名的**显式 `logic` 声明**（⇒ ⇒ 非 implicit wire）
- **AND** mem 端口 MUST **读**该网

#### Scenario: 非相邻的 mem 实参 MUST 响亮拒绝（**(iii) 桶**）

- **WHEN** mem 本体与使用者同 module，但 `addr` / `en` / `wdata` 取自**孙组件**或**兄弟**的 `ch_out`
- **THEN** 层次化 MUST **返回 `ok = false`**
- **AND** `error` MUST 点名该跨层实参与两端 module
- **AND** `error` MUST NOT 含「存储器与使用者同层」这类**错误**归因（mem 本体**已在**同层）

#### Scenario: (ii) 形态的读值 MUST 是确定值

- **WHEN** mem 的初值表已知，且实参地址逐拍变化
- **THEN** 层次化产物在**逐拍**仿真下 MUST 读出与**手推**一致的值
- **AND** MUST NOT 以「非 0」作为判据（⇒ ⇒ 错值只要非 0 即过）

#### Scenario: 写侧的 `wdata` / `en` 同样 MUST 跨层生效

- **WHEN** mem 写端口的 `addr` / `wdata` / `en` **三个**实参槽全部取自成层子组件的 `ch_out`
- **THEN** 层次化 MUST 发射出**读**这三根父侧网的写端口
- **AND** 逐拍仿真 MUST 观察到写入的值（⇒ ⇒ `en` 未接上 ⇒ 写恒不发生 ⇒ ⇒ 必被抓住）

#### Scenario: `en` 实参有值但取不到名字 MUST fail-closed

- **WHEN** mem 端口的 `enable` 槽有实参，但该实参在当前 module 作用域取不到名字
- **THEN** 发射 MUST 诊断点名该 mem 端口与该事实
- **AND** MUST NOT 回退 `1'b1`（⇒ ⇒ 否则恒使能 ⇒ ⇒ 值错且 lint 绿）
