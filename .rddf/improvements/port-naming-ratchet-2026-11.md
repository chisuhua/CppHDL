---
name: port-naming-ratchet-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: implemented
进度: N1–N6 全部落地（B0 ⑥ + B1 · 2026-11-11 验收 APPROVE）· N5 第 2 步 = **独立立项**（不在本项）
主题: Verilog codegen 完整化
依赖: rtl-emit-product-tooling-2026-11（manifest）· public-api-remove-visibility-2026-11（公开面登记机制）· rtl-hierarchical-output（命名规则规范所有者）
来源: 用户 2026-11-10 拍板「⑤ 生成的 RTL 顶层端口，希望保持和 C++ 侧一样的命名」+ planner 实测规模盘点
生成时间: 2026-11-10
evidence: test:tests/test_port_name_named_ctor.cpp
---

# Improvement: 端口**显式命名**（框架先行 + 基线棘轮 · 不做一次性 3103 处 codemod）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **用户裁决（2026-11-10）**: 「#5 按你建议」⇒ ⇒ **方案 A′（框架先行 + 基线棘轮）**——**不**做一次性全量 codemod
- **归属**: 本项只管**端口身份**（名字）；端口**选择**（哪些算顶层）归 `rtl-emit-product-tooling-2026-11`

---

**主题**: Verilog codegen 完整化

## 架构依据

### A.1 根因（实测）

| # | 事实 | 证据 |
|---|---|---|
| ① | `ch_in` / `ch_out` 的**端口名**（`port::name()`）**硬编码**为 `"unnamed_input"` / `"unnamed_output"` ⇒ ⇒ 产物端口名只可能是 `unnamed_input` + 去重序号<br>⚠️ **metis 订正**：产物里的 `top_` 是 **IR 节点名的路径前缀**（组件全路径 + `.` + 创建名），**不是**端口名的一部分 ⇒ ⇒ 「C++ 名 == RTL 名」到底指哪一级名字，**必须先裁决**（见 N5） | `include/core/io.h:143` · `src/core/lnodeimpl.cpp:30-35` |
| ② | ⚠️ **metis 订正**：`port` / `ch_logic_in` **已有具名构造**（`explicit port(const std::string&, sloc)`，`include/core/io.h:221,266`；`ch_logic_in` 同型 `:180-198`）⇒ ⇒ 缺的**不是**「构造」，而是「**`__io` 内的默认成员声明怎么带名**」这一**用法约定 + 护栏**<br>⇒ ⇒ 原稿 N1「新增带名构造」**作废**<br>⚠️ **planner 复核订正（2026-11-10）**：原稿称「仓内已有 **20+ 调用点**」⇒ ⇒ **实测为 0**（全仓扫 `.h/.cpp`，正则 `(ch_in\|ch_out\|port\|ch_logic_in\|ch_logic_out)\s*<…>\s*\{?\s*"` ⇒ 0 个真调用点）⇒ ⇒ **构造存在但从未被任何代码走过** ⇒ ⇒ 正确的事实**更强**：本项缺的不只是「用法约定 + 护栏」，还缺「**这条路径的第一份可运行样例**」⇒ ⇒ N1② 的「最小可运行探针」**从「建议」升格为 MUST**（否则「显式名 → 产物端口名」这条链**从未被端到端验证过**） | `include/core/io.h:221,266,319-330` · 调用点计数见 §事实订正 F2 |
| ③ | `name()` **只读** ⇒ ⇒ 没有事后改名的 API | `include/core/io.h:250` / `:289` |
| ④ | 去重计数器按 **eval-list 顺序** ⇒ ⇒ **图的任何无关改动都会改名**（`_3` → `_4`）⇒ ⇒ 端口名**不可依赖** | `include/codegen_naming.h::build_verilog_name_table`（`counts[base]++` 按 `ctx->get_eval_list()` 拓扑序） |
| ⑤ | **规模**：端口声明 **3103** 处（`ch_in<` **1577** + `ch_out<` **1526**）· 含 `__io(` 的文件 **168** 个 ⇒ ⇒ 一次性 codemod = 全仓级改动<br>⚠️ **planner 复核订正（2026-11-10）**：原稿写 **2769**（1420 + 1349）⇒ ⇒ **该数字不可复现**：按本行给出的命令实测得 **3103**（1577 + 1526）⇒ ⇒ 各种路径组合都到不了 2769（`include/` 1215 · `+examples/` 1593 · `+scripts/` 3208 · `+src/` 3104）⇒ ⇒ 即使按「声明形态」（`ch_in<T> ident;`）重数也是 **2888**（183 文件）⇒ ⇒ **原数字作废**，本行数字为唯一权威 | `grep -rhoE '\bch_in<\|ch_out<' include/ examples/ tests/ \| wc -l`（2026-11-10 实测 = **3103**） |
| ⑥ | 框架**已有**「节点名唯一性 opt-in 策略」⇒ ⇒ 「重名直接报错」**表面上**可复用<br>⚠️ **metis 订正**：`kRejectDuplicates` 判的是 **ctx 内全部节点**，且 `ch_reg` 的默认名（`reg`）会让**普通设计必然报错**（`include/core/context.h:168-176` 文档自陈）⇒ ⇒ 它**不可能**用于产品设计 ⇒ ⇒ 端口重名 MUST 用**端口作用域**的判定（「同一组件内已存在同名显式名」）⇒ ⇒ 这是**新增**的局部判定，**不是**复用 ⇒ ⇒ 原稿「MUST NOT 另造重名机制」**作废** | capability `core-ctx-node-name-uniqueness` · `include/core/context.h:168-176` |
| ⑦ | 命名规则的**规范所有者 = `rtl-hierarchical-output`**；guard capability MUST NOT 复述规则 | capability `hierarchical-port-naming-guard`（Purpose 段明文裁定） |
| ⑧ | 扁平与层次化**共用**同一份命名实现 ⇒ ⇒ 改一处即改两处（**不会漂**） | `include/codegen_naming.h` 被**三个** .cpp 共用（**实测 2026-11-11**）：`src/codegen_verilog.cpp:3`（include）· `src/hierarchy_analysis.cpp:14`（include）· `src/hierarchy_verilog.cpp:14`（include）⇒ ⇒ ⚠️ **原依据写 `src/codegen_verilog.cpp:69` 有误** —— `:69` 是**调用点**（`view.names = ch::build_verilog_name_table(...)`）**不是** include 点 ⇒ ⇒ ⇒ 改为上述 **3 个 include 点**⇒ ⇒ ⇒ **第三处 `hierarchy_verilog.cpp` 原稿漏列** ⇒ ⇒ ⇒ 结论**不变**（改一处即改两处 ⇒ ⇒ 不会漂）但**覆盖面更全** |

