# Design: 边级诊断的记录、归属与渲染

> 决策层 → ADR-060 §2.4.1（三档语义）已定；本文只记**实现取舍**与**判据的判别力**。

## 1. 记录的单位：边

`SynthEdgeRecord{from_node, from_module, to_node, to_module, objects[]}`。

- **键** = `(from_node, from_module, to_node, to_module)` ⇒ 同一（边，产物）幂等（只记一次）。
- `objects` = 该边合成出的产物文本（`output port '__x_0'` / `input port '__if_0'` / `net 'w'`）。
- 记录只在**边确实合成成功**时产生 —— 见 §2 的上下文不变量。

**为什么不复用 `synth_events`**：它的单位是合成物（发射期按 module 遍历端口表 / 网表时 push）。
按合成物计数与 ADR 的「边」不可对齐：1 条边生 k 端口 + 1 网 ⇒ 计数与列举都无法满足
「首行计数 == 列出的条目数」。两个 push 点随之删除（避免两份真相漂移）。

## 2. 产物 → 边的归属：`cur_edge` 上下文

`build_chain`（第三轴 / 向上 pass-through）在**深层**创建逐级同名端口，
它收不到边参数 ⇒ ⇒ 引入 `cur_edge`（在**两个**边循环的迭代入口设置），
`record_synth_object()` 读它并把产物挂到本次迭代的边上。

**不变量**：产物必有边 —— `cur_edge.active == false` 时 `record_synth_object` 是 no-op。
（`add_pass_port` 的**幂等复用**也登记：该边确实**用到了**该端口，否则 `Fatal` 的
「边 + 端口名」会漏掉链路中段 / 顶端的端口。）

**为什么「产物归属」不会串边**：两个循环都逐条迭代边，`cur_edge` 在迭代入口重设；
`build_chain` 只在其调用者的迭代内执行。

**部分失败的边**（产物已创建、随后 `add_conn` 失败）会把产物留在记录里 —— 但那条路径
push 进 `remaining` ⇒ ⇒ 发射在「合成类诊断 collect」处**整体失败**并 `return`，
三档的成功路径渲染**不可达** ⇒ 不会污染诊断。

## 3. 渲染：一份记录，三档取用

| 档 | `error` | `result.warnings` | 产物 |
|---|---|---|---|
| `SynthesizeWarn` | 空 | 每边一行（含产物名） | 产出 + 合成注释 |
| `SynthesizeFatal` | 首行 `N cross-module edge(s) were synthesized` + `[i]` 逐边（含产物名） | 同 `Warn` 的 N 行 | **不产出**（落盘前 `return`） |
| `Reject` | 首行 `1 of N …` + **1** 条 `[1]` 边级记录（**不含**产物名） | **1** 行条件式（`would be synthesized`，不含产物名） | **不产出** |

- `Fatal` 也写 `warnings`：该档的价值 = **失败时的诊断质量**（ADR-060 v1.7）⇒ 两个载体都可用。
- `Reject` 的条件式措辞：本档**不合成** ⇒ 过去式「已合成」是**假陈述**
  （判据里有正向断言：含 `would be synthesized`、**不含** `synthesized cross-module`）。
- `Reject` 的 `warnings` 只 1 行（**镜像该档的诊断广度**）：广度由策略决定，两个载体一致。

**首行计数口径**：`Reject` 首行是 `1 of N` —— 「1」= **列出的条目数**（判据 ③ 的满足点），
`N` = 可合成边的**总数**（另行给出，避免「只有 1 条」的误导）。
`Fatal` 首行是单独的 `N`（= 列出的条目数，因为本档列全部）。

**箭头用 ASCII `->`**（诊断与产物全 ASCII；与既有 `desc` 的写法一致）。

## 4. 若不做（被否掉的备选）

| 备选 | 否掉的理由 |
|---|---|
| 保留合成物清单，只在 `Reject` 里挑「第一条边」 | 计数与列举仍来自两份口径 ⇒ 「首行计数 == 条目数」仍不可满足；`Fatal`/`Warn` 仍报不出边 |
| 新增公开字段（如 `synth_edges`）承载边清单 | 会与后续 `public-api-*` 面（R4.2 / R6）争用；`warnings` 已存在且语义相符 |
| `Reject` 也列全部边（更"有用"） | 违反 ADR-060 §2.4.1：`Reject` 与 `Fatal` 的**唯一**差异就是诊断完整度 |
| 只在发射器里改文本、判据仍抓 stderr | stderr 不可靠（缓冲 / 重定向 / 并行）⇒ 判据脆且有假绿 |

## 5. 判据（元组读法）

`(产物是否存在, 边级诊断条数, 产物注释是否存在)`：

- `Warn` ⇒ 条数读 `result.warnings`（本档**无** `error`）；
- `Fatal` / `Reject` ⇒ 条数读 `error` 的 `[n]` 行（**同时**校验 `warnings` 与 `error` 一致，
  见 §3 的镜像规则）；
- 第 1 项 MUST 先 `std::filesystem::remove` **成功**：`Fatal` 在落盘前 `return`
  ⇒ 磁盘上是**上一轮**的产物 ⇒ 「先删残留」是**判据的一部分**（不是保险措施）。

样本 `MixRow2SiblingTop`（= 行 2 + 兄弟边各一条）：**为什么必须新建**
—— 仓内四个候选各缺一条：`TwoSiblingTop` 只有网（2 边）· `OuterRow2` / `OuterRow1` 单边 ·
`sibling_wire` 的样本即 `TwoSiblingTop`。

产物注释的两类**分别计数**：`implicit port` == 1 且 `net` == 1
（1 边 ⇒ k 端口 + 1 网 ⇒ 「两类之和 == 边数」是**假**判据）。

## 6. 变异自证（逐条转红 ⇒ 还原 ⇒ **重建**）

| # | 变异（`src/hierarchy_verilog.cpp`） | 期望 | 实测 |
|---|---|---|---|
| M1 | `Fatal` 的条目去掉 `objects_text()` | 端口名断言转红 | **红**（`__x_0` 缺失） |
| M2 | `Reject` 的 `error` 塞回产物名 | 「不含产物名」断言转红 | **红**（初版**恒绿** ⇒ 见下） |
| M3 | `Fatal` 首行计数写死 `1`（仍列 2 条） | 计数一致性转红 | **红** |
| M4 | `Warn` 不 push 进 `result.warnings`（回到「只进日志」） | `synth_lines == 2` 转红 | **红** |
| M5 | 边级记录退化成合成物级（同边每产物各开一条） | `synth_lines == 2` 转红 | **红** |

**M2 的初版是恒绿的**（判别力缺口，已修）：旧断言只有 `__x_` / `__if_` / `implicit port`，
而本样本的**第一条**边恰好只合成**网** ⇒ ⇒ 塞回的是 `net '…'` ⇒ 三条断言都不命中。
补 `port '` / `net '` 两条后 M2 转红 —— 「产物名」是一整类，不能只覆盖端口前缀。

## 7. 连带影响

- 产物文本零改动 ⇒ `scripts/verify_hierarchical_rtl.sh`（R1–R8）、
  `verify_hierarchical_port_naming.sh`、逐拍等价判据均无需调整（已实测 PASS）。
- 测试计数锚点不变：catch 用例不逐条注册进 ctest（`CTEST_TOTAL` 仍是 265）。
