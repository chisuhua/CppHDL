---
name: p8-hierarchical-equivalence-public-helper-2026-11
priority: P2
source: 台账 §7 残留 1（用户面缺口）+ §7 残留 11（权威登记）+ §11.1 A4（裁决）+ §11.2（形态约束）+ roadmap `feat-hierarchical-rtl-output` §③ 后续立项清单（P8 行）
phase: phase-5
category: api
type: feature
created_at: 2026-11-07
evidence: probe:docs/architecture/2026-11-02-hierarchical-rtl-output.md#7·#11.2
---

# Improvement: 层次化等价性 harness 公共化（P8）

- **Date drafted**：2026-11-07（**v2** —— v2 吸收 metis 第 1 轮 gap 分析的 12 条 finding）
- **Drafted by**：build-agent（planner 角色）
- **Status**：📝 草稿 v2（**待用户拍板** ⇒ 拍板后转 builder 流程）
- **权威登记**：台账 §7 残留 1（= P8 的**用户面**缺口）· §7 残留 11（= P6 + P8 的共同权威登记）·
  形态约束 = 台账 §11.2 的 P8 行（**唯一权威** ⇒ 本文件只做指针与展开，**不复制全文**）
- **裁决**：§11.1 **A4**（2026-11-05）⇒ 独立立项 · **不为此立新 ADR**（API 尚未设计 ⇒ 为未设计的 API
  写 ADR = 过早固化）
- **用户已拍板的部分**：**Capabilities 走最小承诺**（只暴露 `emit` + `compare` 两个入口 + 结果结构体）

---

## 1. Why (背景与动机)

### 1.1 用户的痛点：**自己的设计拿不到「层次化后行为不变」的证据**

台账 §7 残留 1（2026-11-06 由 oracle 从残留 1 拆出）：

> **用户面缺口**：层次化产物的等价性 harness 仍在 `tests/` 内部 ⇒ ⇒ **用户自己的设计
> 拿不到**「层次化后行为不变」的逐拍反馈（⇒ ⇒ 这才是 P8 的原始痛点）

现有等价性证据**全部**在仓内测试里：`tests/hier_equivalence.h`（T3-005 的 9 用例 / 414 断言）
⇒ ⇒ 用户的仓外代码 **include 不到** ⇒ ⇒ 用户**没有任何**可用的手段回答
「我把设计改成层之后，行为变了吗」。

### 1.2 为什么「把 harness 挪进 `include/`」不是一行 commit

用户要自己复现这个证据，必须重写**四步**，而每一步都有**静默失效**的陷阱：

| 步 | 陷阱 | 后果 |
|---|---|---|
| ① 生成层次化产物 | `GranularityScope` 是**静态**开关 ⇒ 必须**只**包住发射 | 泄漏到别的阶段 ⇒ 行为漂移 |
| ② 派生端口集合 | 端口集合**必须**由 emitter 产物派生（`backend_port_set`）⇒ ⇒ **MUST NOT** 手写列举 | 手写 ⇒ 与 emitter 真相脱钩 ⇒ 假绿 / 假红 |
| ③ 装配两个 backend | 扁平侧**必须**用「backend 自己生成的那份 `top.v`」⇒ ⇒ 不得另生成一份 | 两份不一致 ⇒ 「等价」结论无意义 |
| ④ 施加激励 | 激励 MUST 走 `Simulator::set_input_value(port, <纯整数>)` | 用 `<<=` ⇒ 顶层端口变成「有内部驱动」⇒ 两侧观测面不一致 ⇒ **假分歧** |

⇒ ⇒ 换句话说：**门槛不在「写多少行」，在「知道每个陷阱在哪」** ⇒ ⇒ 这正是框架该提供的价值。

⇒ ⇒ ⚠️ **metis 第 1 轮补充**：这四步的陷阱**应当由 helper 在结构上消除**，而不是**写进文档提醒用户
自己避开**。⇒ ⇒ 本 change 的设计原则因此收紧为：**凡「用户填错就静默失效」的字段，helper MUST NOT
让用户能填错**（⇒ 端口集合只由 `emit` 产出 · 观测信号只由工厂函数产出 ⇒ ⇒ 见 §2.2）。

### 1.3 边界：P8 **不是**「双后端 parity 的替代」

⚠️ 必须写进公共 API 的文档，否则用户会误用：

| | P8（等价性 helper） | 既有双后端 parity |
|---|---|---|
| 比较什么 | **两份 Verilog 产物**（扁平 vs 层次化） | **C++ 解释器 vs Verilog 编译产物** |
| 判别什么 | 「**层次化变换**是否引入行为分叉」 | 「**两套执行后端**是否行为一致」 |
| 对「共享发射底座算错」 | **结构性失明**（两产物同源 ⇒ 同构缺陷一起错） | **能抓**（解释器 vs 编译产物不同源） |
| 对「层次化变换算错」 | **能抓** | **结构性失明**（两层都由同一份 C++ 生成） |

⇒ ⇒ 两者**互补**，**不可互相替代** ⇒ ⇒ 公共 API 的文档 MUST 明说。

### 1.4 与 P6 的关系（同一族「用户面缺口」的两半）

台账 §7 残留 11 同时登记 P6 + P8：

