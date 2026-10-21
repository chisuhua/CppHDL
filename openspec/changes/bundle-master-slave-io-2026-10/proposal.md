# Change: bundle-master-slave-io-2026-10 — 补齐 bundle 作为组件 IO 的 master/slave 连接语义

## Why

`samples/fifo_bundle.cpp` **一直失败**（`empty=0` / exit=1，pristine HEAD 即如此）。
审计把它拆成了**一条 5 环缺陷链** —— 每一环都独立地破坏这条路径，**只修任何一环都不足以让它跑通**。

更关键的是：**这条路径全仓零回归防护**。
`fifo_bundle` / `config_bundle` / `interrupt_bundle` 是仓库里仅有的三个
「普通信号字段」bundle（字段是 `ch_bool` / `ch_uint<N>` 成员，不是 `ch_in`/`ch_out` 端口），
而它们的**全部**使用点只有「打印宽度 / 打印角色 / 命名测试」——
**从未做过一次真实的 master/slave 连接**（`samples/fifo_bundle.cpp` 是唯一的尝试，且它一直是红的）。

⇒ 因此这不是「某个方向写反了」的局部笔误，而是
**「bundle 作为组件 IO」这条路径从未被框架支持过**。

### 实测的缺陷链（2026-10-21，逐环取证）

> ⚠️ **2026-10-21 评审修订**：初版提案把 `Simulator` 的读写（D1/D2）当作根因。
> 经逐环复核，**真正的根因是 D0**（字段节点身份被 `as_master()/as_slave()` 重建），
> D1-D6 都是它的**下游症状**。下表已按此重排。

| # | 缺陷 | 位置 | 取证 |
|---|---|---|---|
| **D0**<br>（**根因**） | `as_master()` / `as_slave()` → `set_role()` → `create_field_slices_from_node()` 把**每个字段重绑定**成「零字面量总线」的**位切片**（`bit_select` / `bits<W>`）⇒ 设计**只读不写**的 bundle 字段**恒为 0** | `include/core/bundle/bundle_base.h:282-297`（`set_role`）+ `:301-345`（`create_field_slices_from_node`） | **决定性探针**：TB 驱动 master 的 `push=1` → slave 的 `io.push` 读到 **0**（期望 1）；而设计**赋值**过的 `io.empty` 读到 **1**（赋值 = 重绑定 = 脱离切片）。⇒ **FIFO 从来没看到过 `push`/`pop`，从来没写过数据** |
| **D1** | `get_bundle_value()` 对**运行时驱动**的字段**恒返回 0** | `include/simulator.h:463` —— `static_cast<uint64_t>(field)` 是**编译期常量转换** | 独立探针（只读 slave 自己的 bundle）：改前读出 `empty=0`；改成读 data_map 槽位后**读出 `empty=1`** |
| **D2** | `set_bundle_value()` 用 `field = ch_bool(v)` → **指针重绑定**（`logic_buffer::operator=` → `node_impl_ = other.node_impl_`），**不是驱动** | `include/simulator.h:541-548` + `include/lnode/logic_buffer.tpp:21-26` | 代码路径确认：TB 写的值落到字段的**新字面量节点**，不进入设计图 |
| **D3** | bundle 字段连接用 `operator=` → 对普通信号字段同样是**重绑定** → **原驱动节点被孤儿化** | `include/core/bundle/bundle_base.h:153-165` | 探针日志：`[WARN] Value not found for signal node ID: 57`（master 的原字段节点不再可达） |
| **D4** | `connect()` **方向盲**：对每个字段无条件 `dst.field = src.field` | `include/core/bundle/bundle_base.h:360-367` | 对 master/slave 对，这会把 slave 的**输出**（`full`/`empty`）用 master 的**未驱动输入**覆盖 |
| **D5** | `fifo_bundle` 的字段方向**未镜像**（`push`：master=IN / slave=OUT） | `include/bundle/common_bundles.h:31-41` | 与同文件的 `config_bundle`（`:88-97`）、`interrupt_bundle`（`:57-68`）对比：那两个是**严格镜像**的，`fifo_bundle` 是唯一的例外 |
| **D6** | 顶层 bundle **未注册为组件 IO**（sample 的 `create_ports()` 是空的）→ master 的字段节点不在图里 | `samples/fifo_bundle.cpp:100-102` | 探针：`[WARN] Value not found for signal node ID: 57..61`（master 的 5 个字段节点全部缺失） |

