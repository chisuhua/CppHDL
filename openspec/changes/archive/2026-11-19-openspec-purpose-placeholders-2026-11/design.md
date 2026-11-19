# openspec-purpose-placeholders-2026-11 — Design

> ⚠️ **本文件的 §2 是「批 A 的唯一可批准对象」**：那 8 段文本 = 执行时**逐字**写进
> `openspec/specs/<capability>/spec.md` 的 `## Purpose` 段（**禁自由发挥**）。
>
> 文本来源 = `.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md` §6（**单一真相源**）
> ⇒ ⇒ 本文件与那份报告的 §6 **逐字一致**（由脚本抽取，非手抄）。

---

## 1. 为什么可以直改主 spec（裁决依据 —— 批准前请先看这段）

| # | 事实 | 证据 |
|---|---|---|
| G1 | CLI 要求**直改主 spec** | `openspec validate <cap> --strict` warning 原文：「…**editing the main spec directly**: a `## Purpose` in a delta is read only when the capability is created, so it cannot replace this one.」 |
| G2 | **delta 机制上不含 Purpose** | 仓内 delta 实测格式 = `## ADDED/MODIFIED Requirements`；护栏脚本自己也这么写：`scripts/verify_openspec_delta.sh:112`「`## Purpose` **不会被合并** ⇒ 不算违规」 |
| G3 | AGENTS.md 禁止直改主 spec | 「OPENSPEC WORKFLOW」段：「直接修改 `openspec/specs/<capability>/spec.md` 是**禁止的**」 |
| G4 | ⇒ **change 流程对 Purpose 是死路** | G1 + G2 |
| G5 | ⇒ **本仓已裁决过一次** | `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md:277`「delta 的 Purpose 不合并 ⇒ ⇒ **归档后手工写**」；`:345`「Purpose 的两处落点」；`:484` ⑨ 列为 **builder 的任务** |
| G6 | 该先例**已落地** | commit `1952ba5`（2026-11-17）「… + T17 两处 Purpose」= `rtl-emit-portability` 占位符 → 实质 Purpose + `rtl-hierarchical-output` Purpose 改写，与 archive **同 commit** |
| G7 | 先例**只覆盖 Purpose** | ⇒ ⇒ **Purpose 与 Requirement 必须分两批**（本 change 只做 Purpose） |

⇒ **本 change 的形状 = ADR-072 §6 ⑨ 的形状**：change 承载**任务 + 逐字文本**，执行时
**手工写进主 spec** ⇒ ⇒ 与 G5/G6 先例一致，**且不违反 G3**（G3 禁的是「绕过 delta 改
Requirement」，本批**零 Requirement 改动**）。

⚠️ **本批对 Requirement 的处置**：**一律不动**。§3 列出的 19 处过期/矛盾项
⇒ **全部另行立项**（每条一个 openspec change + MODIFIED delta）⇒ 理由与逐条判据见
`.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md` §4.2。

---

## 2. 8 段 Purpose 逐字终稿（**批准对象**）

### 2.1 `build-cpp-standard`