> **用户看不到自己的设计是否「层次友好」**（诊断 API）· **用户拿不到自己 DUT 的等价性反馈**（公共 helper）

⇒ ⇒ **P6 已收口**（2026-11-07）⇒ ⇒ 本项是那半边：**「能不能改」+「改了会不会变」**。

---

## 2. What Changes (变更范围)

### 2.1 新增 1 个公共头（公共 API 面 **+1**）

`include/hierarchy_equivalence.h` · 命名空间 **`ch::hier_eq`**（v2 **写死** —— v1 留了三个候选
⇒ ⇒ metis finding #9：悬置命名会让实施期的白名单反复改）。

⚠️ **已知事实（写进文档）**：头文件名（`hierarchy_equivalence.h`）与命名空间（`ch::hier_eq`）
**不同名** ⇒ ⇒ 用户 `grep` 头名时找不到符号名 ⇒ ⇒ 文档 MUST 两处都写。

### 2.2 公共面 = **一个闭合集合**（v2 **不再用「符号个数」当质量指标**）

> ⚠️ **metis finding #10**：v1 写「共 6 个公开符号 · 评审时按 6 个算」⇒ ⇒ 那是**代理指标**
> ⇒ ⇒ 「少暴露 = 好」会诱导实现者**砍功能来凑数**（而 A2 的双向比对**不惩罚功能缺失**）
> ⇒ ⇒ v2 改为：**公共面 = 一个闭合集合 ⇒ 逐符号登记 ⇒ 增删 MUST 改 spec + 护栏白名单 + 双向核对**。

| # | 暴露 | 种类 | 说明 |
|---|---|---|---|
| 1 | `ch::hier_eq::emit(...)` | **入口** | 生成层次化产物 + **由产物派生**端口集合 + 映射表 |
| 2 | `ch::hier_eq::compare(...)` | **入口** | 逐拍比对「扁平产物」与「层次化产物」 |
| 3 | `ch::hier_eq::ProductSet` | **结果结构** | `emit` 的返回（`ok` / `error` / `hier_path` / `ports` / `signal_map`） |
| 4 | `ch::hier_eq::Result` | **结果结构** | `compare` 的返回（`ok` / `equal` / `error` / `first_mismatch` / `mismatch_count` / `flat_trace` / `flat_product` / `hier_product`） |
| 5 | `ch::hier_eq::DrivenInput` | **输入类型** | `compare` 的参数之一（`name` + `set` 回调） |
| 6 | `ch::hier_eq::Observation` | **输入类型** | `compare` 的参数之一（`name` + `read` 回调 + `node_id`） |

⇒ ⇒ **「最小承诺」的精确化**（v1 遗留的 Q1 ⇒ **已答**）：`emit` / `compare` 的**参数类型**也是
公共面 ⇒ ⇒ **不暴露它们就无法调用** ⇒ ⇒ 「最小承诺」的**可执行形态** = **2 入口 + 2 结果结构
+ 2 输入类型** ⇒ ⇒ ⚠️ **第 5 / 6 项不是「新增承诺」，而是「调用所必需」** ⇒ ⇒ MUST 在文档里
分开列 ⇒ ⇒ 评审时**按这 6 项逐个核**，**不按「总数」核**。

**v2 的两处修正（metis #1 / #4）**：

- **① `Trace` 类型别名取消** ⇒ ⇒ `Result::flat_trace` 的类型**内联**写
  `std::vector<std::vector<std::uint64_t>>` ⇒ ⇒ 「公共符号」集合里**没有** `Trace`
  ⇒ ⇒ 上表**恰为 6 项** ⇒ ⇒ A2 的白名单**不会**因别名而漂移。
- **② `Result::trace` 改名为 `Result::flat_trace`** ⇒ ⇒ **实测依据**：现有实现**只 push 扁平侧**
  （`tests/hier_equivalence.h:190` 的 `row.push_back(vf)` ⇒ 层次化侧 `vh` 只参与比较、**不入轨迹**）
  ⇒ ⇒ v1 的 `trace` 这个名字**语义不符**（暗示「两侧轨迹」）⇒ ⇒ metis finding #4：
  「用户第一次查看时就会变成 bug 报告」⇒ ⇒ **改名**。
  ⚠️ **代价**：`tests/test_hierarchy_equivalence.cpp` 会有**一处机械改名** ⇒ ⇒ B1 的「零改动」
  MUST 放宽为「**断言集合与期望值不变**（允许字段机械改名）」⇒ ⇒ 见 §3.2。

**观测信号的生产方式（v2 新增 · metis #5）**：

- `Observation::node_id` **保留**（理由：它让 `compare` 能**自己**做「被观测信号 MUST 出现在映射表里
  且 `hier_top_port_name` 非空」的对账 ⇒ ⇒ 那是 delta 的 Scenario 契约 ⇒ ⇒ 必须有机械载体）
  ⇒ ⇒ **但用户 MUST NOT 手填**：公共面提供**工厂函数** `make_observation(name, port, read)`
  ⇒ ⇒ 由 helper 自己取 `port.impl()->id()` ⇒ ⇒ **公共文档里不出现 `impl()`**（v1 的「首跑卡点」）。
