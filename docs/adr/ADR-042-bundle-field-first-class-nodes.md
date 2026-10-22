# ADR-042: bundle 字段改为一等节点（取代「总线 + 位切片」模型）

- Status: Accepted
- Date: 2026-10-22
- Author: build-agent
- Parent: ADR-002 (bundle 连接设计缺陷修复)、ADR-014 (bundle 反射系统)
- OpenSpec change: `bundle-master-slave-io-2026-10`（capability `bundle-master-slave-io`）

## Context

ADR-002 修 bundle 连接时发现「字段已有节点，无法使用 `operator=` 或 `<<=`」，
于是引入了一个**变通**：让 `as_master()` / `as_slave()` 把每个字段**重绑定**成
「一条零字面量总线」的**位切片**（`bit_select` / `bits<W>`）。ADR-014 随后把
`create_field_slices_from_node()` 列为 Q1 修复的**前置条件确认项**
（ADR-014 §4.1 第 4 条：「确认 `bundle_base::create_field_slices_from_node()`
能正确处理 >10 个字段的切片创建」）。

该变通在 2026-10-22 的根因审计中被判定为**根本缺陷**：

> 字段若被重绑定成「零字面量总线的切片」，则**设计只读不写**的字段**恒为 0**
> —— 切片读的是那条**无人驱动**的零字面量总线。

实测（`samples/fifo_bundle.cpp`）：TB 驱动 master 的 `push=1`，slave 侧的
`io.push` 读到 **0** ⇒ **FIFO 从来没看到过 `push`/`pop`，从来没写过数据**。

更严重的是三处**静默失效**：

| # | 现象 | 机制 |
|---|---|---|
| 1 | 设计只读不写的字段恒为 0 | 字段是零字面量总线的切片 |
| 2 | 连接后读到 0 | 字段连接用 `operator=`（**节点重绑定**）⇒ 字段原有的驱动被**孤儿化**（该节点从设计图消失），新指向的节点无驱动 |
| 3 | `connect()` 覆盖 slave 的输出 | `connect()` **方向盲**（无条件 `dst.field = src.field`）⇒ 把 slave 的 `full`/`empty` 用 master 的**未驱动输入**覆盖 |

⇒ `fifo_bundle` / `config_bundle` / `interrupt_bundle` 这三个「普通信号字段」bundle
在此之前的**全部**使用点只有「打印宽度 / 打印角色 / 命名测试」——
**从未做过一次真实的 master/slave 连接** ⇒ 缺陷长期不可见
（`samples/fifo_bundle.cpp` 一直是红的：pristine HEAD 即 `empty=0` / exit=1）。

## Decision

1. **字段是一等节点**：每个 bundle 字段拥有**自己的**节点（无 src 的
   `proxyimpl` 值代理），由 `as_master()` / `as_slave()` 建立，
   **不再**由「总线 + 位切片」派生。
   - 无 src 时没有指令 ⇒ 槽位保持确定的初值 0（可寻址、确定）；
   - 被 `set_src(0, ...)` 驱动后，`instr_proxy` 把源的值搬进本节点；
   - codegen 对有 src 的代理发射 `assign field = <src>;`（不是悬空 wire）。
2. **连接语义 = 数据流边**：字段连接 MUST 用 `operator<<=`（`set_src(0, ...)`），
   MUST NOT 用 `operator=` 的**节点重绑定**语义。
   `operator=` 保留为**显式的软件拷贝 / 节点重绑定**语义（`deserialize` 路径与
   `ch_stream` 的自由 `operator<<=` 仍在用），但**不再是**连接手段。
3. **`connect()` 方向感知**：逐字段按已声明方向决定驱动方向；
   参数顺序约定为 `connect(src, dst)` ≡
   `dst.connect_field_based_on_direction_and_source(dst.field, src.field)`
   （同方向的「第三分支」下两种顺序结果相反，故写明）。
4. **方向指认按协议语义**：master = 发起方（命令 / 写数据），slave = 响应方
   （状态 / 读数据）。「镜像」只是**必要条件**——镜像但两侧指认反了同样错
   （`fifo_bundle` 的 `push` 此前就放错了边）。
5. **顶层不用 bundle 承载 TB 接口**：顶层用普通 `ch_in`/`ch_out` 端口，
   bundle 只用于**模块间**连接。
   理由：bundle 成员**不会**被注册为组件 IO ⇒ 它的信号**不会**成为生成 Verilog 的
   顶层端口（实测：生成的顶层模块只有 `default_clock` / `default_reset`）
   ⇒ 外部 TB 无法驱动生成的 Verilog。
6. **回归护栏**：新增 `tests/test_bundle_master_slave.cpp`（Catch2），
   覆盖 TB→slave / slave→master 两个方向的跨边界值传播 + 方向元数据断言 +
   未连接 bundle 的槽位读写。

## Consequences

**正面**

- bundle 第一次能承载**真实的** master/slave 连接；
- 「只读不写」的字段不再是恒 0（这是所有「消费型输入」的前提）；
- 连接不再孤儿化既有驱动 ⇒ 设计与连接**顺序无关**（此前靠「`describe()` 恰好
  先于 `connect()`」的顺序巧合）；
- 有了可执行护栏 ⇒ 这类缺陷不会再长期潜伏。

**负面 / 代价**

- **反转了 ADR-014 的确认项**（§4.1 第 4 条）与 ADR-002 的变通思路 ⇒
  本 ADR supersede 该确认项；
- 「拼接 / 切片」设施（`bundle_concat` / `bundle_slice_view`，靠「拷贝共享总线 +
  切片重派字段」工作）**已移除** ⇒ 见 **ADR-043**（它们在「字段一等节点」模型下
  **语义已失效** ⇒ 静默失效；仓库内零存活消费者）。⇒
  `bundle-base-integer-ctor` capability 的公开派生类清单由 **22 → 20**。
- `deserialize_bits_to_fields()`（同样切片重绑定）**已改为字段一等节点** ⇒
  见 `bundle-ecosystem-hardening-2026-10`（D2）；它**不是**死代码
  （经 `deserialize<>` 有活跃消费方）。
- `bundle_base::operator=`（只拷总线节点、不同步字段成员）与
  `master()` / `slave()` 工厂（按值拷贝）仍是**已知债务**
  —— 工厂的**字段节点独立性**已在 `bundle-ecosystem-hardening-2026-10`
  的 D4 收口（`operator=` 的「软件拷贝」语义则被**显式记录**进 capability，
  D5）。
- 顶层 bundle 仍**不是** Verilog 端口（见 Decision 5）。

## Supersedes / 相关

- **supersedes** ADR-014 §4.1 第 4 条（切片创建的确认项）
- **supersedes** ADR-002 采用的「字段节点由总线切片派生」变通
- 与 ADR-002 的原问题（字段已有节点 → 无法 `<<=`）**一并解决**：
  现在字段有自己的节点，`<<=` 正常工作

## 验证

- `tests/test_bundle_master_slave.cpp`：5 个用例 / 19 个断言
- **变异验证**（逐项改坏 → 必须转红）：D0 字段节点退回位切片 ✅、
  D1 get 退回常量转换 ✅、D2 set 退回重绑定 ✅、D3 字段连接退回 `operator=` ✅、
  D4 `connect()` 退回方向盲 ✅、D5 `push` 方向反转 ✅（**六项全部被抓到**）
- `samples/fifo_bundle.cpp`：pristine HEAD `exit=1` → 现在 `exit=0`
  且演示完整序列 `push → empty=0 → pop → empty=1`
- 全量回归：`ctest -LE perf` **171/171 全绿**