### A.2 为什么不做一次性全量 codemod

- **爆炸半径**：3103 处声明 / 168 文件 ⇒ ⇒ 一次 commit 改完 ⇒ review 不可行 ⇒ ⇒ 缺陷必然漏。
- **回归风险集中**：任何一处漏改 ⇒ 端口名静默变化 ⇒ ⇒ 而端口名变化会让**跨仓下游**的 TB 静默接错（不是编译错，是行为错）。
- **收益不需要等**：用户的目标是「RTL 侧端口名 = C++ 侧成员名」⇒ ⇒ **新写/改动**的组件带名即可 ⇒ ⇒ 存量可**增量**消化 ⇒ ⇒ 棘轮是正解。

### A.3 现有命名基线可复用的机制（避免新造）

| 既有机制 | 用途 |
|---|---|
| `core-ctx-node-name-uniqueness`（`kRejectDuplicates`） | 端口重名 ⇒ **创建点 fail-fast** ⇒ ⇒ 直接复用，**不**新造「重名报错」机制 |
| `scripts/lib/header_self_contained_exempt.txt`（豁免表模式） | 「存量豁免 + 新增必守」的**基线棘轮**样板 ⇒ ⇒ 端口命名棘轮照此模式做 |
| `public-api-remove-visibility-2026-11`（已实施） | 公开 API **移除/改名**的登记 + CHANGELOG 载体 ⇒ ⇒ 本项改 `ch_in`/`ch_out` 构造 MUST 复用 |
| `.rddf/checks/public-api-surface.md` | 公开符号面登记 ⇒ ⇒ `ch_in`/`ch_out` 构造签名变更 MUST 登记 |

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **N1** | ⚠️ **metis 订正后的收缩**：**具名构造已存在**（`include/core/io.h:221,266`）⇒ ⇒ 本项的交付物**不是**「新增构造」，而是：① **用法约定**（`__io` 内的成员如何带名 —— 形如 `ch_in<ch_bool> prog_we{"prog_we"};`）② **最小可运行探针**（`__io` 内 NSDMI 经 `new (io_storage_) io_type` 是否生效 —— **本轮静态推断，未运行验证**）③ **一条 ctest** 钉住该用法 ④ **文档**（用法 + 已知限制）<br>⚠️ **planner 复核订正（2026-11-10）**：具名构造的**调用点实测为 0** ⇒ ⇒ 这条路径**从未被端到端跑过** ⇒ ⇒ ② **从「探针」升格为 MUST**（MUST 实跑一次并把产物端口名钉进断言）⇒ ⇒ ② 的失败形态（构造在 `__io` 内不生效 / 生效但名字没进图）**必须先被排除**，否则后面 ③④ 全是空中楼阁 |
| **N2** | **基线棘轮**：登记表 `.rddf/checks/port-naming-registry.md` + 护栏 `scripts/verify_port_naming.sh`：① 扫描面限定 **product 侧**（`include/**` + `examples/**`）⇒ ⇒ `tests/**` **默认排除**（沿用既有裁决 `scripts/verify_input_port_additions.sh`「测试 DUT 端口自包含 ⇒ 不属风险面」）② 判定粒度 = **端口级增量**（`git diff <base>...HEAD` 只看**新增的端口声明**）③ 登记表条目身份 = `<文件>\t<__io 块序号>\t<声明文本>`（**不是文件哈希** —— 哈希对任何注释改动都失效）④ 计数锚点 `EXPECTED_EXEMPT_PORTS=<n>` **钉在护栏脚本里**（照 `scripts/verify_public_api_visibility.sh` 的 `EXPECTED_PROBES=24`），**不**钉在可编辑的登记表里 ⑤ 双向 + **逐端口**（豁免文件里**新增**未命名端口 MUST 红 —— 文件级集合比对堵不住）⑥ `--self-test` 至少投毒 **2 类**（已命名组件里加未命名端口 / 豁免文件里加未命名端口）<br>⑦ 🔴 **oracle 加固 ①（2026-11-10 · 用户已点头）**：护栏 P1 MUST **额外**判「**标识符 == 显式名字符串**」⇒ ⇒ 即 `ch_in<ch_uint<32>> prog_addr{"prog_addr"};` **通过**（标识符 `prog_addr` == 名字串 `"prog_addr"`）⇒ ⇒ 而 `ch_in<ch_uint<32>> prog_addr{"port_addr"};` **MUST 红**（标识符 ≠ 名字串）⇒ ⇒ ⇒ **理由（oracle 根因 F-C）**：框架**无反射** ⇒ ⇒ 运行期唯一存活的是**显式命名串** ⇒ ⇒ manifest 的 `cpp_member` **只能是它** ⇒ ⇒ ⇒ 「可解析到 C++ 成员名」**必须**由这条静态护栏兜住 ⇒ ⇒ ⇒ 否则 AC 是**不可验证的 MUST** ⇒ ⇒ ⇒ **零新增成本**（护栏 P1 的扫描面**已经**拿到声明文本 ⇒ ⇒ 加一条 `ident == "ident"` 的比对即可） |
| **N3** | **名字稳定性**：显式名路径 MUST **绕开** eval-list 序计数器（否则显式名也会被追加序号）⇒ ⇒ 端口名对图的无关改动**稳定**。<br>⚠️ **metis 订正**：① 「重名 MUST 报错」**不能**复用 `kRejectDuplicates`（见 A.1⑥）⇒ ⇒ MUST 是**端口作用域**的局部判定 ② 「零序号」**不是**靠「名字不含 `_N`」判定（合法成员名本身可以以 `_2` 结尾 ⇒ 不可判）⇒ ⇒ 只能用**期望字符串比对**（如 `find("input ... top_prog_we")` 命中 且 `find("top_unnamed_input") == npos`） |
| **N4** | **命名规则进规范**：端口命名规则 MUST 写进 `rtl-hierarchical-output`（**规范所有者**）⇒ ⇒ 本项 MUST NOT 在别处另写一份<br>⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-4 修复**：本行的**条款正文**已移出 ⇒ ⇒ 改为**指针** ⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ ⇒ 「兼容去重路径 + 棘轮豁免表登记」这条**机制义务**在 R1-a 的「未显式命名者走兼容去重路径」Scenario 里也有 ⇒ ⇒ ⇒ **三处落点收敛为一处** ⇒ ⇒ 本行 MUST NOT 再复述条款（否则又造一份副本 ⇒ ⇒ 漂移）<br>✅ **2026-11-10 · oracle 建议 1 裁决 = 方案 A（用户已点头）· 本行已回填**：Requirement 收紧为「RTL 端口名 MUST **可机器解析到 C++ 成员名**（manifest `cpp_member`）· **MUST NOT** 依赖「RTL 名 == C++ 名」这一未成立的假设」（⚠️ **2026-11-11 · critic 第 5 轮 BLOCKER-2 修复**：此处原为**逐字条款引文** ⇒ ⇒ ⇒ **已改为转述** ⇒ ⇒ ⇒ 条款原文只在 R1-a 一处 ⇒ ⇒ ⇒ ⚠️ **措辞以 R1-a 为准**（R1-a 的标题已改「顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定」））⇒ ⇒ ⚠️ **删除**原措辞「顶层端口名 MUST 与 C++ 侧显式命名**逐字相同**」⇒ ⇒ 理由（oracle）：① 「逐字相同」对 T1⊖T2 的多数端口**结构上不可满足**（唯一性要求 + `sanitize` 非单射）② 扁平产物是**单 module 全局命名空间** ⇒ ⇒ 前缀/路径是**唯一消歧手段**③ 用户诉求的**真实缺陷是「名字无语义且不稳定」** ⇒ ⇒ 「显式命名 + 零序号 + manifest 可解析」已对**所有组件**解决它 ⇒ ⇒ 剩下的只是**人眼友好**<br>⇒ ⇒ ⚠️ **「字面同名」的正确归宿** = `rtl-hierarchical-output` 内一条**带前提的 SHOULD**（限定 `is_top_io == true` 视图）+ 一条「**不可能条件** MUST NOT 被承诺」⇒ ⇒ ⇒ **用户诉求不丢**（写进规范）⇒ ⇒ **不制造一条常态违约的 MUST**<br>⇒ ⇒ ⚠️ **集合口径（oracle 建议 2）**：**MUST 覆盖 T1**（顶层 module 端口表**全体**）· **SHOULD 限定 T2**（`is_top_io == true`）⇒ ⇒ 理由：`is_top_io` 的价值恰在区分 T1 的两半；若 MUST 只覆盖 T2 ⇒ ⇒ T1 里那 80 个 inline 后代输出**落到无命名义务的缝里** ⇒ ⇒ 覆盖面漏洞<br>⇒ ⇒ ⚠️ **字段语义只引用** `rtl-emit-portability`（manifest schema 所有者）⇒ ⇒ 本项 MUST NOT 重定义 |
| **N5** | 🔵 **已裁决（2026-11-10 用户）＝ 两步走**：<br>**第 1 步（立刻做）(a)** —— **只做 manifest 双字段**：`name`（RTL 全名，保持 `top_prog_we`）+ `cpp_member`（C++ 成员路径 `prog_we`）+ 复用 I1 的 `owner` / `is_top_io`。⇒ **零产物变更 · 零 ADR 变更 · 零快照漂移** ⇒ 工具链**立刻可用**（下游生成 TB 时能查到 C++ 侧名字）。<br>**第 2 步（独立立项）(c)** —— 兑现**字面同名**：只验**一件事** —— 把 `ch_device` 的根实例名清空（`include/device.h:28` 的 `"top"` → 空）后，`lnodeimpl` 的 `parent == nullptr` 分支（`src/core/lnodeimpl.cpp:33-35`）是否产出**裸 `prog_addr`**；<br>⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 SHOULD_FIX-7 修复**：原回退条件（「若产出的是 `.prog_addr` → `_prog_addr` ⇒ (c) 也不成立」）**已被探针实测证伪** ⇒ ⇒ **实测 = `unnamed_prog_addr`**（**第三形态**：既非裸 `prog_addr` 也非 `_prog_addr`）⇒ ⇒ ⇒ ⇒ **route (c) 在当前命名机制下不可实现**（清空实例名**不足以**产出裸名 ⇒ ⇒ 根因是 `"unnamed"` 哨兵被拼进节点名）⇒ ⇒ ⇒ ⇒ ⇒ **独立立项的前置条件 = 引入新的命名机制**（不是清空实例名）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **本步当前不做** ⇒ ⇒ ⇒ ⇒ 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读（与台账行 21 一致）⇒ ⇒ ⇒ ⇒ ⇒ 详见 `port-name-literal-parity` A.1①② + 批次表 §4 的同名行。<br>⚠️ **第 2 步 MUST NOT** 直接改 `build_verilog_name_table`（其签名 `(eval_order)` **无 root 入参** ⇒ 只能硬编码 `"top_"` ⇒ 对 `ch_module` 作顶层静默失效；且「先计数再剥」**不保证唯一**：`top_prog_we`→`prog_we` 与一个 `parent==nullptr` 的 `prog_we` 剥后同名而计数器不补） |
| **N6** | **公开面登记**：第 1 步**零公开 API 变更**（只加 manifest 字段）⇒ 无需登记；第 2 步若成立 ⇒ `Component::name()` 语义变更 ⇒ MUST 走 `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` + ADR 修订 |