- ⇒ ⇒ **「盲区侧」的对账**（不在观测面内的 `ch_out` MUST 出现在映射表里且 `hier_top_port_name`
  为空）**留在仓内测试**（需要枚举 DUT 的**全部** `ch_out` ⇒ 那是 DUT 的知识 ⇒ helper 拿不到）
  ⇒ ⇒ MUST 在文档里说明「这一半由仓内判据承担」。

⇒ ⇒ **内部实现**（产物生成 · backend 装配 · 逐拍驱动 · 目录创建 · 文件读取 · 对账）**MUST NOT**
出现在公共面 ⇒ ⇒ 落 `src/` 内部头（★ 仓内先例：`src/hierarchy_chain.h` 等 4 个）。

### 2.3 `tests/hier_equivalence.h` 改为「include 公共头 + alias」

```cpp
// tests/hier_equivalence.h  （改造后）
#pragma once
#include "hierarchy_equivalence.h"     // ★ 公共面
namespace ch_test::hier {
using Observation = ch::hier_eq::Observation;
using DrivenInput = ch::hier_eq::DrivenInput;
using ProductSet  = ch::hier_eq::ProductSet;
using Result      = ch::hier_eq::Result;
using emit        = ch::hier_eq::emit;
using compare     = ch::hier_eq::compare;
}
```

⇒ ⇒ 现有 `tests/test_hierarchy_equivalence.cpp`（9 用例 / 414 断言）**断言集合与期望值不变**
（仅 `trace` → `flat_trace` 的机械改名）⇒ ⇒ **零行为回归**。

### 2.4 ⚠️ 护栏**改口径**（`scripts/verify_hierarchical_p8.sh`）

现有 R1–R4 是**为「T3-005 MUST NOT 暴露公共 API」写的** ⇒ ⇒ 公共化**必然**与它们冲突
⇒ ⇒ MUST 改。⚠️ **v1 的改法被 metis finding #3 判为「把更强的判据换成更弱的」** ⇒ ⇒ v2 改为
**「全称否定 + 单一显式豁免」**（新旧判据是**超集**关系 ⇒ ⇒ 「强度不减」这句话**才有载体**）：

| # | v1 提法（❌ 被否） | **v2 提法（✅ 采纳）** | 强度对比 |
|---|---|---|---|
| **R1** | 「`include/` 下 harness 头**恰好一个**且**只**是 `include/hierarchy_equivalence.h`」（**存在性**） | 「`include/` 下**文件名**匹配 `*equivalence*` 或 `*hier_eq*` 的文件 ⇒ MUST **逐字等于** `include/hierarchy_equivalence.h`」（**全称否定 + 单一字面豁免**） | v1 = ∃（严格变弱）⇒ v2 = ∀（**超集**） |
| **R2** | 内部实现符号 deny-list（不变） | 同左 **+ 双向核对**（★ metis #8：deny 符号被改名后「grep 无命中 ⇒ 绿」⇒ 判据**静默失效**⇒ ⇒ MUST 断言「每个 deny 符号**仍存在于代码库某处**」） | **增强** |
| **R2'** | —（v1 无） | **新增**：「`include/` 下**声明** `namespace ch::hier_eq` 的文件 ⇒ MUST **恰好是** `include/hierarchy_equivalence.h`」（**内容维度** ⇒ 堵住「改名绕过」） | **新增**（补 R1 的能力缺口） |
| **R3** | 「公共头 MUST 确实存在」（防假绿） | 同左 **+ 明确形态**：MUST 是**断言存在**（`[ -f include/hierarchy_equivalence.h ]`）⇒ ⇒ **MUST NOT** 沿用旧脚本「不存在 ⇒ 说明没有 ⇒ 绿」的逻辑 | 语义**明确化** |
| **R4** | 公开 API 面 ⊆ 已裁决的 **5** 个公共头 | **拆成两条**：<br>① **历史窗口**（原 R4，base/head **钉死**在 T3-005 的两个 commit ⇒ ⇒ **逐字保留**，不改）<br>② **新增活账**：P8 之后新增的公开头 MUST 登记在 `.rddf/checks/public-api-surface.md` ⇒ ⇒ 护栏**双向核对**（登记了不存在 ⇒ 红 · 存在未登记 ⇒ 红） | **新增活账**（★ v1 的「4→5 白名单扩容」是**空操作** —— 原 R4 只扫 `git diff BASE..HEAD_REF`，P8 的文件**不在窗口内** ⇒ ⇒ 判别力零变化 ⇒ metis finding #2） |

⚠️ **CI 接线**（★ metis finding #2 的延伸 · **实测**）：`scripts/verify_hierarchical_p8.sh`
**当前不在 `.github/workflows/ci.yml`**（实测：CI 里只有 `verify_hierarchical_{tree_extract,rtl,port_naming}.sh`
三兄弟）⇒ ⇒ 现有 P8 守卫**零执行** ⇒ ⇒ 本 change **MUST** 把它接进 CI
⇒ ⇒ 否则「强度不减」的论证**只是一次性本地证据**。

⚠️ `--self-test` MUST 同步改（⇒ 逐条投毒仍必红 ⇒ 否则护栏失去判别力）⇒ ⇒ **投毒清单 MUST
写死**（见 §3.3 C2）。

### 2.5 文档

