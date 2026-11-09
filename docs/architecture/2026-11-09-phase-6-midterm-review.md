# Phase 6 中期复盘（architect 域 · 2026-11-09）

> **目的**：用户口令「对完成和后续任务进行梳理，检查下我们要实现的目标、为什么要实现这个目标、
> 怎么做、为什么要这么做、目前的任务是什么」。
> **本文件的定位**：**当前态快照 + 目标链 + 后续任务清单**（架构主题文档 · 非决策文档）⇒
> ⇒ **决策内容零变更**；本次唯一涉及已签批 ADR 的改动 = **事实订正**
> （ADR-066 **v1.4** · ADR-060 **v1.21** · 审稿记录 `.rddf/reviews/ADR-066-parent-side-landing.md` **§10**）。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**任务状态以 phase-6 为准**；本文件 §4 是
> **2026-11-09 的实测快照**，**会过期** ⇒ ⇒ 不作为状态真相使用）。

---

## 1. 目标链（**做什么 → 为什么**）

### 1.1 顶层目标（为什么要有层次化 RTL 输出）

`toVerilogHierarchical` 的存在意义（台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md`
§2 的 **N1**）= 让**层 2 IP 单元**（`ch::Component`）能成为**独立 Verilog module** ⇒
⇒ 可单独**复用 / 交付 / 综合**。它是「库级代码」的产物 ⇒ ⇒ 对**下游正确性**的要求 =
**产物必须可编译、且与扁平产物行为一致**（ADR-058 §2.3 的**逐字节不变**是硬约束）。

### 1.2 本轮的具体目标（为什么是 `ch_mem` 端口实参跨层）

| # | 理由 | 证据 |
|---|---|---|
| ① | **库自己的写法不被自己的特性支持** —— 公共库** 7 处**用该形态（`reg_file.aread(id_stage.io()…)`）⇒ 「不支持」= 库自身写法不可用 | ADR-066 v1.1 事实 ⑫（`include/cpu/pipeline/rv32i_pipeline.h:194/196/201` 等） |
| ② | **契约层早已规定了该形态的产物规范** ⇒ ⇒ 「不支持」使规范**永不可达** | ADR-066 v1.1 事实 ⑨（capability `rtl-hierarchical-output` 的 Scenario「父层消费子输出 MUST 有连接网」）· ADR-062 §2.6 落点表 |
| ③ | **诊断归因错误 ⇒ 用户改不动** —— 现状让人「把 mem 与使用者放同层」，而 mem **本来**就在同层 | ADR-066 v1.2 §2.7 理由段（实测） |
| ④ | **矩阵缺格** ⇒ 现行拒绝只是「矩阵外由拒绝覆盖」的**缺省**，不是对该形态的 affirmative 裁决 ⇒ ⇒ 契约不闭环 | ADR-066 v1.1 事实 ⑪ |

### 1.3 边界（**为什么不做更多**）

| 不做的事 | 理由 | 权威 |
|---|---|---|
| **mem 本体跨 module 也放行** | = 跨模块读别人的存储器 = **隐藏共享状态** ⇒ 设计味道 ⇒ 与决策 ⑥「跨模块 `ch_mem` 仍拒」**直接冲突**（放宽已签批裁决 ⇒ 须走取代流程） | ADR-066 v1.2 §2.7 选项 **F**（❌ 未选）· ADR-060 §2.1「mem 本体跨 module」行 |
| **发射期插入 IR 别名节点** | 被「**MUST NOT 改写 IR**」禁止（三条承重危害：破 ADR-058 §2.3 的逐字节不变 / 解释器行为变 / 两次生成不确定） | ADR-060 v1.2 · ADR-066 v1.3 §2.9 选项 **E**（❌ 已评估并驳回） |
| **只支持读侧（`addr`/`en`），写侧继续拒** | **成本不对称**：动机面（库内 7 处）**读写都有** ⇒ 半支持 ⇒ 用户改写量**不减半** ⇒ 收益不抵「半张网」的复杂度 | ADR-066 v1.3 §2.9 选项 **F**（❌ 未选） |
| **为 mem 端口合成专用端口**（`__mem_addr_<n>`） | mem 端口**不是一根信号**（= 「地址 → 数据」的**关系**）⇒ 合成端口**无处落脚**；且引入第二套命名 | ADR-066 v1.3 §2.8 ④ · §2.9 选项 **B**（❌ 未选） |

---

## 2. 怎么做 + 为什么这么做（**机制**）

### 2.1 三桶判定（决策 ⑦ · ADR-066 v1.2）

| 桶 | 判据（**归属比对** · 节点级） | 处置 |
|---|---|---|
| **(i)** | **持有 module**（= `port->parent()` 即 mem **本体**的 inline 折叠后归属）≠ **使用 module**（= mem 端口节点自身的折叠后归属） | **响亮拒绝**（mem 本体跨 module） |
| **(ii)** | ① == ② **且** 跨 module src **仅**落在 `addr`/`en`/`wdata` 三槽 **且** 该 src owner **相邻**（父子） | **放行**（归下游 P5 实施） |
| **(iii)** | ① == ② 但 src owner **非相邻**（孙 / 兄弟） | **仍拒**（相邻性规则） |

⇒ ⇒ **互斥且穷举**（缺「相邻性」约束时形态 E 同时满足 (ii) 与 (iii) ⇒ 「互斥」被证伪 ⇒
⇒ 该约束是 critic 第 1 轮 BLOCKER 的产物）。

### 2.2 `(ii)` 的产物形态（决策 ⑧ · ADR-066 v1.3）

**复用决策 ① 的父侧落点网**（**由登记触发**）⇒ ⇒ 不新造机制 ⇒ ⇒ 与决策 ① / ⑤ **同命名**
⇒ ⇒ 同一子 `ch_out` 被多处消费时 MUST 扇出**同一根**网 ⇒ ⇒ **命名恒等**
（网名 = 驱动端点的 ctx 全局名 = `node_names_` 的值）⇒ ⇒ **codegen 侧 MUST NOT 查重定向表**
（恒等映射 = 死代码 ⇒ Zero-Debt）。

### 2.3 `en` 槽 fail-closed（决策 ⑧ ⑥）

`enable == nullptr` ⇒ `1'b1`（**合法缺省** = 恒使能）· `enable != nullptr` 且**取不到名字**
⇒ **MUST fail-closed** ⇒ ⇒ **MUST NOT** 回退 `1'b1` ⇒ ⇒ 否则读侧恒使能 / 写侧恒写 ⇒
⇒ **值错且 lint 绿**（⇒ 这是本次改动面里**唯一真正静默**的面）。