### ❌ 不在本项

- **端口选择语义**（哪些端口算顶层）⇒ 归 `rtl-emit-product-tooling-2026-11`
- **`ch_reg` / `ch_mem` 等其它节点类型的命名** ⇒ 本项只管 `ch_in` / `ch_out` ⇒ ⇒ `ch_reg` 已有名字参数，不动
- **存量 3103 处的一次性消化** ⇒ ⇒ 靠 N2 棘轮**渐进**完成 ⇒ ⇒ 每批消化 MUST 有独立记录
- **`sanitize_verilog_name` 的规则**（关键字表等）⇒ 不动 ⇒ ⇒ 只加"显式名优先"路径

---

## Why

### W.1 现状端口名不可用

实测产物：`top_unnamed_input_3` = `prog_addr`。⇒ ⇒ 用户要写 TB，必须**查 port map**（本轮 planner 靠 node-id 对照才查出来）⇒ ⇒ **接口面不可自解释**。

### W.2 名字不稳定 ⇒ 端口名不能作为契约

④（按 eval 序去重）⇒ ⇒ 图里加一个无关节点 ⇒ ⇒ 所有后续端口的序号 **全体后移** ⇒ ⇒ 任何"按名引用"的外部资产（TB、wrapper、文档）**静默失配** ⇒ ⇒ 这比"名字无语义"更危险（无语义至少是显式的）。