`docs/usage_guide/` 新增用户面小节：
- 显式 include（⚠️ **公共头不进 `ch.hpp` 聚合器** ⇒ ⇒ 必须显式 include ⇒ ⇒ 文档 MUST 说清；
  ★ 与 P6 的 `hierarchy_analysis.h` 同款既有惯例）
- **最小可跑示例**（★ MUST 由**真实测试源文件**承载 ⇒ 文档只引用它 ⇒ ⇒ 防文档腐烂
  —— metis finding：仓内无「文档片段被构建」的机制 ⇒ ⇒ `-fsyntax-only` 手工跑必然腐烂）
- **观测面 MUST 手写**（`Observation` 非空 + 至少一个**随激励变化**的信号）⇒ ⇒ 不得改成「自动猜全部输出」
- **激励 MUST 走 `set_input_value`**（`<<=` 会让顶层端口变成内部驱动 ⇒ 假分歧）
- **错误串 MUST 可行动**（★ metis：仓内有明确已知陷阱 —— `apt install verilator` 是**错的**；
  正确路径是 `cmake --build build --target verilator` ⇒ ⇒ 错误串 MUST 点名这个下一步）
- **Linux-only**（Verilator backend 的既成限制）· **头在 macOS/Windows 上仍可 `#include`**
  （与 `KNOWN LIMITATIONS` 对 `core/verilator_backend.h` 的既有要求同款）
- §1.3 的「与双后端 parity 互补」对照表
- 判别力边界：只判「层次化变换引入的分叉」；**两份产物同源 ⇒ 同构缺陷一起错**
- ⚠️ **「compare 的 A 侧固定是扁平产物」**（metis finding #1）⇒ ⇒ 文档 MUST 写明
  「A vs B（两个层次化粒度互比）**不**直接支持 ⇒ ⇒ 用**传递性**：A ≡ 扁平 ∧ B ≡ 扁平 ⇒ A ≡ B」

### 2.6 明确不做（Non-Goals）

- ✗ **不**自动推断观测面（保留「调用方手写」⇒ 猜 = 空洞判据 + 踩 ADR-058 §4 风险 6 盲区）
- ✗ **不**做「C++ 解释器 vs Verilog」的对比（那是既有 parity 的职责）
- ✗ **不**承诺「层次化产物语义正确」（只承诺「变换不改行为」）
- ✗ **不**改发射器 / 分析层 / backend（**零** `src/hierarchy_*.cpp` 行为改动）
- ✗ **不**承诺观测盲区里的信号（成层子树的 `ch_out` 在层次化顶层**不可观测**）
- ✗ **不**为「公开 API 面登记」机制立新 ADR（记 backlog，与 P6 同源）
- ✗ **不**支持「A vs B（两个层次化产物直接互比）」⇒ ⇒ 走**传递性**（见 §2.5）
- ✗ **不**提供 verilator 编译的**超时控制**（`invoke_verilator` 无此参数 ⇒ ⇒ 调用方自管）
- ✗ **不**承诺在**已仿真过**的 ctx 上可重入（`compare` 会新建 backend + 触发复位脉冲）
- ✗ **不**承诺 `ProductSet` 可**跨 ctx** 传递（`ports` 里的 `node_id` 是 **ctx 私有**编号）
- ✗ **不**提供「粒度全扫掠」的一站式入口（用户按 §2.5 的传递性自行组合）

---

## 3. Acceptance (验收判据 —— MUST 可勾选)

> ⚠️ 判据原则：**每条 MUST 可机械复算**（禁「看起来对」）。
> ⚠️ **每条 MUST 能被「用户少一个能力」触发**（★ metis #10：A2 的「双向比对」**不惩罚功能缺失**
> ⇒ ⇒ 功能判据 MUST **独立**存在）。

### 3.1 公共面

- [ ] **A1** `include/hierarchy_equivalence.h` **自包含**（`scripts/verify_header_self_contained.sh` PASS）
- [ ] **A1'** **ODR 安全**（`scripts/verify_header_odr.sh` PASS —— 公共头含 `inline` 函数 + `std::function`
      成员 ⇒ 多 TU 包含 MUST 无 ODR 违规）
- [ ] **A2** 公共头的**公开符号集**与 §2.2 的**闭合集合**（6 项）**双向比对**（多一个 ⇒ 红 ·
      少一个 ⇒ 红）⇒ ⇒ **判定口径 MUST 写死**（★ metis #12：按「本头**声明**的名字」算，
      **不是**按「传递闭包可达的名字」算 ⇒ ⇒ 否则 `ch::VerilatorPortSpec` / `ch::SignalMapEntry`
      会被算进来 ⇒ ⇒ 白名单数字继续漂移）
- [ ] **A2'** **能力判据**（独立于 A2）：§2.2 的 6 项**逐项**有可运行的调用点
      ⇒ ⇒ 少任一项 ⇒ **红**（★ 堵住「砍功能凑数」）
- [ ] **A3** 公共面**不得**出现内部实现符号（`ch_test::` · `impl::` · `emit_products` ·
      `compare_products` · `ensure_dir` · `read_whole_file`）⇒ ⇒ 护栏 R2 覆盖
- [ ] **A4** 公共头**不进** `ch.hpp` 聚合器 ⇒ ⇒ 文档 MUST 给**显式** include
- [ ] **A5** 公共头的**自我一致性**：**只** include 这**一个**头就够用全部 6 项
      ⇒ ⇒ MUST NOT 逼用户再手动 include `hierarchy_verilog.h` 才能写 `ProductSet`
