# Proposal: 层次化路径的语料级回归（残留 1b）

## Why

### 1. 缺口：**层次化路径在既有语料上零覆盖**

台账 §7 残留 1b（2026-11-06 由 oracle 从残留 1 拆出）：

> **语料级回归缺口**：层次化产物在全仓测试语料上的行为覆盖 = 仅 T3-005 的 **5 个专用
> DUT**（`DUAL_BACKEND_TEST_PAIR` **不参数化粒度** ⇒ 既有语料**一律只跑扁平**）⇒ ⇒
> 层次化路径在**既有语料**上零覆盖

⇒ ⇒ 现状：发射器 / 分析层 / 层次化 backend 的任何改动，只能被 T3-005 那 **5 个为它量身
定做的 DUT** 抓住 ⇒ ⇒ 而那 5 个 DUT 是「**按层次化的形状写出来的**」⇒ ⇒ 它们**不会**
携带既有语料的真实拓扑（AXI / RV32I stage / pipeline chain / cache wrapper / 模块链）
⇒ ⇒ **既有语料里的真实拓扑缺陷对层次化路径不可见**。

⇒ ⇒ **真实痛点不是「少几条测试」，而是「层次化路径没有回归栅栏」**。

### 2. ⚠️ planner 的实测普查**推翻了 naive 方案**

| 事实 | 实测 | 后果 |
|---|---|---|
| 语料规模 | **66** 个源文件 · **289** 处 `DUAL_BACKEND_TEST_PAIR` 调用 · **132** 个 DUT 类型 | 改 macro 签名 = **289** 处 churn |
| **★ 只有 10 个 DUT 带子组件** | `Rv32iPcDut` · `ClintDut` · `CsrBankDut` · `StageDut` · `ChainDut` · `ModuleValueTransferTop` · `ModuleChainTop` · `ModuleIoDirectionTop` · `ICachePrefetch` · `ImmConsistencyDut` | ⇒ ⇒ **122 个叶子**在 `ForceLayer` 下**仍只有 1 个 module** ⇒ ⇒ **逐拍**等价上**结构性零判别力** ⇒ ⇒ ⇒ **naive 的「289 处全铺」被证伪**（巨大 churn 换零判别力 + CI 翻倍） |
| 零成层声明 | `CH_HIERARCHY_LAYER` 的 14 个文件与 66 个 parity 文件**交集为空** | ⇒ ⇒ 既有语料**一律不声明成层** ⇒ ⇒ ⇒ **有判别力的粒度只有 `ForceLayer`** |
| 零跨模块 `ch_mem` | 全仓 `.instance().<mem>` 只有 4 处，**全部**在层次化专用测试 | ⇒ ⇒ 语料**零**跨模块 mem ⇒ ⇒ 分析层的「跨模块 mem」硬拒绝**不被语料覆盖** |

⇒ ⇒ **结论**：**MUST NOT** 做成「全语料逐拍铺开」⇒ ⇒ **MUST** 做成
「**分层（产物级 + 行为级）** + **精选子集** + **登记表 + 栅栏**」。

### 3. ⚠️ **实施期的实测订正**（planner 普查的结论**被实测推翻**）

planner 普查说 `ImmConsistencyDut`（「双子组件各自带寄存器」）是 T1 的 C1 类代表
⇒ ⇒ **实测：它**不可**层次化** ⇒ ⇒ 它的 `io().equal <<= (dec.io().imm == ids.io().imm);`
⇒ ⇒ 根的 `equal` 输出由**两个子组件的内部线网**驱动 ⇒ ⇒ 那两条边**需要隐式端口合成**
（ADR-060 §2.1）⇒ ⇒ **发射器不支持** ⇒ ⇒ `ok=false`。

⇒ ⇒ 它**不是** T0 候选 ⇒ ⇒ 而是「**拒绝边界**」的**真实样本** ⇒ ⇒ 本 change 把它
登记为**拒绝边界**并把**拒绝原因**钉住 ⇒ ⇒ ⇒ 将来发射器长出隐式端口合成时 ⇒ ⇒ 那条
用例会红 ⇒ ⇒ ⇒ 那是「能力边界前移」的**正确信号**。

⇒ ⇒ 连带后果：T1 的「双子组件**各自带寄存器**」这一结构类**在既有语料里没有可用样本**
⇒ ⇒ ⇒ 改用「双子组件（**组合逻辑**）」作代表（`ModuleIoDirectionTop`）⇒ ⇒ ⇒ **如实登记**
为 backlog。

## What Changes

1. **新增 1 个共享 fixture**（`tests/hier_corpus_fixture.h`）：T0 的**通用**判据
   （发射成功 · `ForceLayer` 下真分层 · 派生端口集自洽 · 确定性 · ≠ 扁平 · **lint** 门）
   ⇒ ⇒ **零**重复实现 ⇒ ⇒ 它用 **P8 的公共 API**（`ch::hier_eq`）⇒ ⇒ ⇒ 顺带成为
   「公共面真的够用」的**第三个**消费方。

2. **既有语料文件内新增用例**（★ DUT 的类**大多在各自文件的匿名命名空间内** ⇒ ⇒ **无法**
   从外部引用 ⇒ ⇒ ⇒ 「用哪些真实 DUT」这个事实**只能**留在那些文件里）：
   - **T0 产物级**：**9** 个可层次化的复合 DUT（各 1 个用例）
   - **T0 拒绝边界**：**1** 个（`ImmConsistencyDut` ⇒ MUST 被拒 + 原因点名）
   - **T1 行为级**：**3** 个结构类各 1 个（C1' 双子组件 · C2 兄弟边 + 同类型多实例 ·
     C3 root 持 mem + 子组件带 AXI 端口）

