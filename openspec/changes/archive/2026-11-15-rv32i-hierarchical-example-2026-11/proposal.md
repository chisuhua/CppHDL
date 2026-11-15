# Proposal: RV32I 的**层次化 RTL 交付物**（诉求 ④）+ 发射器的「双类消费者」缺陷修复

## Why

### W.1 诉求 ④ 至今**零交付**

用户 2026-11-10 拍板的 7 项 RTL 侧诉求里，**④ = 交付一个 RV32I 的层次化 RTL 示例**。
此前层次化输出的**全部**语料都是构造出来的小 DUT ⇒ 它们不含「同一个子输出既喂兄弟子模块、
又被父体表达式消费」这种真实微架构形态 ⇒ ⇒ 该形态上的缺陷**不可能**被发现。

### W.2 实测缺陷（2026-11-15 · 可复算）

`bash scripts/probe_r21_hier_granularity.sh` ⇒ `ok=false` · **20 条**跨 module 边无法映射
（improvement 记的 23 条是 B3 之前的边集合 ⇒ ⇒ 本次按**当前基线**订正，见 design §8）。

**根因**（ADR-070 §1.2 ③④）：发射器把「分析层记录的**消费点**名字」当成「该 pin 的**实参**」
⇒ ⇒ 判为双驱动。**危险的是反向修法极具诱惑力** —— 删掉冲突检查就能让 20 条归零，
那会让**真双驱动**静默通过 ⇒ ⇒ 本 change 把「负向对照」写进验收。

### W.3 可观测性缺口

「哪几个 pin 被改接」在公开 API 上**不可观测**（`CHWARN` 只进日志 ⇒ 不可行动）
⇒ ⇒ 与 R3.2a 同型教训。

## What Changes