- [ ] **A6** 公共头在 **macOS/Windows 上仍可 `#include`**（与 `KNOWN LIMITATIONS` 对
      `core/verilator_backend.h` 的既有要求同款）⇒ ⇒ **MUST NOT** 新增平台 `#error`

### 3.2 零回归

- [ ] **B1** `tests/test_hierarchy_equivalence.cpp` 的**断言集合与期望值不变**
      （⚠️ v2 放宽：允许 `trace` → `flat_trace` 的**机械改名** ⇒ ⇒ 判据 = 断言**条数**不变
      （414）且**期望值**逐条不变 ⇒ ⇒ `git diff` MUST 只含该改名）
- [ ] **B2** 仓内**其它** `ch_test::hier::*` 调用点**零改动** ⇒ ⇒ `grep` 可查
- [ ] **B3** `ctest -LE perf` 全绿 · samples 全绿 · examples 全绿 · 计数锚点同步
      （`CTEST_TOTAL` +1 · non-perf +1 · `VERILATOR_E2E_TOTAL` +1 · ⚠️ parity 两锚点**不变**）

### 3.3 护栏

- [ ] **C1** `scripts/verify_hierarchical_p8.sh` 改口径后 **PASS**（R1–R4 + R2' + 活账）
- [ ] **C2** `--self-test` 逐条投毒**仍必红**，且**投毒清单写死**（★ metis #7）：
      ① 内部实现符号泄漏到公共面（→ R2）
      ② 公共头**改名**（→ R1：文件名不再匹配豁免）
      ③ 公共头**缺失**（→ R3：**断言存在** ⇒ 必须真的非零退出）
      ④ **多出第 2 个公共头，且名字**不**命中 `*equivalence*` / `*hier_eq*`**
      （如 `include/hierarchy_compare_public.h`）⇒ ⇒ **必须红**
      （★ v1 的 ④ 写成「多出第 2 个头」但没写死名字 ⇒ 若自检用的名字恰好命中 `*equivalence*`
      ⇒ ⇒ 红的是**旧**判据 ⇒ ⇒ **证明不了白名单机制** ⇒ ⇒ metis finding #7）
      ⑤ **新增**：公共头 `#include` 了内部实现头 / `src/` 里的头 ⇒ ⇒ **必须红**
      （★ **R2 的盲区** —— 符号名检查看不见 `#include` 图 ⇒ ⇒ 这是**最可能的真实泄漏通道**）
- [ ] **C3** 护栏正文含「**全称否定 + 单一豁免 ⇒ 新判据 ⊇ 旧判据**」的显式说明
      （⇒ 后人不会误读为「放松」）
- [ ] **C4** 护栏**接进 CI**（`.github/workflows/ci.yml`）⇒ ⇒ **实测**：CI 里出现该脚本
      （★ 现状：**零执行** ⇒ ⇒ 不接则 C1 只是本地一次性证据）

### 3.4 公共 API 的**可用性**（用户视角）

- [ ] **D1** 一个**独立 out-of-tree 子工程**（只 `-I<repo>/include` + 链接 `libcpphdl`）
      用公共 API 复现「层次化 ≡ 扁平」的结论 ⇒ ⇒ **真编译 + 真链接 + 真运行**
      （★ metis #3：`-fsyntax-only` 只证明语法 ⇒ ⇒ 不证明链接 / 不证明默认参数可用 / 不证明能跑；
      ★ 仓内测试能拿到的 include 路径与仓外不同 ⇒ ⇒ 仓内测试**证明不了**「公共面够用」）
- [ ] **D2** 故意破坏层次化产物 ⇒ 公共 API 的 `compare` **转红**（`equal=false` + `first_mismatch`
      非空）⇒ ⇒ 证明公共入口**有判别力**（不是恒绿）
- [ ] **D3** verilator **不可用**时 ⇒ `compare` 返回 `ok=false` + 明确原因（**fail-closed**）
      ⇒ ⇒ **MUST NOT** 静默返回「相等」
- [ ] **D4** 文档示例**由真实测试源文件承载** ⇒ ⇒ 该源文件被 CI 编译并运行
      ⇒ ⇒ 文档只**引用**它（★ 防文档腐烂）
- [ ] **D5** **空输入 fail-closed**（★ metis finding：现有实现空 `stimulus` ⇒ 循环不执行 ⇒
      `first_mismatch` 为空 ⇒ `equal=true` ⇒ **恒绿**）：
      - 空 `stimulus` ⇒ `ok=false` + 点名
      - 空 `observations` ⇒ `ok=false` + 点名
      - `observations` 非空但**无任何信号随激励变化** ⇒ `ok=false` + 点名
        （★ 否则「逐拍全等」在恒相关信号上**恒绿** ⇒ ⇒ 对应台账登记的判别力上限）
- [ ] **D6** **错误串可行动**：`error` MUST 以**稳定前缀**开头（机器可分类）⇒ ⇒ 枚举至少含
      `verilator-unavailable` / `emit-failed` / `port-set-empty` / `port-width-64-exceeded` /
      `backend-init-failed` / `empty-stimulus` / `empty-observations` / `no-varying-signal`
      ⇒ ⇒ 且 `verilator-unavailable` 的正文 MUST 含**正确构建命令**
      （★ `cmake --build build --target verilator` ⇒ ⇒ **MUST NOT** 建议 `apt install verilator`）
