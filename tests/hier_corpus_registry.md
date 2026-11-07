# 层次化路径的语料级回归 · **登记表**

> **权威**：本文件是「**哪些既有语料的 DUT 被层次化路径覆盖**」的**唯一登记处**
> ⇒ ⇒ 护栏 `scripts/verify_hier_corpus_coverage.sh` 对它做**三向双向核对**：
> ① **登记 → 用例**（登记的每个 DUT MUST 有语料回归用例）
> ② **用例 → 登记**（每个语料回归用例涉及的 DUT MUST 在本表里）
> ③ **★ 语料 → 登记**（**所有**在 dual-backend 语料里**带子组件**的 DUT MUST 在本表
>    **或** 在下方的**豁免表**里 + 原因）
>    ⇒ ⇒ ⇒ **这一条才是让栅栏不随时间退化的机制**（★ 固定清单是 snapshot，不是 fence）。

## 覆盖的两层（T0 / T1）

| 层 | 覆盖什么 | 成本 | 判别力 |
|---|---|---|---|
| **T0 产物级** | 发射**成功** · `ForceLayer` 下**真的分层**（≥2 module）· 派生端口集**自洽** · **确定性**（两次发射逐字节相同）· 层次化产物 **≠** 扁平产物 · **lint** 门 | 低（lint 只需 `--lint-only`，**不编译**） | 抓**发射器/分析层**的硬失败 |
| **T1 行为级** | 逐拍**等价**（扁平 ≡ 层次化） | 高（每 DUT **2 次**编译：flat + hier） | 抓**语义**错误（兄弟边接错 / 端口方向互换 / 1 拍错位） |

## 登记清单

| DUT 类型 | 语料文件 | 覆盖层 | 结构类 | 判别力上限 / 已知边界 |
|---|---|---|---|---|
| `Rv32iPcDut` | `tests/test_rv32i_arch_dual.cpp` | T0 | 单子组件（带 1 个 `ch_reg`） | 顶层只有 **1** 个 output ⇒ 「观测集合内部互换」这一维**不存在** |
| `ClintDut` | `tests/test_rv32i_arch_dual.cpp` | T0 | 单子组件（带 4 个 `ch_reg` · `mtime` 自由运行） | 顶层 **2** 个 output（32 bit / 1 bit）⇒ 无「同宽可交换」问题 |
| `CsrBankDut` | `tests/test_rv32i_arch_dual.cpp` | T0 | 单子组件（带 9 个 `ch_reg`） | ⚠️ **`interrupt_req` = `timer_interrupt_pending \| external_interrupt_pending`** ⇒ 它是另 2 个观测的**确定函数** ⇒ **恒相关** ⇒ 「逐拍全等」**分不清**它被交换（★ 本 change **只**做 T0 ⇒ 不做逐拍 ⇒ 该上限在此登记） |
| `StageDut` | `tests/chlib/test_pipeline_dual.cpp` | T0 | 单子组件（带 1 个 `ch_reg`） | 顶层只有 **1** 个 output |
| `ChainDut` | `tests/chlib/test_pipeline_dual.cpp` | T0 | 单子组件（带 3 级 `ch_reg`） | 顶层只有 **1** 个 output |
| `ModuleValueTransferTop` | `tests/test_module.cpp` | T0 | 单子组件（**0 reg** ⇒ **纯组合**） | ⚠️ 子组件**时序缺陷不可测**（纯组合）· 顶层只有 1 个 output 且 **≡ 激励** ⇒ 观测**不携带信息**（⇒ 只做 T0 ⇒ T0 不看观测面 ⇒ 无影响） |
| `ModuleChainTop` | `tests/test_module.cpp` | **T0 + T1** | ★ **C2**：兄弟跨模块边 + **同类型多实例** | ⚠️ 纯组合（子组件 **0 reg**）⇒ **时序判别力 = 0** ⇒ ★ T1 只覆盖**连线语义**；★ 发射器按 `sanitize(module_type_name)` 分组并编号 ⇒ 产出 `<Type>` / `<Type>_1` **两份同体 module** ⇒ ⇒ 这是 C2 的**真正**判别点 |
| `ModuleIoDirectionTop` | `tests/test_module.cpp` | **T0 + T1** | ★ **C1'**：**双子组件**（两个 `ModuleDataProcessor`） | ⚠️ 纯组合 ⇒ **时序判别力 = 0** ⇒ T1 只覆盖**连线语义**（两个子组件的 `select` 两路） |
| `ICachePrefetch` | `examples/riscv-mini/tests/test_cache_smoke.cpp` | **T0 + T1** | ★ **C3**：root 持 `ch_mem` + 子组件带 AXI 端口 | ⚠️ **已知观测偏差**：内层 `miss`(1 拍 `sread`) 与 `axi_addr`(当拍地址) 存在 **1 拍错位** ⇒ ⇒ 比对时 **MUST NOT** 把它当缺陷（★ 但它**不影响**逐拍等价：比的是 flat ≡ hier ⇒ ⇒ 两侧**同样**错位） |
| `ImmConsistencyDut` | `tests/test_rv32i_stage_dual.cpp` | **T0（拒绝边界）** | ⚠️ **不可层次化** | ★ `io().equal <<= (dec.io().imm == ids.io().imm);` ⇒ 根的 `equal` 由**两个子组件的内部线网**驱动 ⇒ ⇒ 那两条边**需要隐式端口合成**（ADR-060 §2.1）⇒ ⇒ 发射器**不支持** ⇒ ⇒ **被拒**。⇒ ⇒ 本行的用例把**拒绝原因**钉住 ⇒ ⇒ ⇒ 将来发射器**长出**隐式端口合成时 ⇒ ⇒ 该用例会红 ⇒ ⇒ ⇒ 那是「能力边界前移」的**正确信号**（⇒ ⇒ 本表 MUST 同步更新） |

