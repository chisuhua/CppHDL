# Proposal: 层次化 RTL 输出 · 第 4 段 —— 分析层（`hierarchical-rtl-analysis-2026-11`）

## Why

层次化 emitter 需要一份**结构化**的模块视图（模块清单 / 端口表 / 连接表），
而现状**零**支持：唯一的 RTL 出口 `toVerilog(filename, context*)`
（`include/codegen_verilog.h:36`）只吃 context，固定输出**一个** `module top`
⇒ ⇒ writer 内**零** `children()` 遍历、**零**实例化语句发射。

⇒ ⇒ 分析层先落地：把「谁是谁的子组件 / 谁归哪个 module / 端口有哪些 / 哪些边跨模块」
**算清楚并可逐字节比对**，再谈发射。

⚠️ **归属必须先修**（**T3-①b**）：分析层按「节点归属」切模块 ⇒ ⇒ 归属错 ⇒
**把子模块状态划进父模块** ⇒ ⇒ 产出**看起来对、实际错**的 RTL。

## What Changes

1. **`ModuleDef` / `InstanceDef`** 两个结构化类型 + 模块清单、端口表、连接表。
2. **成层判定**接入（类级 / 实例级 / `Granularity`）—— 由 **T3-①c** 提供。
3. **inline 折叠**：被 inline 的组件，其节点并入**最近的成层祖先**。
4. **隐式端口识别**（ADR-060 §2.1 的方向矩阵，含第三轴 pass-through）。
5. **悬空输入冒泡**识别（ADR-060 §2.2）。
6. **无主节点归顶层**（ADR-058 §2.1：`owner == nullptr` 的节点 ⇒ 顶层 + `CHWARN`；
   clock/reset **不告警**）。
7. **前置校验三条响亮拒绝**（MUST 在分析**之前**跑 ⇒ 诊断要指向根因）：
   ① 组件已 build 但**未 `add_child`** ⇒ 抛（给 `add_child` 修法）；
   ② 组件已登记但**从未 build**（判据 MUST 是 `not built()`）⇒ 抛；
   ③ 成层组件**自驱动**自己的 `ch_in` ⇒ 抛。
8. **golden 文本**：模块清单 / 端口表 / 连接表的**结构化**快照（供逐字节比对）。

## 不做（Non-Goals）

- **不**发射任何 Verilog ⇒ ⇒ 那是 T3-③ / ④。
- **不**做等价性验证 ⇒ ⇒ 那是 T3-⑤。
- **不**用「按信号名前缀后处理切分扁平文件」当过渡方案 ⇒ ⇒ 成员节点名字挂父前缀 +
  深度≥3 前缀重复 ⇒ ⇒ **切分边界本来就错**。

## Acceptance

- [ ] 模块清单 / 端口表 / 连接表可产出，且与 `tests/golden/` **逐字节零差异**
- [ ] **golden MUST 含至少 1 份「手推」期望**：选 1 个 DUT，其三层端口表
      （名字 / 方向 / 宽度 / 顺序）MUST 由**人按 ADR-060 §2.1 矩阵逐项推导**写成，
      **MUST NOT** 由实现生成
      ⇒ ⇒ **理由**：全部由实现生成的 golden 会把**分析层的 bug 合法化**
      ⇒ ⇒ **判别性对照**：把分析层的某端口方向写反 ⇒ 手推那份 golden **必转红**
- [ ] golden 形态 MUST 是**结构化的**（MUST NOT 自由文本），且比较限定「**同编译器 / 同 ABI 内**」
- [ ] 三条前置校验 MUST 在分析**之前**跑，且错误信息指向根因
- [ ] `owner == nullptr` 的节点归顶层 + `CHWARN`（clock/reset **不告警**）
- [ ] **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步

## Capabilities

| capability | delta |
|---|---|
| `rtl-hierarchical-output` | **ADDED ×1**（R7 不属于任何组件的节点 MUST 归顶层 module） |

⇒ 归档**禁** `--skip-specs`。