- [ ] **D7** **>64bit 端口** ⇒ MUST 在 `emit` 阶段 **fail-closed** 并**点名端口 + 位宽**
      （★ `VerilatorPortSpec` 已有 `bitwidth` ⇒ ⇒ 可判 ⇒ ⇒ 现状是错误信息不含端口名）
- [ ] **D8** **DUT 完全没有顶层 `ch_out`** ⇒ MUST fail-closed 并**点名成因**
      （★ 层次化侧内部信号**不可观测** ⇒ ⇒ 必然红且原因不明）

### 3.5 判别力自证

- [ ] **E1** 变异（每条**先确认仍能编译** ⇒ 编译失败 ⇒ 变异**无效** ⇒ 如实登记），
      且**判据归属 MUST 写对**（★ metis #7）：
      ① 公共面**多**一个符号 ⇒ A2 红
      ② `emit` 的端口集合改成**手写** ⇒ 断言 MUST 落在「`ports` 与 `backend_port_set(emitted,"top")`
         **逐元素相等**」⇒ ⇒ **MUST NOT** 挂在 D2 上（★ 对固定 fixture 手写集合极易**碰巧相等**
         ⇒ ⇒ 挂在 D2 上 ⇒ 该变异**不改变可观测行为** ⇒ ⇒ 无效变异）
      ③ 激励改用 `<<=` ⇒ 判据归属 = **初始化侧 fail-closed**（`ok=false`），
         **不是** `equal=false`（★ 现状：`<<=` 让端口变内部驱动 ⇒ 初始化失败而非假分歧）
      ④ **新增**：人为让两侧激励不同 ⇒ `equal=false` + `first_mismatch` 非空
         ⇒ ⇒ 证明**比较侧**的判别力（③ 只证明初始化侧）
      ⇒ ⇒ 每条还原后 MUST **逐字节比对 + 重建 + 复绿**

### 3.6 边界（**MUST 写进文档**，否则用户会误用）

- [ ] **F1** 文档明说：与双后端 parity **互补不可替代**（§1.3 对照表）
- [ ] **F2** 文档明说：只判「层次化变换引入的分叉」；**两份产物同源 ⇒ 同构缺陷一起错**
- [ ] **F3** 文档明说：观测面 MUST 手写（不得自动猜）；观测盲区（成层子树 `ch_out` 不可观测）
- [ ] **F4** 文档明说：Linux-only（Verilator backend 的既成限制）
- [ ] **F5** 文档明说：**compare 的 A 侧固定是扁平产物** ⇒ ⇒ 「A vs B」走**传递性**
- [ ] **F6** 文档明说：`ProductSet` **与其 ctx 绑定**（`node_id` 是 ctx 私有编号 ⇒ 不可跨 ctx 传递）
- [ ] **F7** 文档明说：**不提供 verilator 编译超时控制**（调用方自管）· **不承诺可重入**

---

## 4. Capabilities (契约影响)

| capability | delta |
|---|---|
| `hierarchical-equivalence-public-helper` | **ADDED ×N**（新建 ⇒ 归档**禁** `--skip-specs`） |

⚠️ **规范所有者边界**：产物契约（层次化产物的结构 / 端口命名 / 等价性判据本体）归
`rtl-hierarchical-output` ⇒ ⇒ 本 capability **只**管「**公共 helper 的用户面契约**」
（入口形状 · fail-closed 语义 · 调用方义务 · 判别力边界声明）⇒ ⇒ **MUST NOT** 复述产物契约
⇒ ⇒ 两份规范并存必然漂移。

⚠️ 与既有 capability 的关系（**须在 delta 正文点名**，避免读者误以为要跑两遍）：

| 既有 capability | 本 capability 的关系 |
|---|---|
| `rtl-hierarchical-output` | 本 helper 是它的**使用入口**（不重复它的产物契约） |
| `hierarchy-friendliness-diagnosis`（P6） | **互补**：诊断说「能不能改」· 本 helper 说「改了会不会变」 |

**建议的 delta Requirement 条目**（实施期定稿 ⇒ 数量以 design.md 为准）：

1. 公共 helper 的入口 MUST 存在且公共面 MUST 是**闭合集合**（★ 逐符号登记 + 双向核对）
2. helper MUST **fail-closed**（★ 空输入 / verilator 不可用 / >64bit 端口 / 无顶层输出
   ⇒ ⇒ 明确错误 + **稳定前缀** + **可行动的下一步** ⇒ ⇒ **MUST NOT** 静默通过）
3. helper MUST 声明其**判别力边界**（★ 只判变换引入的分叉 · 两产物同源 ⇒ 同构缺陷一起错 ·
   观测面 MUST 手写 · 观测盲区）
4. helper MUST **不替代**双后端 parity（★ 二者互补 ⇒ MUST NOT 被读成「一个就够」）

---

## 5. Impact (影响面)