### D0 为什么是根因（而不只是又一个 bug）

`create_field_slices_from_node()` 让字段的**节点身份**依赖于「它是总线的第几位」。
这带来两个后果：

1. **只读字段恒为 0**：设计从不给 `push` / `pop` 赋值（它们是输入）⇒ 字段停留在
   「零字面量总线的切片」上 ⇒ **恒为 0**。实测已确认（见上表 D0 行）。
2. **赋值会「逃出」切片**：`io.empty = expr` 走的是**重绑定** ⇒ 字段指向 `expr` 的节点
   ⇒ 设计侧「看起来正常」，但这个节点**脱离了总线**，总线的位布局从此与字段无关。

⇒ 因此 **D3/D4 的连接语义修复 MUST 在 D0 之后**：若字段仍是切片节点，
`operator<<=` 的 `set_src(0, ...)` 会打在**切片节点**上 ⇒ 对 `offset > 0` 的字段
（如 `fifo_bundle.push` 在 offset 2）变成「取对方 1-bit 节点的第 2 位」——
**比现在更隐蔽**。而 `offset == 0` 的单字段 bundle（`tests/test_bundle_connection.cpp`
的 `SimpleBundle`）会**碰巧正确** ⇒ 现有测试会**假绿**。

### 已确认的「正确做法」已经在框架里

- `ch_bool::operator<<=`（`include/lnode/bool.tpp:52-69`）与
  `ch_uint<N>::operator<<=`（`include/core/uint.h:147-166`）已经实现**数据流边**
  （`node_impl_->set_src(0, src)`，ADR-002 模式）—— 这就是框架里既有的**正确连接语义**，
  普通信号字段**只需改用它**。
- `ch_bool` / `ch_uint<N>` **已经**具备 `as_input()` / `as_output()` / `direction()` /
  `flip_direction()`（`include/core/uint.h:132-176`）—— **方向是可承载的**，
  框架不需要新增方向概念。
- `bundle_base::operator<<=`（`include/core/bundle/bundle_base.h:119-134`）**已经是方向感知的**，
  逐字段调用 `connect_field_based_on_direction_and_source()` —— 只有它内部用的
  `operator=` 是错的（D3）。

⇒ 也就是说：**修法基本是「把该用 `<<=` 的地方换成 `<<=`」+「把该读写槽位的地方读写槽位」**
+「让字段拥有自己的节点身份」，不需要新机制。

## What Changes

> **缺陷 → 修复决策 映射**（缺陷编号见上表，决策编号在下文）：

| 缺陷 | 修复决策 | 顺序 |
|---|---|---|
| **D0** 字段节点身份被 `as_master()`/`as_slave()` 重建（根因） | **D7** 字段节点身份 MUST 稳定 | **1（最先）** |
| **D1** `get_bundle_value` 走编译期常量转换 | D1 读 data_map 槽位 | 2 |
| **D2** `set_bundle_value` 走节点重绑定 | D2 写 data_map 槽位 | 2 |
| **D3** 字段连接用 `operator=`（重绑定） | D3 改用 `operator<<=`（数据流边） | 3 |
| **D4** `connect()` 方向盲 | D4 方向感知 | 3 |
| **D5** `fifo_bundle` 方向未镜像 | D5 严格镜像 | 4 |
| **D6** 顶层 bundle 未注册为组件 IO | D6 顶层改用普通端口（裁决方案 c） | 5 |
| —— | D8 可执行回归护栏 | 贯穿 |

### D1: `Simulator::get_bundle_field_value()` MUST 读 data_map 槽位

现状用 `static_cast<uint64_t>(field)`（编译期常量转换）→ 对运行时节点必然失败并返回 0。
改为与 `get_port_value()` 同源地**读该字段节点的 `data_map` 槽位**。

> ⚠️ 读/写**必须对称**（见 D2）：只修读侧会让「能读到但写不进去」。

> ⚠️ **MUST 逐字段读写**，MUST NOT 依赖「把整个 bundle 打包进一个 `uint64_t`」：
> 实测 `axi_lite_bundle<32,32>` 的**总宽是 142 位** > 64 ⇒ 打包式 API **静默截断**
> （现 `get_bundle_value` / `set_bundle_value` 就是 `uint64_t` 打包）。
> 逐字段走槽位则不受总宽限制（每个字段有自己的槽位，宽度任意）。