| 编号 | 内容 |
|---|---|
| **R21-A** | `src/hierarchy_verilog.cpp`：**H-A** = `add_conn` 的冲突判据**方向感知**（只对 `into_child == true` 判冲突）· **H-B** = 兄弟边分支登记 `parent_landing_wire`（驱动侧 pin）⇒ ⇒ 分析层旧行被丢弃、父 `output` 端口走决策⑤ `assign` · **结构化字段** `HierarchyEmitResult::rewired_pins` |
| **R21-B** | `include/cpu/pipeline/rv32i_pipeline.h`：**7 处** `ch_module` ⇒ `ch_layer` + 显式模块类型名（ADR-070 D2 选项 ①）⇒ ⇒ 交付粒度（`RespectDeclarations`）下产物 = **8 个 module** |
| **R21-C** | 新用例 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`：交付粒度发射 · 确定性 · 单连接不变式 · 顶层端口表 · **lint 门** · **逐拍等价** · **手推期望值** · **判别性自证** |

## Out of Scope

- **SoC 端口契约**（ADR-046 D10）⇒ 零端口改动。
- **RV32I 流水线语义**（数据通路 / 停顿 / 前推）⇒ 归 B3 / B4；本 change 零语义改动。
- **`hier_rtl_products` 语料全套接入（R2–R8）** ⇒ 本轮只做「产物级 lint + 单连接不变式 +
  顶层端口表 + 确定性」四项 ⇒ ⇒ 全套接入 = 后续独立项 ⇒ 登记残差 **`id=25`**。
- **跨 LCA 的 fan-out** ⇒ 仍 fail-closed（负向对照 b 钉住）。
- **`load_lane` 成层** ⇒ ADR-070 D2 的集合是**7 处字面**，不含 B3 之后新增的 `load_lane`
  ⇒ ⇒ 本 change 按 ADR 字面实施（否则会擅自新增第 8 个**产物级契约名**）。
- **ADR-060 / ADR-066 的判据正文更新** ⇒ **architect 域**（本 change 只交回实测订正项）。

## 验收标准（可勾选）

- [x] **A1** spike：H-A + H-B 后探针 `ok == true` 且**边数 20 → 0**（不是「减少到 N」）
- [x] **A2** 产物级 lint：`verilator --lint-only --top-module top` **退出码 0** · **零 `%Error`** · **零 `%Warning`**（实测；禁 `-Wall` / 禁 `-Wno-fatal` / 禁 `-Wno-<单项>`）
- [x] **A3** 负向对照 (b)：同一子输出 fan-out 到**不同 LCA** ⇒ **仍 fail-closed** 且消息点名
- [x] **A4** 负向对照 (c)：子 output 的另一个消费者是父 **output 端口** ⇒ 实例映射**恰好一次** + `assign` 取值 + `rewired_pins` 点名
- [x] **A5** 负向对照 (a)：**结构性不可达**（`ch_in` 的 `set_driver` 是 last-wins ⇒ 同一 pin 不可能有两个 IR 驱动）⇒ ⇒ 保障载体 = **代码不变量 + capability 的反向 Requirement**（**不**写「已覆盖」）
- [x] **A6** 多消费者 ≥ 3 的 pin 有覆盖（RV32I：`hazard_unit.unnamed_output_*` 喂多个 stage）
- [x] **A7** proxy 端点（`reg_file.rs1/rs2` 作为父侧消费点）实测成立（RV32I 产物 + lint 绿）
- [x] **A8** 「改接」是**结构化字段**（`rewired_pins`），**不是** CHWARN；不破坏断言 `warnings.empty()` 的既有测试
- [x] **B1** 交付粒度发射：`ok == true` 且 **8 个 module**（7 stage + top）
- [x] **B2** 扁平产物**零扰动**（`ch_layer` 只写两个标志 ⇒ 结构性恒真 · 只作跳线）
- [x] **B3** 模块类型名**撞名**可诊断（`sanitize_verilog_name` 后同 base 不同类 ⇒ 既有 `:1017-1028` 拒绝；本 change 不新增该逻辑）
- [x] **B4** 7 个模块类型名在 capability 里登记为**产物级契约名**（⚠️ 契约强度见 ADR-070 §6 ⑦ —— **用户待裁决**，本 change 按 ADR 推荐取向实施并显式标注）
- [x] **C1** 确定性：同一设计发射两次**逐字节相同**
- [x] **C2** 单连接不变式：每个 pin 在实例语句里**恰好一次**
- [x] **C3** 顶层端口表：**根 `io()` 端口段逐字逐序** + 其余端口**逐字存在**（`⊆` 扁平面）
- [x] **C4** 逐拍等价：扁平 ≡ 层次化（198 拍 × 9 观测 · 同一段手写固件）
- [x] **C5** 手推期望值：写回序列 `{(x1,10),(x2,3),(x3,13),(x4,13),(x5,23)}` + `sw` 地址 `0x100` + ID 级指令序列 == 固件（**禁抄仿真输出**）
- [x] **C6** 判别性自证：篡改产物的一条 `assign` ⇒ 等价性**转红**
- [x] **D1** 既有语料零回归：`ctest -LE perf` 全绿 + `verify_hierarchical_rtl.sh`（R1–R9）+ `--self-test` 全绿

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| 发射器（跨 module 边） | 同一子输出的**双类消费者**从 fail-closed 改为**可发射**（走中转网 / 落点网） | 无（此前该形态直接失败） |
| `HierarchyEmitResult` | **追加** `rewired_pins`（公开面**追加**，非破坏） | 无 |
| RV32I 层次化产物 | 从**不存在**到 **8 module** | 无 |
| 扁平产物 | **不变**（结构性恒真） | 无 |
| RV32I 模块类型名 | `IfStage` / `IdStage` / `ExStage` / `MemStage` / `WbStage` / `HazardUnit` / `ForwardingMux<32>`（sanitize ⇒ `ForwardingMux_32_`）成为**产物里的名字** | ⚠️ 契约强度**待用户裁决**（ADR-070 §6 ⑦） |