### W.3 一次性 codemod 是**风险换时间**的错配

3103 处 ⇒ 一次提交 ⇒ ⇒ reviewer 只能"看个大概" ⇒ ⇒ 漏改不可见 ⇒ ⇒ 而漏改的后果是**静默**的（端口名变 ⇒ 行为错）。⇒ ⇒ **棘轮**把风险摊薄到每一小批，每批可被独立 review + 独立回归。

### W.4 复用既有机制 > 新造

⑥（唯一性策略已有）· ⑦（规则所有者已裁定）· `header_self_contained_exempt.txt`（棘轮样板已有）· `public-api-remove-visibility`（公开面登记已有）⇒ ⇒ **四项全部复用** ⇒ ⇒ 本项的**净新增**只有「带名构造 + 棘轮护栏 + 稳定性收口」三件事。

---

## What Changes

### N1 — 带名构造（向后兼容）

```cpp
// 既有（零变化）：名字仍是 "unnamed_input"
ch_in<ch_bool> prog_we;

// 新增：显式名字
ch_in<ch_bool> prog_we{"prog_we"};
```
- `ch_in<T>` / `ch_out<T>` 增加**带名构造**（或**第二构造参数**）⇒ ⇒ 名字进入图节点名 ⇒ ⇒ 产物端口名 = 该名字。
- ⚠️ **构造形态选择**：braced-init（`{"name"}`）vs 第二位置参数 ⇒ ⇒ 前者与 `ch_reg` 惯例**不一致**（`ch_reg` 是 `{init, "name"}`），后者会把 `ch_in` 的现有"值构造"语义（`ch_in<T>(value)`）挤掉 ⇒ ⇒ **裁决**：用**独立的具名构造**，不挤占现有形态。
- ⚠️ **零变化保证**：所有既有 `ch_in<...> x;` / `ch_in<...> x(value);` 的行为与产物**逐字节不变** ⇒ ⇒ 由既有 257 个 ctest + 层次化护栏 R1–R8 守住。

### N2 — 基线棘轮

- 登记表 `.rddf/checks/port-naming-registry.md`：
  - **豁免基线**：按文件登记「该文件的 `__io` 端口**尚未**显式命名」的**基线快照**（文件路径 + 端口条数 + 快照哈希）。
  - **规则**：新增组件 / 修改了 `__io` 的组件 ⇒ MUST 全部带名。