### D2: `Simulator::set_bundle_field_value()` MUST 写 data_map 槽位

现状用 `field = ch_bool(v)` → 触发 `logic_buffer::operator=` → **指针重绑定**，
TB 的值落到字段的新字面量节点上，不进入设计图。
改为与 `set_port_value()` 同源地**写该字段节点的 `data_map` 槽位**。

> ℹ️ **方向规则不在本 change 裁定**：bundle 语境下「master 的 output 字段」恰恰是
> TB 要驱动的那一侧（与 top-level `ch_in`/`ch_out` 的直觉相反）。
> 本 change 只保证「读写走槽位、逐字段、不截断」；
> 「谁写哪个字段」由 D6 的裁决（顶层改用普通端口）自然消解。

### D3: bundle 字段连接 MUST 用 `operator<<=`（数据流边），不得用 `operator=`

`bundle_base::connect_field_based_on_direction_and_source()` 的两个方向分支
（`left_field = right_field` / `right_field = left_field`）改为
`left_field <<= right_field` / `right_field <<= left_field`。

> ⚠️ **2026-10-21 评审修订**：初版提案在这里写的是「端口型 bundle 不受影响，
> 因为端口的 `operator=` 本就走 `set_src`」。**该论证的前提不存在**：
> 全仓**没有任何 bundle 用 `ch_in`/`ch_out` 做字段**（`include/bundle/*.h` 里 grep 为空）——
> `stream_bundle`（`include/bundle/stream_bundle.h`）、`axi_bundle` 等的字段
> **全部**是 `ch_bool` / `ch_uint<N>` 成员。
> 另：`ch_in::operator=` 是**运行时 CHERROR**（`include/core/io.h:237-241`
> 「Cannot assign value to input port!」），**不是**数据流边。
>
> ⇒ **真实的爆炸半径 = 全部「普通信号字段」bundle**（走 `bundle_base` 连接路径的那些）。
> 但**实际回归面很小**，因为这些 bundle 的使用点极少：
> `fifo_bundle`（2 个 sample）、`config_bundle` / `interrupt_bundle`
> （只用于打印宽度/角色/命名，从未做过真实连接）。
>
> ⚠️ **特例：`ch_stream` 依赖「重绑定」语义** ——
> `include/chlib/stream_operators.h:25-33` 的**自由** `operator<<=(ch_stream&, ch_stream&)`
> 在重载决议中**优先于**成员版 `bundle_base::operator<<=`，且它内部就是字段级 `=`
> （`sink.valid = source.valid; ...`）⇒ **stream 子系统今天靠重绑定语义活着**，
> D3 不会影响它，但**这意味着框架里同时存在两套连接语义**（见「遗留债务」）。

### D4: `bundle_base::connect()` MUST 方向感知

现状无条件 `dst.field = src.field`。改为逐字段复用 D3 的方向感知逻辑
（`dst.connect_field_based_on_direction_and_source(dst.field, src.field)`）。

> ⚠️ `bundle_base.h:163-164` 还有**第三个分支**（方向相同 / 无方向 → `left = right`
> 同样重绑定），初版提案只提了「两个分支」⇒ MUST 一并处理
> （同方向时应改为 `left <<= right`，否则仍会重绑定）。

> ⚠️ **既有测试的断言编码了旧的「重绑定 / 节点共享」语义** ——
> `tests/test_bundle_connection.cpp` 共 **6 处**：
> `:375`、`:448-449`、`:485-487`、`:544-546`（断言 `dst.field.impl() == src.field.impl()`，
> 即**节点共享**）、`:453-454`、`:490-491`（断言「重绑定后跟随源」）。
> 而 `:374` 的注释写的是「connect 函数逐字段连接，**不会共享节点**」——
> **注释与断言互相矛盾**。
> ⇒ 改成数据流边后节点不再共享 ⇒ 这 6 处 MUST 改成表达其**注释里声明的意图**
> （dst 节点独立、但由 src 驱动；用「值能传播」而不是「指针相等」来断言）。

### D5: `fifo_bundle` 的字段方向 MUST 严格镜像