---

## 3. ★ 三项实测发现（本轮复盘的**实质产出**）

> **方法**：最小复现探针 + **改前二进制对照**（`HEAD~1` = `364e213^`）⇒ ⇒ 每条结论都有
> **可复跑**的证据，而非推断。

### 3.1 ★ 发现 1：**承重事实错误**（已订正 → ADR-066 **v1.4** · ADR-060 **v1.21**）

**ADR-066 v1.2 §2.7 的「机制订正」段断言**：现行 `:543` 的**边闸门**对字面 (i) **不可达**
（mem 本体**零** `add_src` ⇒ 端口与其 src 的 owner 同 module ⇒ 不产生跨 module 边）
⇒ ⇒ 「现状 = 分析放行 + 发射期静默丢端口」⇒ ⇒ **契约-实现缺口**。

**实测证伪**（改前二进制 · 最小复现 = 父持 `addr` + `child.instance().mem.aread(io().addr)`）：

```
[FATAL] hierarchical analysis: cross-module ch_mem access ('top.mem_aread' in 'top'
        <- 'top.child.mem' in 'top.child'). ADR-060 requires the memory and its user
        to be in the same module. … at src/hierarchy_analysis.cpp:544
```

⇒ ⇒ **现状 = 已响亮拒绝**（**不是**「放行 + 静默丢端口」）⇒ ⇒ **无契约-实现缺口**。
**漏看的边** = mem **端口**节点的 src 列表**含 mem 本体**
（`include/ast/mem_port_impl.h:107`（读端口）/ `:145`（写端口）的 **`add_src(parent)`**）
⇒ ⇒ 「mem 本体**零** `add_src`」**不等于**「mem 本体与端口**之间无边**」。

**⇒ ⇒ 影响（已处置）**：
- ADR-066 **v1.4** §2.7 机制订正段（订正为实测事实）+ 判别程序 ③ **职责界定**（归属比对判定的
  价值 = **独立于边结构** + **归因精确** + **三桶可判** ⇒ ⇒ **不是**「补一条缺失的拒绝」）+
  §5 **V20 预期值订正**（= **已响亮拒绝** ⇒ ⇒ **无**契约-实现缺口 ⇒ ⇒ 探针的实质交付物 =
  **(i)/(ii) 归因分开**）· **决策内容零变更**。
- ADR-060 **v1.21** §2.1「mem 本体跨 module」行的「现状实现缺口（v1.17）」段订正 +
  「mem 端口实参跨层」行追加**实测注记**。