```text
定义 CppHDL 的 **C++ 语言基线**契约 —— 整个仓库（库 / 示例 / 测试 / benchmark）MUST 以 **C++23** 为唯一的 C++ 标准基线，且**不得**由任何子目录单独降级或改写。

- **唯一权威**：根 `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD` / `CMAKE_CXX_STANDARD_REQUIRED` / `CMAKE_CXX_EXTENSIONS` 三元组；子目录 CMakeLists **MUST NOT** 覆写这三项（⇒ ⇒ 覆写即视为违反本 capability）。
- **为什么存在**：C++ 标准差异会改变**语言语义**（`char8_t` / `auto{x}` / `consteval` 收紧）⇒ ⇒ 同一份源码在不同子目录用不同标准编译 ⇒ ⇒ 「能编过」与「行为一致」不再等价 ⇒ ⇒ 标准基线必须有单点权威。
- **边界**：本 capability **不**管辖具体特性的可用性（`std::expected` 的平台限制 ⇒ 归 `verilator-error-flow`）⇒ **不**管辖第三方代码（`third_party/**` / `external/**` 保留其自带标准）⇒ **不**管辖「升级到新标准时的破坏点审计」——那是**一次性动作**，其结论保留在归档 change 中。
```

### 2.2 `jit-llvm-compat-tests`

```text
定义 CppHDL **JIT 后端与 LLVM 版本之间的兼容性红线**契约 —— 仓库必须携带一组**可判定的版本探测测试**，使「换一个 LLVM 大版本会不会静默把 JIT 打坏」这件事在构建矩阵里**可见**，而不是等到某个设计跑不出结果时才发现。

- **覆盖**：① **版本区间判定**（当前受测区间 = LLVM 17–22）⇒ ⇒ 区间内 JIT **MUST** 可用；区间外 **MUST** 显式 SUCCEED 并**报出版本号**（⇒ ⇒ 「未来版本需要显式 opt-in」而不是「静默失败」）；② **构造期不崩**（构造 + 可用性报告）；③ **关闭 JIT 的构建矩阵不受影响**（该配置下这些测试 **MUST NOT** 链接 JIT 实现）。
- **为什么存在**：LLVM 的 C 风格 `LLVMInitializeNative*` 家族在大版本间被移除 ⇒ ⇒ 故障只在**编译期**暴露 ⇒ ⇒ 需要一个**每次构建都跑**的红线测试，把「编译不过」变成一条明确的红线。
- **边界**：本 capability **不**管辖 JIT 的**性能**（⇒ 归 perf 基线）⇒ **不**管辖 JIT 的**正确性**（⇒ 归 golden 测试）⇒ **不**规定「JIT 应当支持哪些 LLVM 版本」这个**产品决策**（⇒ 属 roadmap）；只要求「区间判定 + 区间外的显式 opt-in 机制」存在。
```

### 2.3 `rv32i-privilege`

```text
定义 RV32I **机器级陷阱入口的异常上下文写入**契约 —— 陷阱被接收时，`mepc`（异常指令 PC）与 `mcause`（异常原因码）MUST 以**同一次异常的一对**被写入 CSR 存储体，并且 MUST 可从 CSR 存储体**读回**。

- **为什么存在**：陷阱上下文一旦丢失或**配对错位**（`mcause` 写的是另一次异常的原因码），软件侧就**无法**判定「为何陷入、陷入在哪」⇒ ⇒ 缺陷发生在**硬件层**却**只由软件可见** ⇒ ⇒ 必须在组件级把「写入 + 配对 + 读回」钉死。
- **覆盖**：写入的**两拍配对**语义（陷阱当拍写 `mepc`、其后一拍写 `mcause`）+ 从 CSR 存储体**读回**的闭环验收 + `exception_valid` 为**电平**时的重复写入行为。
- **⚠️ 边界（名与实的落差）**：本 capability 名字带 `privilege`，但**只覆盖陷阱入口的异常上下文写入**；**不**覆盖 CSR 指令语义（`csrrw`/`csrrs`/`csrrc`）· `ECALL`/`EBREAK` · `mtval` · `mtvec` 取值语义 · `mstatus` 的陷阱侧更新（`MIE`↔`MPIE`）· `MRET` 的 PC 恢复 · 并发异常的排队语义 ⇒ ⇒ 这些**当前不存在**，属独立立项（登记在 `.rddf/improvements/rv32i-privileged-csr-2026-11.md`）。
- **接缝**：本 capability **不**改变 `rv32i-isa-semantics` 的「未知 / 未实现 opcode ⇒ 流水线级冻结」条款 ⇒ ⇒ 未来把陷阱入口接进流水线时，**MUST** 以 MODIFIED delta 修订那条冻结条款（触发信号已登记）。
```

### 2.4 `verilator-backend-e2e`

```text
定义 **Verilator 后端与 CppHDL 解释器在端到端行为上的对齐契约** —— 同一份设计分别跑解释器与 Verilator 仿真时，两者在**可观测顶层端口**上 MUST 给出一致的结果。

- **覆盖**：① **符号解析**（后端 MUST 真实 `dlopen` 生成的共享库并解析出**全部 7 个**入口符号，缺一 MUST 优雅失败并报出缺失符号名）⇒ ⇒ ② **数据同步**（设计状态与仿真模型之间的双向读写 MUST 是真的读写，不是日志）⇒ ⇒ ③ **时钟模型**（1 cycle MUST 等于 3 次求值调用：组合求值 1 次 + 时钟沿相关求值 2 次）⇒ ⇒ ④ **缓存**（同一份设计的第二次初始化 MUST 命中缓存，MUST NOT 重跑编译子进程）⇒ ⇒ ⑤ **字面量宽度**（生成的 Verilog 字面量 MUST 落在目标 wire 宽度内 ⇒ ⇒ 否则 Verilator 报 `Too many digits` 硬错）⇒ ⇒ ⑥ **输出传播**（组合 `assign` 与 `always_ff` 两条路径的值 MUST 都回到可观测状态）。
- **为什么存在**：Verilator 后端是**第二条执行路径** ⇒ ⇒ 它与解释器不一致时，缺陷会在「只在 Verilator 跑」的交付面上暴露 ⇒ ⇒ 本 capability 是那条路径的一致性锚点。
- **边界**：本 capability **只管「两后端一致」**；**不**规定 Verilator 的**具体建模方式**（时钟序列 / 缓存键算法 / 生成命令属实现细节，会随 ADR-035 演进 ⇒ ⇒ 本规范只钉**可观测行为**）⇒ **不**覆盖「覆盖率的充分性」（⇒ 归 `verilator-e2e-coverage`）⇒ **不**覆盖「失败怎么被报告」（⇒ 归 `verilator-error-flow`）。
```

### 2.5 `verilator-e2e-coverage`

```text
定义 **Verilator 双后端仿真的覆盖面在 CTest 矩阵中可见**的契约 —— 「哪些设计类别被双后端仿真覆盖了」这件事 MUST 在 `ctest -N` 里**一眼可见**，而不是只藏在某个测试文件里。

- **覆盖**：① **类别维度**（trace parity / bundle / stream / memory / 复杂设计（流水线 + FSM）各自 MUST 至少有一组可执行用例）⇒ ⇒ ② **命名维度**（误导性的测试名 MUST 被纠正，且旧名 MUST 保留为 CTest alias 以免打断历史命令）⇒ ⇒ ③ **判据维度**（每条覆盖 MUST 同时有**双后端 parity 断言**与**手推的绝对语义金标** ⇒ ⇒ 两端由**同一份** fixture 生成 ⇒ ⇒ parity 对「同构缺陷」**结构性失明** ⇒ ⇒ 金标不可省）⇒ ⇒ ④ **判别力前提**（字节序方向 MUST 用**非回文**字节 ⇒ ⇒ 回文字节使「位序」这一维度零判别力）。
- **为什么存在**：覆盖不足在 CI 上是**静默**的（少一个测试，ctest 照样全绿）⇒ ⇒ 必须让「覆盖了什么」成为**可枚举**的矩阵，而不是靠人记。
- **边界**：本 capability **不**规定「具体覆盖了哪些 DUT」（⇒ 那会立刻过期）⇒ ⇒ 只规定**维度**；**不**管辖「两后端一致性的深度」（⇒ 归 `verilator-backend-e2e`）⇒ **不**管辖环境矩阵本身（平台范围 / CI 拓扑）。
```

### 2.6 `verilator-error-flow`

```text
定义 **Verilator 后端失败时的错误传递契约** —— 后端的每一个失败点 MUST 以**结构化错误**（机器可判的错误码 + 人类可读消息 + 源码位置）返回给调用方，而不是以布尔值 + 散落的日志存在。

- **覆盖**：① **错误类型契约**（错误码变体集合 + 消息 + 源码位置三元组）⇒ ⇒ ② **失败点覆盖面**（后端的**全部**失败路径 MUST 走同一条结构化通道，MUST NOT 有「某个失败点只返回 false 而不携带原因」的漏网）⇒ ⇒ ③ **向后兼容**（公共初始化入口仍返回布尔 ⇒ ⇒ 旧调用点 MUST NOT 被迫改写）⇒ ⇒ ④ **风险可追溯**（错误码与已登记风险项的对应关系 MUST 可见；**不**对应任何已登记风险的错误码 MUST 显式标注「无对应风险」）。
- **为什么存在**：后端失败在流水线里最常见的形态是「初始化返回 false ⇒ 测试 SKIP ⇒ 全绿」⇒ ⇒ 失败原因一旦不可见，**真实故障会被伪装成「环境不支持」** ⇒ ⇒ 错误码 MUST 可被断言。
- **边界**：本 capability **只管「失败怎么被报告」**；**不**规定「失败之后该怎么办」（重试策略 / 是否致命 ⇒ 属调用方）⇒ **不**管辖 Verilator 工具链自身的诊断输出。
```

### 2.7 `verilator-hier-input`

```text
定义 **层次化设计中「内部被驱动的输入端口」不得泄漏为顶层端口**的契约 —— 一个 `ch_in` 若在设计内部已经有了驱动（来自父层或同层信号），它 MUST NOT 出现在生成的顶层 Verilog 端口表里；它 MUST 退化为一条内部连线，其值由那条驱动决定。

- **为什么存在**：泄漏会把**内部信号**误升级为**外部契约**（外部 TB 会看到一个本不该存在的端口）⇒ ⇒ 更糟的是生成侧与仿真侧对这个端口的认知会分叉（一边当作顶层输入等外部驱动、一边当作内部连线）⇒ ⇒ 子模块的输入**永远拿不到值** ⇒ ⇒ 表现为「下游恒为未驱动 / 恒 0」。
- **覆盖**：① **判据口径**：判断「有无驱动」MUST 用**端口自身的驱动关系**判定（`ch_in` 节点自身的连线数**恒为 0**，驱动落在代理节点上 ⇒ ⇒ 用连线数判会把**每一个**被正常驱动的子端口误判成未驱动）⇒ ⇒ ② **两侧一致**：生成侧（端口表 + 内部连线 + 仿真主程序的端口写表 + 端口访问表）MUST 用**同一套**过滤口径 ⇒ ⇒ 任何一处漏过滤都会让仿真侧引用一个**不存在**的端口字段。
- **边界**：本 capability **只管「被驱动的输入端口的可见性」**；**不**管辖「未驱动输入端口的处置」（⇒ 归层次化输出契约）⇒ **不**管辖端口命名规则（⇒ 归端口命名契约）。
```

### 2.8 `verilator-reg-init`

```text
定义 **Verilator 后端与解释器在寄存器初值上的对齐契约** —— 带非零初值的寄存器，在两个后端上 MUST 从**同一个初值**开始，且**复位**语义 MUST 一致。

- **覆盖**：① **初值对齐**（`ch_reg` 的初值 MUST 经「一次复位脉冲」载入仿真模型 ⇒ ⇒ 否则模型从 0 起、解释器从初值起 ⇒ ⇒ 首个观察点就分叉）⇒ ⇒ ② **复位脉冲的单一实现**（初始化路径与复位路径 MUST 复用同一套脉冲序列 ⇒ ⇒ 两条路径各写一遍是漂移的来源）⇒ ⇒ ③ **字面量宽度**（复位分支的字面量 MUST 按寄存器宽度归一 ⇒ ⇒ 否则窄宽字面量会让复位值随字面量写法漂移）⇒ ⇒ ④ **幂等**（重复初始化 MUST NOT 产生累积副作用）。
- **为什么存在**：初值分叉是**静默**的 —— 首个差异往往出现在「初值非零的设计」上，而这类设计在测试里通常不占多数 ⇒ ⇒ 不钉住就会在集成期才暴露。
- **边界**：本 capability **只管「寄存器初值 / 复位」这一条对齐**；**不**管辖「复位后的功能行为」（⇒ 归各组件自己的 capability）⇒ **不**管辖「复位信号本身的产生方」（⇒ 归 SoC / 组件层）。
```

---

## 3. 本批明确**不改**的东西（避免「顺手改」）

| 类别 | 清单 | 归属 |
|---|---|---|
| 8 个 capability 的 **Requirement 正文 / Scenario** | **零改动**（AC-4 机械钉死） | 批 B：逐条立项 |
| `verilator-backend-e2e` 的 VCD「每拍自动 dump」 | **不改文案**（那是**实现缺口**，不是文案问题） | 独立立项（实现缺口） |
| `rv32i-privilege` 的**改名** | **不改名**（正文已有范围声明；`.rddf/improvements/rv32i-privileged-csr-2026-11.md:146` 已登记「扩展」⇒ 改名会作废已登记计划） | 留到真正覆盖 CSR 指令时 |
| 其余 43 个 capability 的 `>500 字符` warning | **不动** | 独立立项（另一项目） |
| 仓内 `openspec validate --specs --strict` 的**其余红项** | **不动**（本批只清 8 条 Purpose warning ⇒ 仓级 23→27 passed / 28→24 failed） | — |

---

## 4. 风险与反面判据（批准前请确认你接受这三条）

| # | 风险 | 反面判据（怎么发现做错了） |
|:-:|---|---|
| R1 | **散文注水**：8 段 Purpose 写成「Requirement 标题拼接」或 `⇒ ⇒` 装饰 ⇒ ⇒ 制造**第二份会漂移的真相源** | **AC-3′**：① Purpose 段 MUST NOT 出现任一 Requirement 标题的**完整文本**；② Purpose 的**任一行**与某标题（去标记去标点）的最长公共子串 / 标题长度 **MUST < 0.85** ⇒ ⇒ 「把 8 个标题逐条列成 8 行」必红，而「行内枚举类别名」不误报（⚠️ 旧版「< 10 字符」判据已被证伪 —— 会把「 MUST NOT 」这类功能词碎片算成 slop ⇒ 假阳性 3/4 ⇒ 已于 2026-11-19 作废，见审查报告 §11.8）|
| R2 | **把假话写进第一屏**：如「VCD 每拍 dump」「3 步时钟」「缓存对象是 Vtop」「每个错误码都有 ADR 注释」—— 这 4 条**都与实现相反** | §2 的 8 段文本已**逐条核对**不包含它们（`verilator-backend-e2e` 只写「7 个符号 / 1 cycle = 3 次求值 / 缓存命中」，不提缓存文件名）；AC-5：Purpose 里的**非 glob** 路径 `test -e` 必须成立 |
| R3 | **假绿**：以为 strict 红项会少 8 条 | AC-0：只有 **4 个** spec 会翻绿（另 4 个还有 `>500 字符` warning）⇒ 仓级 = **27 passed / 24 failed / 51 items** |

---

## 4.1 批准前的机械预检（已跑过，全绿）

预检脚本 = `/tmp/opencode/purpose/precheck.py`（T12 的护栏脚本将在其基础上落地到
`scripts/verify_openspec_purpose.sh`）⇒ ⇒ **2026-11-19 实跑结果：8/8 全过，0 问题**：

| capability | 字符数 | AC-2 | AC-3′ | AC-5 |
|---|---:|:-:|:-:|:-:|
| `build-cpp-standard` | 618 | ✅ | ✅ | ✅ |
| `jit-llvm-compat-tests` | 639 | ✅ | ✅ | ✅ |
| `rv32i-privilege` | 816 | ✅ | ✅ | ✅ |
| `verilator-backend-e2e` | 837 | ✅ | ✅ | ✅ |
| `verilator-e2e-coverage` | 685 | ✅ | ✅ | ✅ |
| `verilator-error-flow` | 584 | ✅ | ✅ | ✅ |
| `verilator-hier-input` | 614 | ✅ | ✅ | ✅ |
| `verilator-reg-init` | 554 | ✅ | ✅ | ✅ |

⇒ ⚠️ **预检在批准前抓到 2 个真问题**（否则会带着错批准）：
1. `verilator-e2e-coverage` 的 Purpose 开头是 Requirement 标题的**近义复述**（4 个实词重合）⇒ **已改写**；
2. **AC-3 判据本身有缺陷**（字符阈值会把功能词碎片算成 slop ⇒ 假阳性 3/4）⇒ **判据已改**（见 R1）。

---

## 5. 与既有工件的对应关系（可回溯）

| 本文件 | 来源 / 去处 |
|---|---|
| §1 裁决依据 | `.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md` §2.1（G1–G7） |
| §2 的 8 段文本 | 同上 §6.1–§6.8（**逐字**，由脚本抽取） |
| §3 的「不改」清单 | 同上 §2.3 + §4.2（19 条过期项） |
| §4 的三条风险 + §4.1 预检 | 同上 §5（AC-0…AC-7）+ §6 写法约定 + §11.8；metis S1–S6 / oracle 的假绿论证 |
| 批 B 的 19 条逐条判据 | 同上 §4.2（表头「判据 D1–D5」） |

---

## 6. 载体形态与跟踪面（P1 补充 —— 本节原本在手写的 proposal 里，现按工件归属归 design）

### 6.1 派生关系（proposal.md 的来源）

```
.rddf/improvements/openspec-purpose-placeholders-2026-11.md   ← 权威提案层（**入库** · 跨人可见）
        │  generate_full_proposal.py（rdd-builder P0 的生成器 · canonical）
        ▼
openspec/changes/openspec-purpose-placeholders-2026-11/proposal.md   ← **P0 生成物**（原样采用）
        │  P1（本文件 = design.md）
        ▼
design.md（§2 逐字终稿 = 批准对象）+ tasks.md（T0 批准门 + 任务 + AC + 批 B 清单）
```

⇒ 本仓同类先例 = `.rddf/improvements/arch-residual-registry-2026-11.md`（其 Status 行同样写明
「`proposal.md` 由 `rdd-builder` **P0** 从本草稿生成 ⇒ **不手写**」）。

### 6.2 为什么本 change **无 spec delta**（`skip_specs: true`）

- openspec 的 delta **机制上不含 `## Purpose`**（CLI 提示原文 + `scripts/verify_openspec_delta.sh:112`
  双重证实）⇒ ⇒ **change 流程对 Purpose 是死路** ⇒ ⇒ 该动作属「**归档后手工写**」通道
  （`ADR-072` §6 ⑨ + commit `1952ba5` 的 T17 先例）。
- ⇒ ⇒ 本 change 承载**任务 + 逐字终稿**，执行时手工写进主 spec ⇒ ⇒ **零 `### Requirement:` 改动**
  ⇒ ⇒ 与 AGENTS.md「spec 变更必须走 change」的禁令**不冲突**（该禁令的本意是防「绕过 delta 改
  Requirement」）。
- ⚠️ **代价（已知并接受）**：`openspec validate <change> --strict` 的「delta 完整性」检查对本 change
  **不适用** ⇒ ⇒ 实质校验落在 **AC-0…AC-10**（`tasks.md`）与 §4.1 的机械预检上。

### 6.3 跟踪面（你在哪里能看到它的进度）

| 视图 | 命令 | 性质 |
|---|---|---|
| **openspec（权威 · 入库）** | `openspec list` / `openspec validate <change>` | ✅ 提案 + 逐字终稿 + 任务 checkbox **都在 git 里** ⇒ 跨会话/跨人可见 |
| **rddf 面板（本机 · 不入库）** | `rddf status openspec-purpose-placeholders-2026-11` | ✅ `.rddf/state/iteration.json`（`status: proposed` · `tasks 0/23`）⇒ ⚠️ `.rddf/state/` 整个被 `.gitignore` ⇒ **只在本机可见** |

### 6.4 批准门

**T0（`tasks.md`）是硬门**：未经用户显式批准，**不得**执行 T1 及之后任何任务（改主 spec /
改 AGENTS.md / 落护栏 / 提交）。批准语句 + 日期回填进 `tasks.md` 的 T0 记录区。