| 面 | 影响 | 风险 |
|---|---|---|
| **公开 API 面** | **+1 个公共头** · 6 个公开符号 | ⚠️ **语义承诺 + 兼容负担** ⇒ ⇒ 触发 **critic**（2 轮预算） |
| **兼容承诺边界**（v2 新增 · metis #2 的延伸） | 公开即冻结：① 字段集 / 语义 / 默认参数 ② **只承诺源码兼容**（header-only + `inline` ⇒ **无 ABI 承诺**）③ 加字段 = additive（合法）· 改字段名 / 删字段 / 改 `error` 语义 = breaking ④ `Granularity` 默认值改动 = breaking | ⚠️ MUST 写进 delta 正文 ⇒ 否则后人会以为还能改 |
| **护栏** | `scripts/verify_hierarchical_p8.sh` 改口径（R1 全称化 + R2 双向 + **R2' 新增** + R3 明确化 + **活账**）+ **接进 CI**（★ 现状零执行） | ⚠️ 护栏本身是本 change 的**唯一**回归栅栏 ⇒ ⇒ MUST CI 化 |
| **测试** | `tests/hier_equivalence.h` 变 alias · `tests/test_hierarchy_equivalence.cpp` 一处机械改名 | 低（结构性零回归） |
| **CI** | 新增 1 个 ctest 条目（跑真 verilator ⇒ Linux-only ⇒ CI 矩阵已是 Ubuntu-only ⇒ ✅ 一致） | 中（CI 时间 +1 个 verilator 编译） |
| **文档** | `docs/usage_guide/` 新增用户面小节（示例**由真实测试源文件承载**） | 低 |
| **计数锚点** | `CTEST_TOTAL` +1 · non-perf +1 · `VERILATOR_E2E_TOTAL` +1 · parity 两锚点不变 | 低（机械同步） |
| **发射器 / 分析层 / backend** | **零行为改动** | 无 |

⚠️ **范围外历史遗留（只记录不清理）**：`include/` 下公共头的**聚合器归属**不统一（部分不在 `ch.hpp` 里）
⇒ ⇒ **既有状态** ⇒ ⇒ 本 change **只**保证新公共头与既有惯例一致（不进聚合器），
**MUST NOT** 顺手统一其它头。

---

## 6. metis 第 1 轮 gap 分析的处置（逐条）

> metis 第 1 轮（2026-11-07）· **本 improvement 总计 1 轮**（预算用尽）。
> ⚠️ metis 输出的 4 条**事实断言**已**逐条实测复核**（不凭 subagent 结论直接采信）：

| metis 断言 | 实测复核 | 结论 |
|---|---|---|
| `verify_hierarchical_p8.sh` **不在 CI** | `grep -rn "hierarchical_p8" .github/workflows/` ⇒ **零命中**（CI 里只有 `tree_extract` / `rtl` / `port_naming` 三兄弟） | ✅ **成立** ⇒ ⇒ 采纳（§2.4 CI 接线 + C4） |
| `Observation.node_id` 在 impl 里**从未被读** | `grep -n node_id tests/hier_equivalence.h` ⇒ 只有**声明**（:55）+ 注释（:48）⇒ 读取只出现在 `tests/test_hierarchy_equivalence.cpp:204/212`（仓外对账） | ✅ **成立** ⇒ ⇒ **部分采纳**（保留字段但**移入 helper 的对账** + 提供工厂 ⇒ 不让用户手填 `impl()`）⇒ ⇒ 未采纳「直接砍掉」（砍掉则 delta Scenario 失去机械载体） |
| `trace` **只存扁平侧** | `tests/hier_equivalence.h:190` = `row.push_back(vf)` ⇒ 层次化侧 `vh` 只参与比较、**不入轨迹** | ✅ **成立** ⇒ ⇒ 采纳（改名 `flat_trace` + 取消 `Trace` 别名） |
| R4 的窗口**钉死**在 T3-005 ⇒ 「4→5 白名单扩容」是**空操作** | `scripts/verify_hierarchical_p8.sh:39-53` 钉死 base/head；R4 只扫 `git diff BASE..HEAD_REF -- include/` | ✅ **成立** ⇒ ⇒ 采纳（拆成「历史窗口保留 + 活账新增」） |