## 结构类的覆盖情况（T1）

| 结构类 | 含义 | 代表 DUT | 状态 |
|---|---|---|---|
| **C1'** | **双子组件**（各自独立） | `ModuleIoDirectionTop` | ✅ |
| **C2** | **兄弟跨模块边** + **同类型多实例** | `ModuleChainTop` | ✅ |
| **C3** | **root 持 `ch_mem`** + 子组件带 AXI 端口 | `ICachePrefetch` | ✅ |
| **C1**（原计划） | **双子组件**且**各自带寄存器** | — | ⏠ **既有语料里没有可用样本** ⇒ ⇒ 唯一候选 `ImmConsistencyDut` **不可**层次化（见上表）⇒ ⇒ **记 backlog** |

## 豁免表（**每个 MUST 有原因**）

| DUT 类型 | 原因 |
|---|---|
| （暂无） | — |

> ⚠️ **豁免表不是垃圾桶**：每条 MUST 有**具体**原因 ⇒ ⇒ 护栏会核对「豁免的 DUT **确实**在语料里」
> ⇒ ⇒ ⇒ 「为一个早已不存在的 DUT 豁免」会被抓住。

### 豁免行的**写法要求**（★ 全部由机器核验 ⇒ ⇒ ⇒ 写错**必红**）

| # | 要求 | 护栏方向 | 违反时的现象 |
|---|---|---|---|
| ① | 原因 MUST **非空** | **③''** | 「豁免的 DUT 'X' **没有写原因**」 |
| ② | 原因 MUST NOT 是**占位符**（★ `—` · `-` · `N/A` · `NA` · `TODO` · `TBD` · `无` · `?` · `待定` ⇒ ⇒ **整串精确匹配**） | **③''** | 「原因是**占位符**（'—'）」 |
| ③ | 被豁免的 DUT MUST **确实**在语料的复合 DUT 集合里 | **③'** | 「不在语料的复合 DUT 集合里 ⇒ 豁免表成了垃圾桶」 |
| ④ | 被豁免的 DUT MUST **没有**语料回归用例（★ 「豁免」= 「**不覆盖**」） | **②** | 「有用例却**未登记** ⇒ 覆盖在暗中漂移」 |
| ⑤ | 同一个 DUT MUST NOT **既**在登记清单**又**在豁免清单 | **③'''** | 「既在登记清单又在豁免清单 ⇒ 两者互斥」 |

⚠️ **反例**（**必红**）：

```
| `Foo` | — |            ← 违反 ②（占位符）
| `Foo` |  |             ← 违反 ①（空原因）
| `Foo` | 暂不覆盖 |      ← ⚠️ **不**在占位符集合里 ⇒ ⇒ 绿 ⇒ ⇒ ⇒ 但它**不是**具体原因
                             ⇒ ⇒ ⇒ ⇒ ⇒ 真正的兜底是**人工评审**（机器只能判「占位符」）
```

⚠️ **限名注意**：豁免项按**简单名**匹配 ⇒ ⇒ ⇒ 语料里若以 `ns::Foo` 被调用 ⇒ ⇒ ⇒
豁免表写 `ns::Foo` 或 `Foo` **都可以** ⇒ ⇒ ⇒（★ 护栏方向 ③' 的比较**两侧都取简单名**）。

## 已知边界（**只登记，不清理**）

- ⚠️ **122 个叶子 DUT**（132 个 dual-backend DUT 里）**不在**本表 ⇒ ⇒ 理由：叶子在
  `ForceLayer` 下**仍只有 1 个 module** ⇒ ⇒ **逐拍**等价上**结构性零判别力** ⇒ ⇒
  它们**不是**有效回归对象 ⇒ ⇒ ⇒ 但它们**仍**是**发射器回归**的对象 ⇒ ⇒ 「全语料产物级」
  扫描记为 **backlog**（本 change 只做**复合** DUT）。
- ⚠️ **顶层只有 1 个 output** 的 DUT ⇒ ⇒ 「观测集合内部互换」这一维**不存在**
  ⇒ ⇒ 「逐拍全等」**分不清**输出被交换。
- ⚠️ **恒相关信号**（如 `CsrBankDut` 的 `interrupt_req`）⇒ ⇒ 同上。
- ⚠️ **纯组合 DUT**（子组件 0 reg）⇒ ⇒ 子组件的**时序缺陷**结构性测不出。

## 相关

- 护栏：`scripts/verify_hier_corpus_coverage.sh`（三向双向核对 + `--self-test`）
- 共享 fixture：`tests/hier_corpus_fixture.h`（T0 的通用判据）
- 公共 API：`include/hierarchy_equivalence.h`（P8 · 本 change **零**重复实现 ⇒ ⇒ 顺带是
  「公共面真的够用」的第三个消费方）
- capability：`hierarchical-corpus-regression`