- ⚠️ **对 P5 的直接影响**：`tasks.md` **S1**（V20 探针）的**预期值 MUST 用订正版** ⇒ ⇒
  否则探针会得出「与预期不符」的**假结论** ⇒ ⇒ **实施前必须同步**（planner 域 · 见 §5 待办）。

### 3.2 ★ 发现 2：`(ii)` 的产物形态**已由既有机制自动满足**（⇒ S3 **不需要新代码**）

实测产物（`(ii)` 最小复现 = 父持 `ch_mem` + 成层子 `ch_out` 驱动 `addr`）：

```verilog
module top ( … );
    logic [7:0] top_mem [0:15];
    logic [3:0] top_prod_unnamed_output;      // ★ 父侧落点网（**有显式声明** ⇒ 非 implicit wire）
    assign top_mem_aread_data_proxy = top_mem[top_prod_unnamed_output];   // ★ 读该网
    producer prod ( … .top_prod_unnamed_output(top_prod_unnamed_output) );  // ★ 子 output .port(net)
endmodule
```

⇒ ⇒ 决策 ⑧ ① / ② **得到实测确认**：父侧网由**既有机制**（`register_parent_wire` + `add_conn`）
**自动合成** ⇒ ⇒ **登记义务在 `(ii)` 路径上天然满足** ⇒ ⇒ **codegen 侧确实无需查表**
（`node_names_[addr]` 已等于网名 ⇒ 命名恒等）⇒ ⇒ **决策 ⑧ ② 的「恒等映射 = 死代码」判断成立**。

⇒ ⇒ **对 P5 的直接影响**：**S3 的「登记义务」不需要写产品代码** ⇒ ⇒ 交付物 = **产物级断言**
（AC4：产物内该名字 MUST 有 `logic` 声明）⇒ ⇒ 值判据（AC3）成为 (ii) 支持态的**主判据**。

### 3.3 ★ 发现 3：P5 在 (i) 上的实质交付 = **归因分开**，不是「新增拒绝」

由发现 1 ⇒ ⇒ (i) **今天已被拒绝** ⇒ ⇒ P5 在 (i) 上的**唯一实质交付** =
**诊断把 (i) 与 (ii) 分开说**（(i) 说「mem 本体跨 module」· (ii) 说「仅实参视线跨层」）
⇒ ⇒ 与 ADR-066 v1.2 §2.7 判别程序 ③ 的**职责界定**一致。

⇒ ⇒ ⚠️ **连带**：phase-6 **P5 判据 ②**（「P4 裁『不支持』时」分支）**已成死分支**
（决策 ⑧ 已裁定「支持」⇒ ⇒ 该分支的前提消失）⇒ ⇒ **planner MUST 订正 phase-6 该行**
（否则 builder 会照字面执行一条**不存在**的分支）。

---

## 4. 目前的任务是什么（**现状快照**）

### 4.1 当前 change

`openspec/changes/mem-port-arg-cross-layer-2026-11/`（**WIP** · commit `364e213`）——
AC1–AC10 · 实施顺序 S0–S6。

| 步 | 内容 | 状态 |
|---|---|---|
| **S0** | 基线（`ctest -LE perf` + 锚点） | ⬜ 未记录（**须补**：AC10 依赖基线对照） |
| **S1** | V20 探针（字面 (i) 现状登记） | ⬜ **预期值已被 v1.4 订正**（见 §3.1）⇒ **实施前必须同步** ⇒ ⇒ ⚠️ **仍必要**（删掉会失去台账/登记表**归因登记**载体 + 「不放松」负向护栏）⇒ ⇒ 形态 = 「登记现状 = **已响亮拒绝**」+「(i)/(ii) 归因分开」 |
| **S2** | 判别程序（三桶 + 归属比对） | ✅ **已提交**（`364e213`）· build 通过 · `ctest -R hier` **4/4 绿**（既有 42 格矩阵 / 分析层 / 逐拍等价**未回归**） |
| **S3** | `(ii)` 放行 + 登记义务 | 🟡 **放行已生效**（实测）⇒ ⇒ **登记义务无需新代码**（见 §3.2）⇒ ⇒ **缺 AC3 值判据 + AC4 声明断言** |
| **S4** | `en` 槽 fail-closed + 归因订正 + 注释同步 | ⬜ **代码未改**（`src/codegen_verilog.cpp` 读侧 `:1294-1297` / 写侧 `:1368-1371` 仍静默回退 `1'b1`） |
| **S5** | V22 变异 + V18 复算 | ⬜ |
| **S6** | spec delta + 归档 + 台账行 20 闭口 | ⬜ |

