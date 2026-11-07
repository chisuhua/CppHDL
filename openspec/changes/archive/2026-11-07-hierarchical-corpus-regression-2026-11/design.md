# Design: 层次化路径的语料级回归（残留 1b）

> 本文是 **builder 域**的定稿设计。推导与证据在 `proposal.md` 附录 A；判据在 `tasks.md`。
> ⚠️ 本 change **不**新增公开 API 面（⇒ ⇒ 用 P8 已有的公共 API）⇒ ⇒ 无需 critic 的
> 公开 API 轮次，但**仍会**走 critic（触及护栏 + 测试基础设施 ⇒ 跨模块契约）。

## 1. 为什么**必须**是「两层」而不是「一层」

| 层 | 能抓 | 抓不到 | 相对成本 |
|---|---|---|---|
| **T0 产物级** | 发射器/分析层的**全部硬失败**：`ok=false` · 跨模块 mem 误判 · 自驱 `ch_in` · 宽度不一致 · bubbling 崩 · **module 名冲突/编号** · 端口集与 `signal_map` 不自洽 · lint 失败 · 确定性漂移 | lint 认可的**语义**错误 | 低（lint 只需 `--lint-only`，**不编译**） |
| **T1 行为级** | **语义**错误：兄弟边接错信号 · 端口方向互换 · clock/reset 接错 · 1 拍错位 · 宽度一致的错连 | —（这些正是 T3-005 那 5 个结构 DUT 的职责） | 高（每 DUT **2 次**编译：flat + hier） |

⇒ ⇒ **两层缺一不可** ⇒ ⇒ T0 广而浅 · T1 窄而深。

## 2. T0 的 6 项判据（`tests/hier_corpus_fixture.h`）

| # | 判据 | 实现 | 为什么这样判 |
|---|---|---|---|
| ① | 发射成功 | `ch::hier_eq::emit(...)` ⇒ `ok` | `error` MUST 为空 |
| ② | **`ForceLayer` 下真分层** | `signal_map[].hier[].module` 的**去重 module 数** ≥ 2 | ★ 取自**发射器自己的**真相 ⇒ ⇒ **MUST NOT** 自研文本解析器（★ 仓内教训：自研解析器「不可能完全闭合」） |
| ③ | 派生端口集**自洽** | 每个 input 端口 ⇒ ctx 里有同 `node_id` 的 `type_input` 节点；clock/reset 齐备 | ★ clock/reset 是**独立**节点类型（★ **不是** `type_input`）⇒ ⇒ MUST 接受它们 ⇒ ⇒ 否则**假红** |
| ④ | **确定性** | 两次发射的产物**逐字节**相同 | 与 T3-005 的 R4 同款 |
| ⑤ | **≠ 扁平产物** | `ch::toVerilog(ctx)` 的产物 vs 层次化产物 | 证明「分层真的改变了产物」（⇒ ⇒ ② 的补充：② 数 module，⑤ 比文本） |
| ⑥ | **lint** 门 | `verilator --lint-only --top-module top <file>`（★ **无** `-Wall`、**无** `-Wno-fatal`） | 与 `scripts/verify_hierarchical_rtl.sh` 的 R1 **同款** ⇒ ⇒ **不**自研 lint 口径 |

⚠️ **⑥ 的 skip 纪律**：verilator 不可用 ⇒ ⇒ **显式** `lint_skipped=true` ⇒ ⇒ 用例断言
`lint_skipped || lint_ok` ⇒ ⇒ ⇒ **MUST NOT** 静默通过。

⚠️ **⑥ 的边界**：lint 只需 `--lint-only` ⇒ ⇒ **不编译** ⇒ ⇒ T0 的成本比 T1 低**一个数量级**。

## 3. T1 的 3 个结构类（★ **按结构**选样，不按个数）

| 类 | 含义 | 代表 DUT | 它守什么 |
|---|---|---|---|
| **C1'** | **双子组件**（各自独立） | `ModuleIoDirectionTop` | 两个子组件的 `select` 两路 ⇒ ⇒ 覆盖「多子组件的连线」 |
| **C2** | **兄弟跨模块边** + **同类型多实例** | `ModuleChainTop` | ① 发射器的「LCA 中转 wire」路径（`src/hierarchy_verilog.cpp`）② ★ 发射器按 `sanitize(module_type_name)` **分组并编号** ⇒ ⇒ 产出 `<Type>` / `<Type>_1` **两份同体 module** |
| **C3** | **root 持 `ch_mem`** + 子组件带 AXI 端口 | `ICachePrefetch` | 「mem 在模块内自用、跨层只走端口」的**合法**形态（★ 对照 `src/hierarchy_analysis.cpp` 的「跨模块 mem」硬拒绝） |

⚠️ **「双子组件各自带寄存器」这一类在既有语料里没有可用样本** ⇒ ⇒ 唯一候选
`ImmConsistencyDut` **不可**层次化 ⇒ ⇒ **记 backlog**（★ 不得用「合成一个 DUT」来凑 ——
那会回到 T3-005 的「专用 DUT」形态 ⇒ ⇒ 缺口原样留存）。