- 护栏 `scripts/verify_port_naming.sh`：
  | 面 | 判据 |
  |---|---|
  | P1 | **新增/改动**组件的 `__io` 端口 MUST 带名（按 git diff 增量 ⇒ 事件驱动） |
  | P2 | 豁免表与实际**双向一致**（表里有但文件没了 / 文件有但表里没有 ⇒ 都红） |
  | P3 | 豁免表**只许减不许增**（新增豁免条目 ⇒ 红 ⇒ 强制消化而非积累） |
  | P4 | `--self-test`（投毒必红 ⇒ 还原复绿） |
  | — | fail-closed（缺工具 ⇒ exit 2） |
- ⚠️ **P1 的粒度**：按**文件**（不是按端口）⇒ ⇒ 改一个文件 ⇒ 该文件**全部**端口须带名 ⇒ ⇒ 这会迫使「改一个组件 ⇒ 把该组件所有端口一次性带名」⇒ ⇒ **单批规模 = 一个组件**（可 review）⇒ ⇒ 消化速度 = 组件数（不是端口数）。

### N3 — 名字稳定性

- 显式名进入图后，顶层端口的最终名 MUST **按名去重** ⇒ ⇒ 同名 ⇒ **报错**（复用 `kRejectDuplicates`；默认策略仍是 `kAllowDuplicates` ⇒ ⇒ **既有行为不变**）。
- ⚠️ **顺序敏感消解**：现在 `_N` 后缀来自 eval-list 序 ⇒ ⇒ 显式名路径 MUST **绕开**该计数器（否则显式名也会被追加序号）⇒ ⇒ **判据**：一个组件加了显式名 ⇒ 该端口名 MUST **逐字**等于显式名 ⇒ ⇒ **零序号**。
- ⚠️ **存量兜底**：未显式命名的端口 **仍**走旧的去重路径（否则 3103 处端口名**全体变** ⇒ ⇒ 大爆炸）⇒ ⇒ 旧路径 MUST 保留为**兼容模式** ⇒ ⇒ 由棘轮逐步收敛。

### N4 — 规范落点

- `rtl-hierarchical-output` 追加 Requirement —— ⚠️ **条款正文见唯一落地文本**（见下条指针）⇒ ⇒ **规则只此一份**。
  - ✅ **2026-11-11 · critic 第 3 轮 BLOCKER-4 修复**：本行**不再自带条款正文** ⇒ ⇒ 改为**指针** ⇒ ⇒ **唯一落地文本 = `decision-request-port-naming-parity-2026-11.md` §2 R1-a**（`rtl-hierarchical-output` 的规范所有者 = 本 capability ⇒ ⇒ 「兼容去重路径 + 豁免表登记」这条**机制义务**在 R1-a 里也有 ⇒ ⇒ ⇒ **三处落点收敛为一处** ⇒ ⇒ 本行 MUST NOT 再复述条款（否则又造一份副本 ⇒ ⇒ 漂移）。
  - ⚠️ **落地指针**：R1-a 是**待走 openspec change 流程**的 delta 文本（`## ADDED Requirements`）⇒ ⇒ 立项后 MUST 以 R1-a 为**唯一源**逐字复制进 change 的 delta ⇒ ⇒ 本行在此之前 MUST NOT 另写一份正文。
  - ✅ **2026-11-10 · oracle 建议 1 裁决 = 方案 A（用户已点头）**：原措辞「顶层端口名 MUST 与 C++ 侧显式命名**逐字相同**」**已删除** ⇒ ⇒ 改为「**可机器解析**」+ 「字面同名」下沉为一条**带前提的 SHOULD**（限定 `is_top_io == true` 视图）+ 一条「**不可能条件** MUST NOT 被承诺」⇒ ⇒ **规则仍只此一份**（SHOULD 与 MUST NOT 都落在同一 capability 内）。
- `hierarchical-port-naming-guard` **MUST NOT** 复述（沿用既有裁定）。

### N5 — 同名的传递路径

`ch_in`/`ch_out` 的显式名 ⇒ 图节点名 ⇒ `build_verilog_name_table` 的 base name ⇒ `sanitize_verilog_name` ⇒ **顶层端口名** ⇒ ⇒ manifest `name` ⇒ ⇒ **C++ 成员名 = RTL 端口名**（用户 ⑤ 的诉求）。
- ⚠️ **sanitize 的副作用**：名字含非法字符会被替换（`-`→`_`）⇒ ⇒ **判据**：成员名 MUST 为合法 Verilog 标识符（`[A-Za-z_][A-Za-z0-9_]*`）⇒ ⇒ 护栏 P1 顺带检查 ⇒ ⇒ 否则 C++ 名与 RTL 名**不逐字相同**。

### N6 — 公开面登记

- `.rddf/checks/public-api-surface.md` 记「`ch_in` / `ch_out` 新增具名构造（**纯新增**，既有构造保留）」。
- `docs/CHANGELOG.md` 一行。

---

## Acceptance

### 框架层

- [ ] `ch_in<T>` / `ch_out<T>` 支持带名构造；**既有全部构造形态**行为与产物**逐字节不变**（257 个 ctest 全绿 + `verify_hierarchical_rtl.sh` R1–R8 全绿且产物逐字节不变）
- [ ] 显式名的端口，其 Verilog 顶层端口名**可机器解析**到 C++ 成员名（manifest 的 `cpp_member` 字段）⇒ ⇒ **判据正文见唯一落地文本** ⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ （本条 AC 只保留**可判定的期望值**，条款措辞以 R1-a 为准 ⇒ ⇒ 避免两处措辞漂移）
- [ ] 显式名路径**不追加去重序号**（N3 的「零序号」判据）
- [ ] 同名端口在 `kRejectDuplicates` 策略下**创建点抛异常**（复用 `core-ctx-node-name-uniqueness`；默认策略仍宽松 ⇒ 既有代码零影响）