| # | metis finding | 处置 |
|---|---|---|
| **M1** | **P0 · 隐藏意图**：用户真实动作是「粒度 A vs 粒度 B」，而 API 把 A 侧**硬编码**成扁平 ⇒ 粒度互比**无处安放** | ⚠️ **部分采纳**：核实 `compare_products` 的 A 侧 = 「backend 自己生成的扁平产物」⇒ ⇒ **A vs B 走传递性**（A ≡ 扁平 ∧ B ≡ 扁平 ⇒ A ≡ B）⇒ ⇒ 不改签名 ⇒ ⇒ 但**必须写进文档**（F5）+ **列入 Non-Goals**（§2.6）⇒ ⇒ 不把「粒度全扫掠」当本 change 的义务（超出台账 §7 残留 1 的登记范围） |
| **M2** | **P0 · R1 从全称变存在性 ⇒ 强度损失** | ✅ **采纳** ⇒ v2 改为「全称否定 + 单一字面豁免」+ 新增 **R2'**（内容维度）⇒ ⇒ 新旧判据成**超集**关系（C3） |
| **M3** | **P0 · R4 的「4→5 扩容」是空操作** | ✅ **采纳** ⇒ 拆成「历史窗口原样保留 + 新增活账 `.rddf/checks/public-api-surface.md` 双向核对」 |
| **M4** | **P0 · D1 只证明「能用」⇒ 需最短路径 + 真外部 TU + CI 执行** | ✅ **采纳** ⇒ D1 改为「**out-of-tree 子工程**（只 `-I include` + 链接 `libcpphdl`）+ 真编译 + 真链接 + 真运行」 |
| **M5** | **P1 · 空 `observations` / 空 `stimulus` ⇒ 恒绿** | ✅ **采纳** ⇒ 新增 **D5**（3 条 fail-closed 判据） |
| **M6** | **P1 · 错误串 MUST 可行动 + 稳定前缀**（★ 仓内已知陷阱：`apt install verilator` 是错的） | ✅ **采纳** ⇒ 新增 **D6**（稳定前缀枚举 + 正确构建命令） |
| **M7** | **P1 · C2 ④ 投毒没写死名字 ⇒ 证明不了白名单机制**；缺「`#include` 图」投毒 | ✅ **采纳** ⇒ C2 写死 5 条投毒（④ 用**不命中**模式的名字 · ⑤ 新增 `#include` 图投毒） |
| **M8** | **P1 · E1 ② / ③ 的判据归属写错**（② 手写集合易**碰巧相等** ⇒ 无效变异；③ `<<=` 致 `ok=false` 而非 `equal=false`） | ✅ **采纳** ⇒ E1 ② 断言落在「逐元素相等」· ③ 归属改为「初始化侧 fail-closed」· **新增 ④**「人为两侧激励不同 ⇒ 证明**比较侧**」 |
| **M9** | **P1 · `node_id` 是无回报的承诺**（只服务仓内测试 + 要求用户写 `impl()`） | ✅ **部分采纳**（见上表「实测复核」行）⇒ ⇒ **保留字段**（它是 delta Scenario 的机械载体）· **移入 helper 的对账** · **提供工厂** ⇒ ⇒ 用户侧不出现 `impl()` |
| **M10** | **P1 · `trace` 暴露 = 永久冻结嵌套容器 + 成功路径也付拷贝代价** | ⚠️ **部分采纳**：接受「冻结 `std::vector<std::vector<uint64_t>>`」（v1 API · 诊断字段 · 量级小）⇒ ⇒ **改名** `flat_trace`（语义修正）⇒ ⇒ **不**引入 `Options{collect_trace}`（★ 那会**扩大**公共面 ⇒ 与「最小承诺」相悖）⇒ ⇒ 记为已知取舍 |
| **M11** | **P1 · `Trace` 单侧 ⇒ 名字误导** | ✅ **采纳** ⇒ 改名 `flat_trace` + **取消 `Trace` 别名**（⇒ 公开集合恰为 6 项） |
| **M12** | **P1 · 补 4 条机械 AC**：ODR / 空输入 fail-closed / 错误码前缀 / macOS-Windows 可 include | ✅ **全采纳** ⇒ A1' / D5 / D6 / A6 |
| **M13** | **P1 · R2 deny-list 改名后判据静默失效** ⇒ 需双向核对 | ✅ **采纳** ⇒ R2 加「每个 deny 符号**仍存在于代码库某处**」的反向检查 |
| **M14** | **P2 · 命名空间写死**（建议 `ch::hier_eq`） | ✅ **采纳** ⇒ §2.1 写死 + 「头名 ≠ 命名空间」写进文档 |
| **M15** | **P2 · 删掉「共 6 个」这个代理指标** ⇒ 改「闭合集合」 | ✅ **采纳** ⇒ §2.2 + A2（**不按总数核**）+ **A2'**（能力判据独立存在） |
| **M16** | **P2 · B1 加脚注**（字段名被零改动要求**锁死** = 有意的兼容承诺） | ✅ **采纳** ⇒ §2.2 ② 的 ⚠️ + B1 放宽措辞 |
| **M17** | **P2 · 补 3 条 Non-Goal**（verilator 超时 / 可重入 / 跨 ctx） | ✅ **采纳** ⇒ §2.6 |
| **M18** | **P1 · 命名空间「头名 ≠ 命名空间」+ `include/` 惯例** | ✅ **采纳** ⇒ §2.1 + A4 |
| **M19** | **P1 · `Observation.node_id` 要么砍要么给工厂** | ✅ **采纳** ⇒ 给工厂（见 M9） |
| **M20** | **P2 · A2 的「公开符号」口径未定义**（声明的名字 vs 传递闭包可达） | ✅ **采纳** ⇒ A2 写死「按**声明**的名字」 |
| **M21** | **P0 · §2.4 R2' 内容维度 + R4 换登记表 + CI 接线** | ✅ **采纳** ⇒ §2.4 + C4 |
| **M22** | **P1 · 文档示例防腐烂 ⇒ 示例由真实测试源文件承载** | ✅ **采纳** ⇒ D4 + §2.5 |
| **M23** | **边界 · 组合逻辑 DUT / 单时钟沿 ⇒ 判别力弱** | ✅ **采纳** ⇒ 并入 D5 第 3 条（「无任何信号随激励变化 ⇒ fail-closed」）+ F3 |

---

## 7. 实施期发现（如实登记）

（实施期追加 ⇒ ★ 只增不改本文件的决策内容。）