3. **新增 1 份登记表**（`tests/hier_corpus_registry.md`）：逐 DUT 记录覆盖层 · 结构类 ·
   **判别力上限** · 已知边界。

4. **新增 1 个覆盖栅栏**（`scripts/verify_hier_corpus_coverage.sh`）：**三向双向核对**
   （① 登记 → 用例 ② 用例 → 登记 ③ **★ 语料 → 登记**）⇒ ⇒ ⇒ **第 ③ 条才是让栅栏不随
   时间退化的机制**（★ 固定清单是 snapshot，不是 fence）⇒ ⇒ + `--self-test` 投毒。

5. **CI 接线**（护栏是**纯静态** ⇒ ⇒ 与分片无关 ⇒ ⇒ 每个分片都跑）。

## 不做（Non-Goals）

- ✗ **不**给 `DUAL_BACKEND_TEST_PAIR` 加粒度参数（既有 body 是**扁平后端语义** ⇒ ⇒
  观测集**无法**从 body 推断 ⇒ ⇒ 必须独立用例 ⇒ ⇒ 与 churn 无关）
- ✗ **不**对 **122 个叶子** DUT 做**逐拍**比对（**结构性零判别力**）⇒ ⇒ 它们归「全语料
  产物级」**backlog**
- ✗ **不**覆盖 `CounterWide<96>`（>64bit ⇒ Verilator 合法拒绝 ⇒ 判据空转）
- ✗ **不**覆盖 `Rv32iPipelineTop`（已被 T3-005 覆盖 + TCM 的 4 MB 展开数组 ⇒ 编译成本不可接受）
- ✗ **不**把跨模块 `ch_mem` 纳入语料（语料里**零例**）
- ✗ **不**宣称「逐拍全等 ⇒ 层次化路径正确」（★ 只判「层次化**变换**引入的分叉」⇒ ⇒ 两产物
  同源 ⇒ ⇒ 同构缺陷仍会一起错 ⇒ ⇒ 归既有 parity + 手推期望值义务）
- ✗ **不**新增 ADR（§11.1 A4）

## Acceptance

> 完整判据清单见 `tasks.md` §3。

- [ ] **覆盖**：**10** 个复合 DUT **全部**被登记（9 个 T0 + 1 个拒绝边界）⇒ ⇒ **T1 覆盖 3 个
      结构类各 1 个**
- [ ] **T0 判别力**：`ForceLayer` 下产物**含 ≥ 2 个 module**（⇒ ⇒ 叶子 DUT 必红 ⇒ ⇒ 从
      结构上杜绝「接叶子充数」）· 派生端口集**自洽** · **确定性** · **≠ 扁平** · **lint**
- [ ] **T1 判别力**：逐拍**等价**成立（`equal=true` · `mismatch_count==0`）
- [ ] **判别力上限登记**：逐 DUT 记录（恒相关信号 · 顶层只有 1 个 output · 纯组合 ·
      已知 1 拍错位 · 不可层次化）
- [ ] **栅栏不退化**：三向双向核对（★ 含「**语料 → 登记**」）+ `--self-test` 投毒必红
- [ ] **零回归**：`ctest -LE perf` 全绿 · samples · examples · 计数锚点同步

## Capabilities

| capability | delta |
|---|---|
| `hierarchical-corpus-regression` | **ADDED ×N**（新建 ⇒ 归档**禁** `--skip-specs`） |

⇒ 归档**禁** `--skip-specs`。

---

## 附录 A：实测证据

### A.1 普查的 10 个复合 DUT（护栏的分析器**可复算**）

`scripts/verify_hier_corpus_coverage.sh` 的分析器扫 `tests/` + `examples/` + `include/`
⇒ ⇒ 输出**恰好 10** 个「在 dual-backend 语料里被调用 **且** 类定义含 `ch_module<`」的 DUT
⇒ ⇒ 与 planner 普查**一致** ⇒ ⇒ 「122 叶子零判别力」这条**可复算**。

⚠️ 分析器 MUST 扫 `include/`（★ `ICachePrefetch` 是**库**类 ⇒ 它的 `ch_module<` 不在测试
文件里 ⇒ ⇒ 只扫 `tests/` + `examples/` 会**漏判**⇒ ⇒ ⇒ 第 ③ 个方向就**有洞**）。

### A.2 T1 的结构类（★ 实施期订正过）

| 结构类 | 代表 | 状态 |
|---|---|---|
| C1' **双子组件**（组合逻辑） | `ModuleIoDirectionTop` | ✅ |
| C2 **兄弟跨模块边 + 同类型多实例** | `ModuleChainTop` | ✅ |
| C3 **root 持 `ch_mem` + 子组件带 AXI 端口** | `ICachePrefetch` | ✅ |
| C1 **双子组件各自带寄存器** | — | ⏠ **语料里无可用样本** ⇒ ⇒ 唯一候选**不可**层次化 ⇒ ⇒ **backlog** |

### A.3 已知陷阱（仓内既有）

- ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 必须先算布尔再断言
- ⚠️ `ch::hier_eq::emit` **不建** `work_dir` ⇒ ⇒ 调用方 MUST 先建（⇒ 公共 API 的这条义务
  已写进 `docs/usage_guide/05-component-design.md` §5.4-ter）
- ⚠️ 框架的 clock / reset 是**独立**节点类型（★ **不是** `type_input`）⇒ ⇒ 端口集自洽检查
  MUST 接受它们（⇒ 否则**假红**）
- ⚠️ `verilator --lint-only` **无** `-Wall`、**无** `-Wno-fatal`（ADR-060 §2.5）⇒ ⇒ 与
  `scripts/verify_hierarchical_rtl.sh` 的 R1 **同款**