### 棘轮

- [ ] `.rddf/checks/port-naming-registry.md` 存在，含存量豁免基线 + 双向校验规则
- [ ] `scripts/verify_port_naming.sh` 存在且可独立运行；`--self-test` 投毒必红 ⇒ 还原复绿
- [ ] 豁免表**只许减不许增**（新增豁免条目 ⇒ 护栏红）
- [ ] 改动任一含 `__io` 的文件 ⇒ 该文件全部端口 MUST 带名 ⇒ 否则护栏红
- [ ] 护栏 fail-closed（缺工具 ⇒ exit 2）

### 存量消化（**可增量，不设一次性完成条件**）

- [ ] 首批消化完成并登记（**至少 1 个组件**全量带名；建议选 `examples/riscv-mini/src/rv32i_pipeline_top.h` —— 32 个端口，是用户 ⑤ 最关心的那个）
- [ ] 每次消化 MUST 独立 commit + 独立回归 + 在登记表留痕（文件 + 端口数 + 日期）
- [ ] ⚠️ **本项 MUST NOT** 声称「存量已全部带名」⇒ ⇒ 豁免表非空时该项**未完成**（如实登记）

### 不回归

- [ ] 既有 **257** 个 ctest 全绿
- [ ] `scripts/verify_hierarchical_rtl.sh`（R1–R8）全绿且产物**逐字节不变**
- [ ] 层次化产物的顶层端口名与扁平产物**逐字相同**（`rtl-hierarchical-output` 核心契约 ⇒ MUST NOT 被本项破坏）
- [ ] `scripts/verify_public_api_visibility.sh` 全绿（公开面登记到位）
- [ ] `docs/CHANGELOG.md` 有本项的一行

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `rtl-hierarchical-output` | **ADDED** | 追加**一条** Requirement —— ⚠️ **条款正文见唯一落地文本**（**本行不引条款原文**）⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 BLOCKER-B**：Capabilities 表**正是 delta 的取材处** ⇒ ⇒ ⇒ 留引文 ⇒ ⇒ 实施方取材时拿到**缺 A/C 类豁免的旧版** ⇒ ⇒ ⇒ 故本行**连引文都不留**（只留指针）<br><br>⚠️ **2026-11-11 · critic 第 5 轮 SHOULD_FIX-2 修复 · 承载唯一性**：**本行即唯一承载**（= N4）⇒ ⇒ ⇒ `port-name-literal-parity` P2 **只是本项的契约化提案** ⇒ ⇒ ⇒ **MUST NOT 另立 change 承载同一条 Requirement**⇒ ⇒ ⇒ ⇒ （R1-b 由 `node-name-prefix-consistency` P3 承载）<br>⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-3**：delta 形态 `MODIFIED` → **`ADDED`**（`verify_openspec_delta.sh` R3 要求 MODIFIED 标题与主 spec **逐字一致** ⇒ ⇒ 否则**静默不合并**）<br>⚠️ **2026-11-10 · oracle 建议 1 裁决 = 方案 A（用户已点头）**：原「端口命名 MUST 与 C++ 侧显式命名**逐字相同**」**已删除** ⇒ ⇒ 「字面同名」改为**带前提的 SHOULD**（限定 `is_top_io == true` 视图）+ 一条「**不可能条件** MUST NOT 被承诺」⇒ ⇒ **MUST 集合覆盖 T1**（顶层 module 端口表全体）· **SHOULD 集合限定 T2**（oracle 建议 2）⇒ ⇒ ⚠️ 「层次化产物的顶层端口名与扁平产物**逐字相同**」是**另一条**契约（hier vs flat）⇒ ⇒ **本行不动它** |
| `core-ctx-node-name-uniqueness` | **不动**（复用） | 本项 MUST **复用**其 `kRejectDuplicates` 策略 ⇒ ⇒ MUST NOT 另造重名机制 |
| `hierarchical-port-naming-guard` | **不动** | 沿用既有裁定：guard capability MUST NOT 复述命名规则 |
| `port-naming-ratchet`**（新**） | ADDED | 棘轮契约：登记表 + 双向校验 + 只许减不许增 + fail-closed + `--self-test` ⇒ ⇒ **机制义务**，不承载命名规则本身 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `include/core/io.h` | `ch_in` / `ch_out` 构造**新增**（纯新增 ⇒ 源码兼容）⇒ ⇒ 但**所有**组件的 `__io` 端口在消化前仍是旧名 ⇒ ⇒ 产物端口名**混合**（已带名的用新名，未带名的用旧名）⇒ ⇒ ⚠️ 这本身是**可接受**的中间态（MUST 在 CHANGELOG 说明） |
| `include/codegen_naming.h` | 去重路径改（显式名优先 ⇒ 零序号）⇒ ⇒ **扁平与层次化共用** ⇒ ⇒ MUST 同批改 |
| 跨仓下游 | 端口名变化 ⇒ ⇒ 走 `public-api-remove-visibility` 机制登记 + CHANGELOG ⇒ ⇒ **MUST NOT** 静默改名 |
| 产物 golden | 既有逐字节 golden（若比对 .v）MUST 更新 ⇒ ⇒ 登记 |
| CI | 新增护栏 ⇒ ⇒ 墙钟 +（纯文本扫描 ⇒ ≈ 忽略不计） |
| 计数锚点 | `CTEST_TOTAL` **不变**（护栏自测若进 ctest 则 +1 ⇒ ⇒ 同步 `AGENTS.md`） |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **显式名与 `sanitize_verilog_name` 不一致**（成员名含 `-` 等）⇒ ⇒ C++ 名 ≠ RTL 名 ⇒ ⇒ 用户 ⑤ 的诉求落空 | 护栏 P1 检查成员名 MUST 为合法 Verilog 标识符 ⇒ ⇒ 不合法 ⇒ 护栏红 |
| **中间态混合命名**让用户困惑 | manifest 的 `role` + `semantic` 字段标注 ⇒ ⇒ 显式名者有 `semantic`，未命名者标 `data_unused`/空 ⇒ ⇒ 可区分 |
| **棘轮长期不消化**（豁免表只减不增 ⇒ 若无人消化 ⇒ 表不动 ⇒ 缺口留存） | 表非空 ⇒ 本项**如实登记为未完成** ⇒ ⇒ 且 P3 每次 PR 都提醒 ⇒ ⇒ 压力靠 CI 而非自觉 |
| **N3 的「零序号」改动波及全部端口** | 只对**显式名**路径生效 ⇒ ⇒ 存量端口走兼容路径 ⇒ ⇒ 产物变化**限于**已带名的组件 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `rtl-emit-product-tooling-2026-11` | **本项依赖它**（manifest 是端口名的载体）⇒ ⇒ 它先做；它 MUST NOT 在本项落地前定义命名规则 |
| `examples-rtl-sim-coverage-2026-11` | 依赖 manifest 的 `name` ⇒ ⇒ 命名落地后 TB 端口名才可读 ⇒ ⇒ **本项先做**（至少首批消化先做） |
| `memory-timing-contract-2026-11` | 无耦合 |
| `rv32i-dead-component-consolidation-2026-11` | ⚠️ **有耦合**：I5 会改 `Rv32iDecoder` / `Rv32iAlu` / `HazardUnit` 的 `__io` ⇒ ⇒ 那些组件**被改动** ⇒ ⇒ 触发本项 P1 ⇒ ⇒ **必须带名** ⇒ ⇒ 顺序：I5 的每个组件改动 MUST 顺带带名（成本低，因为本来就在改）⇒ ⇒ **建议 I5 与本项首批合并** |