### 4.2 phase-6 其余未完成步（**不做重复劳动**：逐条**实测核对**过）

| 步 | 内容 | 实测现状（2026-11-09） | 未做 |
|---|---|---|---|
| **P6** | RV32I ISA 缺口（CSR / `ECALL`·`EBREAK` / `FENCE`） | `OP_SYSTEM` **仅定义**（`include/cpu/pipeline/rv32i_decoder.h:52`）· **零使用** ⇒ 缺口确认；`CsrBank`（`include/cpu/riscv/rv32i_csr.h`）· `ExceptionUnit`（`rv32i_exception.h`）**已存在** ⇒ ⇒ 缺口 = **指令级集成**，**非**从零实现 | 全部 |
| **P7** | 三档 `PortPolicy` 在可合成形态上的语义分化 | `port_policy` 的**唯一读取点** = `src/hierarchy_verilog.cpp:956`（**失败路径**）⇒ ⇒ `SynthesizeFatal` 与 `SynthesizeWarn` 行为**完全相同** ⇒ ⇒ 三档**未分化**；`CppHDL-synth` 产物注释 = **零实现** | ②③④⑤ |
| **P8** | `Simulator::set_ab_verification()` 是 no-op stub | `set_ab_verification` / `is_ab_verification` / `ab_verification_` / `ab_result_a` / `run_ab_verify` **仍在**（`include/simulator.h:95-96,655` · `src/simulator.cpp:297-300,332,529-530`）⇒ ⇒ 路线 **A（删 stub）** 未实施；golden 覆盖 **5** 用例 / `include/jit/jit_ir.h` **39** 个 `JitOp` ⇒ ⇒ **下限 5/39 恰好达标**（防回退锚点） | ①②④ |
| **P11** | ADR-036 逻辑算子**物理移除** | 🔒 **日历锁**（最早 **2027-04-17** · `verify_deprecation_calendar_gate.sh` 守） | — |

### 4.3 已知但**未**在本窗口承接的 architect 域遗留

| 项 | 事实 | 归属 |
|---|---|---|
| **P10 登记的 ADR 判据漂移** | ADR-052 `:234` / ADR-063 `:236` / ADR-067 `:270` 仍写「`openspec validate --specs --strict` 绿（**CLI 须 1.4.1**）」⇒ ⇒ 实测 CLI = **1.14.3** · 仓级 **红**（21 passed / 23 failed / 44 items）⇒ ⇒ 这三份是 **Accepted ADR 的判据** ⇒ ⇒ **planner / builder MUST NOT** 单方面改 | **architect**（独立修订 · 本窗口未做） |

---

## 5. 后续任务清单（**归属 + 前置**，详细顺序见 phase-6）

| # | 任务 | 归属 | 前置 | 判据形态 |
|---|---|---|---|---|
| **A** | **订正 phase-6 P5 行**：判据 ②（死分支）· 判据 ⑤（「边闸门对字面 (i) 不可达」这个**理由**已失效 ⇒ 换成 v1.4 版）· 判据 ⑥（V20 **预期值**）· S1 预期 | planner | 本次 §3 | 逐字可 grep |
| **B** | **订正 change 文档全套**：`proposal.md` What Changes **#2**（括注）+ **#6**（V20 预期值）· `design.md` §「现状」段（`:19`）· `tasks.md` **S3** 的「登记义务」表述（**不需要新代码** ⇒ 见 §3.2）⇒ ⇒ ⚠️ **B 的对象是 `proposal.md` / `design.md`**（错误预期值在那儿）⇒ ⇒ `tasks.md` **S1 本身不含**错误预期值 | planner | A | 同上 |
| **C** | **AC1 负向用例**（字面 (i) / 非相邻 (iii) MUST 仍红）⇒ ★ **(i) 的既有用例已存在**（oracle 已确证：`tests/test_hierarchy_analysis.cpp:666` 的 `MemTop`/`MemUser` = **桶 (i)**（mem 在父 · 端口在子）⇒ ⇒ S2 的独立扫描对**两个方向**的 (i) 走**同一条扫描路径 + 同一文案** ⇒ ⇒ (i) 负向覆盖**实质已存在**）⇒ ⇒ **不需**为 (i) 重做用例 ⇒ ⇒ 只补 **(iii) 非相邻** + 「不放松」断言 | builder | B | 负向用例转红 |
| **D** | **AC3 值判据**（读 `addr` 与写 `wdata`/`en` **两侧** · 精确期望值 · 手推） | builder | C | 值判据 |
| **E** | **AC4 声明断言**（产物内 mem 实参引用的名字 MUST 在父 module 有 `logic` 声明 ⇒ 非 implicit wire） | builder | D | 产物断言 |
| **F** | **AC5 `en` 槽 fail-closed**（两处共用 helper ⇒ 免漂移） | builder | E | 诊断断言 |
| **G** | **AC7 V22 变异**（不登记落点网 ⇒ 值判据转红 · 先编译确认 · 还原后重建） | builder | F | 变异自证 |
| **H** | **AC9 spec delta + 归档**（禁 `--skip-specs`）+ **AC10 台账行 20 闭口**（`已裁决 → 已收口` · `归属 → —` · 闭口证据 = archive 路径）+ 登记表**归因描述**同步（本文件 §3.1 的订正） | builder | G | 护栏绿 |
| **I** | **P7 builder 步**（三档分化 · 样本四约束 · 失败档先删残留产物） | builder | 与 A–H **可并行**（写点函数级不重叠） | 两两可区分元组 |
| **J** | **P8 builder 步**（删 stub + P14 登记 + golden 下限锚点 + 两份文案订正） | builder | 与 A–H **可并行** | `verify_public_api_visibility.sh` 绿 |
| **K** | **P6-0 侦察**（复用面清单 + `FENCE` 裁决）→ 立项 | planner → builder | 与 A–H **可并行** | capability spec |
| **L** | **P10 的 ADR 判据订正**（ADR-052 / ADR-063 / ADR-067 的 CLI 版本判据） | architect | 独立 | oracle → critic |