改为与 `config_bundle` / `interrupt_bundle` 同构：

| 字段 | master | slave |
|---|---|---|
| `data`（写数据） | output | input |
| `push` | output | input |
| `pop` | output | input |
| `full` | input | output |
| `empty` | input | output |

### D6: 顶层不用 bundle 承载 TB 接口（裁决：方案 c）

sample 的 `Top` 用 `create_ports() override {}`（空）+ bundle 作为普通成员
⇒ master 的 5 个字段节点不在图里（实测 D6 的告警）。

**裁决：顶层改用普通 `ch_in`/`ch_out` 端口承载 TB 接口，bundle 只用于模块间连接。**

理由：

1. **根 `AGENTS.md` CONVENTIONS 已有明文裁决**：「**Bundle as IO**: Use individual
   `ch_bool`/`ch_uint` ports, **NOT** bundle struct」—— sample 本就违反既定约定，
   本 change 顺带把它收敛回约定。
2. **TB↔设计边界需要「输入端口」语义**（`inputimpl` / `outputimpl`：
   TB 驱动、**不被求值覆写**，见 `include/core/io.h` 与
   `src/simulator_init.cpp:94-96` 的 `input_instr_list_`）。普通字段节点一旦被
   `<<=` 驱动，就会**每拍被求值覆写** TB 写入的值 ⇒ bundle 字段天然拿不到
   「TB 驱动」的语义 ⇒ (a) 解决不了这个语义鸿沟。
3. **(a) 其实已半存在**：`tests/test_bundle_connection.cpp:222-228` 的
   `__io(SimpleBundle ...)` + placement new **能编译** ⇒ 恰恰证明 (a) 给不了
   TB 驱动语义。

> ℹ️ 若未来确需「bundle 作为顶层 IO」，那是 (b)（**逐字段生成真端口**），
> 架构上最正确但超出「修既有缺陷」的范围 ⇒ 独立立项。
>
> ⚠️ **D0（字段节点身份）与 D6 正交**：即便选 (c)，FIFO 模块自己的 bundle 字段
> 若仍是「零字面量总线的切片」，顶层端口驱动它依然无效 ⇒ **D0 MUST 先修**。

### D7: 字段节点身份 MUST 稳定（`as_master()` / `as_slave()` MUST NOT 重建字段节点）

`set_role()` MUST NOT 通过 `create_field_slices_from_node()` 把字段**重绑定**成
总线的位切片。字段 MUST 是**一等节点**（各自拥有节点身份）。

- 若确实需要一个「聚合总线」视图（例如打包/解包），它 MUST 由字段
  **concat 派生**（方向：字段 → 总线），而不是由总线**切片**得到字段。
- `as_master()` / `as_slave()` 的职责 MUST 限定为**设置方向**（`dir_`），
  不得改动字段的节点身份。

> 这是本 change 里**语义变更最大**的一条（节点身份稳定性），
> 因此 MUST 有对应的 spec Requirement（见 `specs/bundle-master-slave-io/spec.md`）。

### D8: MUST 有可执行回归护栏（本次缺陷之所以活到今天的原因）

新增 `tests/test_bundle_master_slave.cpp`：
- 一个 bundle↔bundle 的 FIFO DUT（master bundle ↔ slave bundle）
- 断言 TB 驱动的 `push` / 写数据**到达** FIFO；FIFO 产生的 `empty` / `full` **到达** master
- ⚠️ **DUT MUST 用多字段 bundle，且 MUST 含一个 `offset > 0` 的字段**
  （否则切片覆写缺陷测不出来 —— `SimpleBundle` 是单字段 offset=0，会**假绿**）
- ⚠️ **DUT MUST 含一个「设计只读不写」的字段**（如 `push`）⇒ 才能暴露 D0
  （只读字段恒为 0）
- 该用例 MUST 对 D0-D5 的任何一条回归**报错**（非空转）
- ⚠️ **非空转 MUST 用变异验证证明**（不是「理论上会红」）：
  实施者 MUST 临时改坏每一处（D0 恢复切片 / D1 改回常量转换 / D2 改回重绑定 /
  D3 改回 `operator=` / D4 去掉方向感知 / D5 反转 `push` 方向），
  每次确认对应用例**转红**，然后**逐一还原**