## 事实订正（planner 复核 · 2026-11-10）

> **触发**：planner 复核 commit `1dc1d98` 时逐条复现本文档的每个数值锚点 ⇒ 发现 **2 处不可复现** ⇒ ⇒ 纯文本订正，**未动任何决策**。

| # | 原值 | 实测值 | 复现命令 | 影响 |
|---|---|---|---|---|
| **F1** | 端口声明 **2769** 处（1420 + 1349） | **3103**（1577 + 1526） | `grep -rhoE '\bch_in<\|\bch_out<' include/ examples/ tests/ \| wc -l` | ⚠️ 原稿给出的**命令本身**产出 3103 ⇒ **数字与证据自相矛盾** ⇒ 各种路径组合都到不了 2769（`include/` 1215 · `+examples/` 1593 · `+scripts/` 3208 · `+src/` 3104）；即使按「声明形态」重数也是 **2888**（183 文件）⇒ ⇒ 原数字作废 |
| **F2** | 具名构造「仓内已有 **20+ 调用点**」 | **0** | 全仓 `.h/.cpp` 正则 `(ch_in\|ch_out\|port\|ch_logic_in\|ch_logic_out)\s*<…>\s*\{?\s*"` | ⚠️ **构造确实存在**（`io.h:180-182` / `:221` / `:266`）⇒ ⇒ metis 的**结论**（缺的**不是**构造）**仍然成立**；但正确的事实**更强**：这条路径**从未被任何代码走过** ⇒ ⇒ N1② 的探针**升格为 MUST** |