## 4. 拒绝边界样本：`ImmConsistencyDut`

- `io().equal <<= (dec.io().imm == ids.io().imm);` ⇒ ⇒ 根的 `equal` 由**两个子组件的内部
  线网**驱动 ⇒ ⇒ 那两条边**需要隐式端口合成**（ADR-060 §2.1）⇒ ⇒ **发射器不支持**
  ⇒ ⇒ `ok=false`
- ⇒ ⇒ 本 change 把它登记为**拒绝边界**并把**拒绝原因**钉住（`implicit-port synthesis`
  MUST 出现在错误串里）⇒ ⇒ ⇒ 将来发射器长出该能力 ⇒ ⇒ 用例会红 ⇒ ⇒ ⇒ 那是「能力边界
  前移」的**正确信号**（⇒ ⇒ 登记表 MUST 同步更新）

## 5. 登记表 + **三向**双向核对（★ 让栅栏不退化的机制）

| 方向 | 判据 |
|---|---|
| ① **登记 → 用例** | 登记的每个 DUT ⇒ MUST 有对应的语料回归用例 |
| ② **用例 → 登记** | 每个语料回归用例涉及的 DUT ⇒ MUST 在登记表里 |
| ③ **★ 语料 → 登记** | **所有**在既有语料里**带子组件**的 DUT ⇒ MUST 在登记表里 **或** 在**显式豁免表**里 + 原因 |

⚠️ **③ 的实现要点**：
- 「带子组件」的判定 = DUT 的**类定义**里出现 `ch_module<`
- ⚠️ **类定义可能在 `include/` 里**（★ `ICachePrefetch` 是**库**类）⇒ ⇒ 扫描 MUST 含
  `include/` ⇒ ⇒ ⇒ 只扫 `tests/` + `examples/` 会**漏判** ⇒ ⇒ ⇒ 第 ③ 个方向就**有洞**
- ⚠️ DUT 名**可能带命名空间限定**（★ `chlib::ICacheAsync`）⇒ ⇒ 正则 MUST 允许 `::`
  ⇒ ⇒ ⇒ 否则会只截到 `chlib` ⇒ ⇒ **误判**
- ⇒ ⇒ 分析器用 **python3**（⇒ 免去 bash 正则的脆弱性），**输出可复算**

⚠️ **豁免表不是垃圾桶**：每条 MUST 有**具体**原因 ⇒ ⇒ 且该 DUT MUST **确实**存在于语料里
⇒ ⇒ ⇒ 「为一个早已不存在的 DUT 豁免」会被抓住。

## 6. 落点：**为什么用例 MUST 留在各自文件里**

既有 DUT 的类**大多在各自测试文件的匿名命名空间内** ⇒ ⇒ **无法**从外部枚举/引用
⇒ ⇒ ⇒ 「用哪些真实 DUT」这个事实**只能**留在那些文件里 ⇒ ⇒ ⇒ 而「怎么查」是**通用**的
⇒ ⇒ ⇒ 抽到 `tests/hier_corpus_fixture.h`（**共享 fixture**）。

⇒ ⇒ **改动面**：5 个既有测试文件（4 个 `tests/` + 1 个 `examples/`）+ 1 个新 fixture 头
+ 1 份登记表 + 1 个新护栏 + CI 接线。**零**改既有 DUT 的类定义 ⇒ ⇒ **零**改既有
`DUAL_BACKEND_TEST_PAIR` 调用 ⇒ ⇒ ⇒ parity 的两个锚点**不变**。

## 7. 公开 API 面的影响：**零**

⇒ ⇒ 本 change **不新增**公开符号 ⇒ ⇒ 用 P8 的 `ch::hier_eq::{emit, compare}` ⇒ ⇒ ⇒
⇒ 它顺带成为「公共面真的够用」的**第三个**消费方（第一个 `tests/test_public_equivalence_api.cpp`
· 第二个 T0 fixture · 第三个 T1 用例）⇒ ⇒ ⇒ **零**重复实现 ⇒ ⇒ ⇒ 与 emitter 漂移的风险为 0。

## 8. 口径差（★ 用户 2026-11-07 拍板 = X1）

台账 §7 残留 1b 的原文是「**行为**覆盖」⇒ ⇒ 本 change 的 T0（产物级）**不满足**该原文
⇒ ⇒ ⇒ **口径差 MUST 显式登记** ⇒ ⇒ ★ 台账措辞归 **architect** ⇒ ⇒ ⇒ 本 change
**MUST NOT** 单方面改台账 ⇒ ⇒ ⇒ 归 architect backlog。

⇒ ⇒ **X2（全语料逐拍）不可行**（132 × 2 次编译 + 122 个零判别力）⇒ ⇒ **X3（只做 T1）**
会让栅栏退化（无第 ③ 个方向）⇒ ⇒ ⇒ **X1** 是唯一自洽的选项。