⚠️ **samples 不在 ctest、也不在 CI** ⇒ 护栏 MUST 是 Catch2 用例，不能只靠 sample 自检。

## 实施顺序（**D0 MUST 最先**）

```
D0（字段节点身份）→ D1/D2（Simulator 读写）→ D3/D4（连接语义）→ D5（方向）→ D6（顶层改端口）
```

⚠️ **D0 MUST 先于 D3/D4**：否则 `operator<<=` 的 `set_src(0, ...)` 会打在
**切片节点**上 ⇒ 对 `offset > 0` 的字段变成「取对方 1-bit 节点的第 2 位」，
**比现在更隐蔽**，且 `offset == 0` 的单字段 bundle 会**碰巧正确** ⇒
RED→GREEN 的分步验证会被污染。

## Impact

| 项 | 影响 |
|---|---|
| `include/simulator.h` | `get_bundle_field_value()` / `set_bundle_field_value()` 改走 data_map 槽位（D1/D2） |
| `include/core/bundle/bundle_base.h` | **D0**：`set_role` / `create_field_slices_from_node` 不再重建字段节点身份；**D3**：字段连接改 `<<=`；**D4**：`connect()` 方向感知（含第三分支） |
| `include/bundle/common_bundles.h` | `fifo_bundle` 方向镜像（D5） |
| `samples/fifo_bundle.cpp` / `samples/minimal_fifo_bundle.cpp` | 顶层 bundle 注册方式（D6）；`data` 回读的语义修正（见下） |
| `tests/test_bundle_connection.cpp` | D0/D3/D4 导致的断言变更（**6 处**，见 D4） |
| `tests/test_bundle_master_slave.cpp` | **新增**（D8） |
| 根 `AGENTS.md` | `CTEST_TOTAL` 181 → **182**；`tests/test_*.cpp` 111 → **112** |
| 风险 | ⚠️ **中高**。① **D0 是语义变更最大的一条**（字段节点身份稳定性）—— 现有 `bundle_base` 的「总线 + 切片」模型被整体换掉；② **D3/D4 改的是 bundle 框架的核心连接语义** —— 爆炸半径是**全部普通信号字段 bundle**（**不存在**「端口型 bundle」这类，初版提案的该论证已删除）；③ 但**实际回归面很小**：这些 bundle 的使用点极少（`fifo_bundle` 2 个 sample、`config_bundle`/`interrupt_bundle` 只打印宽度/角色）；④ **`ch_stream` 走自由 `operator<<=`**（`include/chlib/stream_operators.h:25-33`）⇒ 不经 bundle_base 的连接路径 ⇒ D3/D4 对它 no-op，但**它内部用的正是重绑定语义**（见「遗留债务」）；⑤ `fifo_bundle` 是**公开头文件** ⇒ 方向变更对下游是行为变更（但方向此前**根本没生效**，下游不可能依赖它）；⑥ D0 会改变**所有** bundle 的字段节点身份 ⇒ 对依赖切片节点的下游代码有影响（当前仓库内无此类依赖，但下游未知） |

## 遗留债务（本 change 记录但**不修**，各自独立立项）

1. **框架里同时存在两套连接语义**：D3 之后，普通 bundle 走**数据流边**，
   而 `ch_stream` 仍走**重绑定**（自由 `operator<<=`）。长期应收敛到**一种**语义，
   把 `operator=` 的重绑定保留为**显式的软件拷贝**语义并走 deprecation 轨迹。
2. **`bundle_base::operator=`（`bundle_base.h:170-175`）只拷贝总线节点、不同步字段成员**
   （其注释自承「不再自动同步成员」）⇒ `bundle_utils.h:10-17` 的
   `master()` / `slave()` helper 依赖**按值拷贝**穿过它 ⇒ 潜伏陷阱。
3. **FIFO 的读数据通路**（见 Non-Goals #1）。

## Non-Goals

1. **FIFO 的读数据通路**（`data` 只能当写数据，读数据无通路）—— 用户已裁决**本 change 不处理**，
   作为**独立 follow-up** 立项。理由：加 `rd_data` 字段是**公开 API 布局变更**
   （字段数 5→6、位宽变化、`tests/test_bundle_advanced_features.cpp` 的
   `bundle_field_count_v == 5` 断言与 `tests/test_bundle_bitstream.cpp` 的宽度断言需同步更新、
   下游有兼容风险）⇒ MUST NOT 与本次的「修既有缺陷」混提。