**处置**：F1 ⇒ 全文 7 处 + `improvement-suggestions.md` 索引 1 处同步订正；F2 ⇒ A.1② / N1 / metis finding 1 三处同步订正。
**未变**：方案 A′（框架先行 + 基线棘轮）**不受影响** —— 两个数字都指向「规模大到不能一次性 codemod」，且 F2 的修正**加强**了「必须先有可运行探针」的结论。

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **具名构造早已存在**（`include/core/io.h:221,266`）⇒ 原稿 N1「新增带名构造」是把已有能力当待建 | N1 **收缩**为「用法约定 + 最小可运行探针 + 一条 ctest + 文档」；并在 A.1② 记录订正<br>⚠️ **planner 复核订正**：原稿附的「仓内已有 20+ 调用点」**实测为 0** ⇒ ⇒ 该路径**从未被任何代码走过** ⇒ ⇒ N1② 的探针**升格为 MUST**（见 §事实订正 F2） |
| 2 | 🔴 产物里的 `top_` 是 **IR 节点名的路径前缀**，不是端口名 ⇒ 「C++ 名 == RTL 名」指哪一级**未定义** | A.1① 补记；N5 改为「**先裁决语义**（三选一，推荐 manifest 双字段）」 |
| 3 | `kRejectDuplicates` 会被 `ch_reg` 默认名触发 ⇒ **不可能**用于产品设计 | A.1⑥ 补记；N3 改为**端口作用域**的局部判定；**删除**「MUST NOT 另造重名机制」这条自相矛盾的约束 |
| 4 | 棘轮若按**文件** ⇒ 一次要改 94 个组件（`tests/test_hierarchy_verilog.cpp` 实测 94 个 `__io`）⇒ ⇒ 爆炸半径没被摊薄，只是换了方向 | N2 改为**端口级增量** + 扫描面限定 product 侧（`tests/**` 排除，沿用 `verify_input_port_additions.sh` 的既有裁决） |
| 5 | 登记表用**文件哈希**做基线 ⇒ 任何注释改动都失效 ⇒ 且「只许减」在自指的登记表上可被绕过 | N2 改为「条目身份 = 文件 + `__io` 块序号 + 声明文本」+ 计数锚点 `EXPECTED_EXEMPT_PORTS=<n>` **钉在护栏脚本**（照 `EXPECTED_PROBES=24`） |
| 6 | 「零序号」**不可判**（合法成员名可以以 `_2` 结尾） | N3 改为**期望字符串比对**（`find("input ... top_prog_we")` 命中 且 `find("top_unnamed_input") == npos`） |
| 7 | 文件级集合比对**堵不住**「豁免文件里新增未命名端口」 | N2 加**逐端口**双向校验 |
| 8 | `sanitize_verilog_name` 的关键字表**不完整**（`bit`/`time`/`event`/`table`… 不在表里 ⇒ 合法 C++ 名产出**非法 Verilog**） | 登记为已知缺口（并把「C++ 名 = RTL 名」的可判条件从正则改为**自反检查** `sanitize(名) == 名`） |

### ⚠️ 待用户裁决

1. ~~**「同名」的语义**（N5 三选一）~~ ✅ **已由用户 2026-11-10 裁决 = 两步走**（第 1 步 (a) manifest 双字段立刻做 · 第 2 步 (c) 独立立项只验根实例名清空后的 `parent==nullptr` 行为）。原三选一的表述保留在 N5 供追溯。**原始问题**：
2. ✅ **已裁决（2026-11-11 · 用户拍板）**：**首批消化对象 = `examples/riscv-mini/src/rv32i_pipeline_top.h`** —— 实测 **32 个端口**（5 个 `ch_in`：ext_clk · ext_rst · prog_we · prog_addr · prog_data；27 个 `ch_out`）⇒ ⇒ 正是用户 ⑤「端口命名」最关心的那颗核 ⇒ ⇒ 且它已是 SoC 级双后端 parity 的 DUT ⇒ ⇒ 有现成判据基础可挂。

## critic 第 1 轮（plan, build）—— **REJECT** · mergeable: no · scope_exceeded: false

> **轮次**：1/2 · 审查者：critic

### 🔴 BLOCKER（**需 architect**）：oracle 的 ⑥「剥根前缀」**落点不可实现** + 与已签批 ADR 冲突

| # | 事实 | 证据 |
|---|---|---|
| ① | `build_verilog_name_table(const std::vector<lnodeimpl *> &eval_order)` **只接名字、不接 root** ⇒ **无法知道「根前缀」是什么** | `include/codegen_naming.h:72` |
| ② | `top` 只是 `ch_device` 的实例名副作用 | `include/device.h:28` |
| ③ | 在该表里实现只能**硬编码 `"top_"`** ⇒ 对「`ch_module` 直接作顶层」的根**静默失效** | 同上 |
| ④ | 剥前缀使 Verilog 端口名 ≠ ctx 节点名派生值 ⇒ 冲撞「下游 `resolve_node_id(ctx,name)` 按 `node->name()` **精确匹配**」 | `docs/adr/ADR-061-*.md:32`（事实④）· `ADR-060:283`（G3-4 冒泡端口名恒为 ctx 级全局名） |
| ⑤ | 「先算去重计数器再剥前缀」**不保证唯一**：`top_prog_we`→`prog_we` 与一个 `parent_==nullptr` 的节点 `prog_we` 剥后**同名**，而计数器已在剥前分配过 ⇒ **重复标识符** | `src/core/lnodeimpl.cpp:33-35` |

⇒ ⇒ oracle 的推荐**不构成裁决**（本文档 N5 本就写着「需裁决」）。

**处置**：⏸ 交 architect。三条候选：
(a) **manifest 双字段**（`name` 保留 `top_prog_we` + `cpp_member` = C++ 成员名）⇒ **零产物变更 · 零 ADR 变更**，但用户的「RTL 侧同名」只兑现为「工具侧可查」；
(b) 改 `Component::name()` 语义 / 改名字表签名（牵动 flat + hierarchical **两个发射器** + ADR-059 + 跨仓下游）⇒ **真正兑现**，代价大；
(c) 只改 `ch_device` 的根实例名（`"top"` → 空）⇒ ⚠️ 需验：`top` 是否被 `codegen_verilog.h:41` 的扁平模块名 / `verilator` 的 `Vtop.h` 约定**硬依赖**。

### ✅ 已采纳的 SHOULD_FIX

- 关键字表补全（B0）在**剥前缀之前**对现网产物**无任何可观测变化**（节点名恒带 owner 路径前缀，`top_time` ≠ 裸关键字）⇒ 验证 MUST 用**单元判据**（`sanitize_verilog_name("bit") == "bit_r"` 等 4 条），不能靠产物 diff。**已记入 N2 的验收补充。**

### ⚠️ 顺带（critic NITS 采纳）

- `semantic` 字段与「双轨可区分」的判据冲突已在本轮前订正；`driven` 字段保留。