---

## 6. 遗留清扫（**本次范围内**）

| 项 | 状态 |
|---|---|
| ADR-066 v1.2 机制订正段的事实错误 | ✅ **已订正**（v1.4）· 审稿记录 §10 |
| ADR-060 v1.17「现状实现缺口」段的事实错误 | ✅ **已订正**（v1.21） |
| ADR-066 v1.4 / ADR-060 v1.21 的审稿（oracle → critic） | ⏳ **待跑**（本次提交前完成） |
| phase-6 P5 判据 ②（死分支）/ ⑥（V20 预期值） | ⬜ **planner 域待办**（§5-A） |
| `tasks.md` S1 预期值 / S3 表述 | ⬜ **planner 域待办**（§5-B） |
| ADR-052 / ADR-063 / ADR-067 的 CLI 判据漂移 | ⬜ **architect 域待办**（§5-L · **不在**本窗口其余部分） |
| 探针脚本 / 临时文件 | ✅ 落在 `/tmp/opencode/`（未入仓）· 工作树无残留 |
| ⚠️ **index 残留**（oracle 第 1 轮发现 · **已修**）：复现「改前二进制」时用了 `git checkout HEAD~1 -- <file>` ⇒ ⇒ 该命令**同时改了工作树与 index** ⇒ ⇒ 还原后工作树已复原，但 **index 里仍暂存着 S2 的回滚**（-64/+13）⇒ ⇒ 若直接 `git commit` ⇒ ⇒ **S2 成果被静默回滚** ⇒ ⇒ 已 `git restore --staged src/hierarchy_analysis.cpp` 清理 ⇒ ⇒ **教训**：任何「对照旧版本」的 `git checkout <ref> -- <path>` 之后 MUST `git restore --staged <path>` |

---

## 7. 判据自检（**可复跑**）

```bash
# ① ADR-066 v1.4 的机制订正段已就位（逐字锚点）
grep -c "事实错误" docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md   # ≥ 1
grep -c "add_src(parent)" docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md  # ≥ 1
# ② ADR-060 v1.21 订正已就位
grep -c "v1.21" docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md      # ≥ 1
# ⚠️ MUST 用**区间限定**（全文件级 grep 会被 v1.21 元数据行的**自指**命中 ⇒ 恒红）
awk '/^### 2.1/,/^### 2.2/' docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md \
  | grep -c "现状实现缺口（v1.17）"                                          # == 0
# ③ 事实依据（可复跑）
grep -n "add_src(parent)" include/ast/mem_port_impl.h      # 107（读）/ 145（写）
# ④ 复审记录已落（⚠️ **时序**：本检查在 §10 追加**之后**才为绿）
grep -c "^## 10\." .rddf/reviews/ADR-066-parent-side-landing.md   # == 1
# ⑤ 下游残留清零（planner 完成后才为绿 ⇒ ⚠️ 命中须带「证伪 / 订正」语境）
grep -rn "不可达\|静默丢端口" .rddf/roadmap/phases/phase-6.md \
  openspec/changes/mem-port-arg-cross-layer-2026-11/{proposal.md,design.md}
```