2. **不**改 AXI / stream / flow 等 bundle 的**字段布局**；`ch_stream` 的自由
   `operator<<=` **不在本次范围**（它不经 bundle_base 的连接路径）。
3. **不**改 `config_bundle` / `interrupt_bundle` 的字段布局（本次只让它们能真正连接）。
4. **不**引入 bundle 的新能力（如 `flip()` 语义、协议层 bundle）。
5. **不**把「bundle 作为顶层组件 IO」做成框架特性（裁决见 D6；未来若需要 ⇒ (b) 独立立项）。

## 附录 A：实施期 MUST 实测的设计点

D0-D5 已在本次审计中**逐环取证**（D0 有决定性探针），但「全部叠加后端到端跑通」
尚未验证（本次的探针在最后一步被自身的悬垂指针问题打断）。实施者 MUST 按 TDD 顺序推进：

1. **先**写 `tests/test_bundle_master_slave.cpp` 的 RED 用例（此时必红）
   —— ⚠️ DUT MUST 是**多字段** bundle 且含 **`offset > 0`** 的字段与
   **「只读不写」**的字段，否则 D0 测不出来（单字段 offset=0 会假绿）
2. **再**按 **D0 → D1/D2 → D3/D4 → D5 → D6** 逐条修，每修一条 MUST 让对应断言转绿
3. **D0 MUST 先于 D3/D4**（否则 `set_src` 打在切片节点上，分步验证被污染）
4. 探针配方（避免重复本次走过的弯路）：
   - 顶层用 `__io(ch_out<ch_bool> empty_probe; ...)` 把 slave 的 bundle 字段**引到顶层端口**观测
     —— ⚠️ **MUST NOT** 保存 `describe()` 里的局部 `ch_module` 指针（它是局部变量 → 悬垂
     → 会读出垃圾 node id，2026-10-21 的探针即栽在这里）
   - 独立 slave 探针（`ch_device<FiFo>` + `sim.get_bundle_value(dev.instance().io)`）
     与 master/slave 探针**分开**跑，用于区分「slave 自身没驱动」与「连接没传过去」
   - **「只读不写字段」的探针**（本次决定性证据）：把 slave 的 `push`（设计只读）
     引到顶层端口，TB 驱动 master 的 `push=1` ⇒ slave 侧 MUST 读到 1

## 附录 B：实施前 gap 分析（2026-10-21，自补；`metis` subagent 因 provider 鉴权故障不可用）

| # | gap | 处置 |
|---|---|---|
| B1 | **打包 API 的 64 位上限**：`axi_lite_bundle<32,32>` 总宽 **142 位** > 64 ⇒ 现 `get/set_bundle_value` 静默截断 | ✅ 已折进 D1/D2（逐字段读写，不打包） |
| B2 | **TB 写哪个字段的方向规则**：bundle 语境下 master 的 output 字段才是 TB 驱动的（与 top-level 端口直觉相反） | ✅ 已折进 D2 的注记 + D6 裁决（顶层改普通端口）自然消解 |
| B3 | **嵌套 bundle 字段**（`bundle_base.h:142-145` 的 `is_bundle_v` 分支）已经用 `<<=` ⇒ D3 修好后与普通字段一致；但 MUST 有覆盖 | ✅ 已加进 Task 4 的 verify（`ch_fragment` / `ch_flow` 套件） |
| B4 | **`flip()`**（`bundle_base.h:181-188`）硬编码 `as_slave()` 且创建**全新** bundle ⇒ D0 之后其字段是全新节点，连接走 D3 ✓；但方向语义（总是 slave）MUST 明确 | ✅ 已加进 Task 4 的 verify |
| B5 | **「非空转」容易变成空话**：「MUST 对每一类改动报错」若无变异验证 = 不可验证的承诺 | ✅ 已折进 D8（要求逐项变异验证 + 还原） |
| B6 | **Requirement ↔ 测试的映射无检查**：没有任何 Task 验证「每条 Requirement 都有对应断言」 | ✅ 已加进 Task 7 的 verify |
| B7 | **多驱动语义**：字段既被设计驱动又被 TB 写 | ✅ D2 只约束「走槽位」，方向/归属留给 D6；Task 6 MUST 复核 |
