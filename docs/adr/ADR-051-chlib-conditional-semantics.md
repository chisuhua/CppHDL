# ADR-051: chlib 条件构造的语义模型 —— 裁决「守卫贡献」与权威表面（`_if` 语句块 / `if_then` 表达式）

- Status: Accepted（proposed 2026-10-30）
- Date: 2026-10-30
- Author: build-agent（architect role）
- Parent: ADR-050（`ch_state_machine` 的状态动作语义 —— **同型机制**：守卫贡献 + 唯一写入点）· ADR-043（死 / 误导设施物理移除 —— 零消费者直接删、无别名期）· ADR-046（RV32I 单源化 —— 「一份权威」原则）
- 关联 OpenSpec change：**待立项**（实施本 ADR）
- 关联 capability：**新建** `chlib-conditional-semantics`（ADDED ×N ⇒ 归档**禁** `--skip-specs`）
- 前置审查：本 ADR 的两个候选实现形态与「双重失效」机制经**源码逐行核实 + 独立探针**（§1.4）⇒ 两条结论是实测发现，不是推理

## 1. Context

### 1.1 触发

`include/chlib/if_stmt.h` 的 `_if(cond).then(body).elif(cond).else_().end()` 是仓内**唯一**的「语句块风格」条件构造。它被 `include/chlib.h` 聚合器导出、被 `tests/chlib/test_if_stmt.cpp`（**10** 用例，已注册进 CTest）覆盖 ⇒ ⇒ 是**已发布**的公开 API。

### 1.2 缺陷：**双重失效** ⇒ 条件被**完全忽略**

| # | 机制 | 后果 |
|---|---|---|
| ① | `conditional_reg_assignment::next_proxy::operator=` 的**条件分支体是空的**（`if_stmt.h:159-162` 的注释：「实际实现需要更复杂的机制」）⇒ 条件块内的赋值本应被**收集**成候选值，实际**被丢弃** | 机制 ① 被 ② 掩盖（见下）⇒ **从未**表现为「赋值丢失」 |
| ② | `end()` **先**置 `finalized_ = true` **再**遍历分支体（`if_stmt.h:86-97`）⇒ 遍历期间 `is_active()` 返回 **false** ⇒ `operator=` 走**「直接赋值」**分支 | ⇒ 条件块内的 `reg->next = v` **无条件生效** ⇒ **条件被彻底忽略** |

⇒ ⇒ **实测/源码结论**：写 `_if(rst).then([&]{ reg->next = 0_d; })` ⇒ `reg->next` **每拍**都被写成 0 ⇒ 与「rst 时才清零」**相反** ⇒ ⇒ 这是「**名字不兑现语义**」的典型（与 ADR-050 处理的 FSM 缺陷**同族**）。

⚠️ 连带：`conditional_reg_assignment::in_conditional_block_` 字段被构造但**从不使用**（`next_proxy` 不看它）⇒ 死字段。

### 1.3 ⚠️ 测试**钉住**了坏行为 ⇒ 长期「假绿」

`tests/chlib/test_if_stmt.cpp:204-226` 的用例名是「conditional_reg_assign **inside** if block forwards to next」，其注释**明写**：

> end() sets finalized_=true BEFORE walking the bodies, so `conditional_block::is_active()` returns false when the body actually runs. The next_proxy::operator= therefore takes the "direct assign" branch … **This test pins down that current (deferred-select-tree) behavior so future changes are noticed.**

⇒ ⇒ 该用例**断言**「条件块内的赋值无条件生效」⇒ ⇒ 它是「**假绿钉子**」：任何**正确**的实现都会让它变红 ⇒ ⇒ **实施时 MUST 先改测试**（否则「修好了」与「测试红了」无法区分）。

⚠️ 同文件的另两个用例只断言 **C++ 标志**（`body_executed`）⇒ **零硬件覆盖** ⇒ ⇒ 整份测试**从不**检查寄存器**真的**被条件化。

### 1.4 规模事实与实测

- **活消费者 = 0**（实测 grep）：`_if` / `conditional_reg_assign` / `if_then` / `priority_if*` 的全部消费者只有 ① 本测试文件 ② 两个**禁用** sample（`samples/conditional_demo.cpp` · `samples/if_demo.cpp`）⇒ ⇒ **爆炸半径极小**；
- `samples/conditional_demo.cpp` 用的是**更早的 API**（`seq_if` / `reg_if` / `.endif()`）—— 当前头文件里**不存在** ⇒ 该 sample 需要**重写**（不是改名）；
- `multi_if`（`include/chlib/if.h`）的表达式形式**实现正确**：从后往前构 select 链 ⇒ **先匹配者胜** ⇒ 它是本 ADR 的**权威参照实现**；
- ⚠️ `priority_if` 与 `multi_if` 的 `operator T()` **逐行等价** ⇒ 「优先级 / 互斥优化」的承诺**未兑现**（名字 over-promise，与 `DCacheWriteBuffer` 同型）；
- **核实方法**：§1.2 的双重失效由**源码逐行**核实（`finalized_` 的置位行号早于分支体遍历行号）；§1.4 的消费者计数由 **grep** 确认；「`priority_if` ≡ `multi_if`」由两者 `operator T()` 的**逐行比对**确认。

## 2. Decision

### D1. 语义方向：**守卫贡献**（条件守卫的组合贡献），不是「执行分支」

语句块内对**目标**的赋值 MUST 被解释为「**在该分支条件下，该目标取该值**」⇒ ⇒ 框架 MUST 把各分支的候选值合成为**一条** `select` 链 ⇒ ⇒ **先匹配者胜**（分支顺序即优先级）⇒ ⇒ 未匹配 ⇒ 目标**保持**（寄存器）或取 `else_` 的值（输出）。

⇒ ⇒ 与 ADR-050 的**状态守卫贡献**同型：**框架只合成一条链，写入动作由框架在唯一一处完成** ⇒ ⇒ `ch_reg` 的**单 next 源**契约**零变化**（`core/` 零改动）。

### D2. 优先级：**先匹配者胜**（分支书写顺序）

`multi_if::operator T()` 的既有实现**已是**该语义 ⇒ ⇒ **不改**它 ⇒ ⇒ 语句块形式 MUST 与之**优先级顺序等价**（同一条件序列 ⇒ 同一分支被选中）。

⚠️ **等价范围限定**（critic 第 1 轮 SF2）：「**带 `else_` 的**序列 MUST **逐位等价**」；而**无** `else_` 的序列**不等价** —— `multi_if` 的尾部默认 = **最后一个分支的值**（`if.h:67`），而 D3 规定寄存器无 `else_` ⇒ **保持** ⇒ ⇒ 两者**刻意不同**（D3 的「保持」才是语句块语义想要的默认）⇒ ⇒ 「逐位等价」的全称表述**作废**，只保留「优先级顺序等价」。

### D3. 默认尾部：**寄存器保持** / **输出必须显式默认**

- **寄存器**：无 `else_` 分支 ⇒ 未匹配时**保持**（`reg->next = select(c0,v0, select(c1,v1, …, reg))`）⇒ 合法（读的是**当前值**，非组合环）；
- **输出端口**：MUST 有 `else_` 分支（显式默认）⇒ 否则「保持」= 引用自身 = **组合环** ⇒ ⇒ **缺失即显式拒绝**（构建期抛异常），**不**降级为「默认 0」。
  ⚠️ **判据是「target 级」而非「块级」**（critic 第 1 轮 SF1）：块**有** `else_` 但该输出**无**默认候选 ⇒ 块级检查会放过 ⇒ 组合环仍可达 ⇒ ⇒ 拒绝条件 MUST 写成「**被捕获的输出 target 无 `else_` 候选值 ⇒ 构建期拒绝**」（镜像 ADR-050 D4 的 per-target `resolve_or` 措辞）。

### D4. 拦截点：`conditional_assign(target).next() = value`（**改名** + 统一 `ch_reg` / `ch_out`）

- 条件块内的赋值 MUST 经由 `conditional_assign(target)` 捕获 ⇒ ⇒ 目标可以是 `ch_reg` **或** `ch_out`；
- ⚠️ **改名理由**：现名 `conditional_reg_assign` 表达「只对寄存器」⇒ 而输出端口**同样**需要条件化 ⇒ ⇒ 统一入口 `conditional_assign`（**零活消费者** ⇒ 改名零成本 ⇒ 依 Zero-Debt + ADR-043 **不留** deprecated 别名）；
- ⇒ ⇒ **`core/` 零改动**（拦截点在 `chlib` 层）⇒ ⇒ **普通** `reg->next = v`（未经 `conditional_assign`）**不被条件化** ⇒ 见 D6。

### D5. `end()` 的执行语义：**捕获**（遍历所有分支体），不是「条件成立才执行」

`end()` MUST 遍历**所有**分支体以**收集候选值** ⇒ ⇒ 分支体在**建图期**执行 ⇒ ⇒ 分支体 MUST **无副作用**（`std::cout` / 文件 IO / 改全局状态 ⇒ 会在建图期发生）⇒ ⇒ 这一点 MUST 写进头注释。

⚠️ 机制修正：`finalized_` MUST 在**遍历分支体之后**才置位 ⇒ 否则 `is_active()` 在捕获期间为 false ⇒ 拦截永不触发（§1.2 ②）。

⚠️ **析构兜底 × 抛出路径**（critic 第 1 轮 SF4）：`~conditional_block()` 在 `!finalized_` 时会**兜底调 `end()`**（`if_stmt.h:51-58`；既有用例「end() is idempotent」依赖此）⇒ ⇒ D3 新增的「构建期抛异常」若经**析构**路径触发 ⇒ 析构默认 `noexcept` ⇒ `std::terminate` ⇒ ⇒ **同仓 ADR-050 实施期实测③已登记过这一陷阱** ⇒ ⇒ 本 ADR 继承该结论：抛出路径 MUST 兼容析构调用（`noexcept(false)` + 「展开期不抛」守卫），**或**嵌套 / 缺省场景 MUST 强制显式 `end()`。

### D6. 未走 `conditional_assign` 的赋值：MUST 有**静态护栏**检出

条件块内的 `reg->next = v`（普通写法）**不**被条件化 ⇒ **静默**变成无条件 ⇒ ⇒ MUST 有静态护栏（形态同 `scripts/verify_fsm_next_wiring.sh`：**词法级**、闭包内检出「未走 `conditional_assign` 的 `->next =`」）。

⚠️ **输出端口是同型路径**（critic 第 1 轮 SF5）：条件块内裸 `io().x = v`（未走 `conditional_assign`）**同样**静默无条件化 ⇒ ⇒ 护栏 MUST **同时**覆盖 `->next =` **与** `io().x =`（后者在 `scripts/lib/fsm_wiring.py` 里已有 `RE_IO_ASSIGN` ⇒ 覆盖成本 ≈ 0）⇒ ⇒ **不得**只登记为已知漏报方向。

⚠️ **「共用同一份」的准确口径**（critic 第 1 轮 NIT2）：可共用的是**词法器**（注释 / 字符串剥离）与**花括号配平**；`fsm_wiring.py::find_closures` 的**入口匹配是 `on_active(` 专用** ⇒ 新护栏需**新增** `.then(` / `.else_(` 入口 ⇒ ⇒ 复用其**骨架**而非其入口表（口径**不会漂移**，但**不是**同一份入口表）。

### D7. 嵌套：active block MUST 是**栈**

内层 `_if` 的 `end()` MUST NOT 清掉外层 ⇒ ⇒ 内层先合成**一条**表达式，再作为外层该分支的**候选值** ⇒ ⇒ **显式 `end()` 是嵌套的纪律**（现状 API 已有 `end()`；析构虽会兜底调用，但兜底**不能**表达「内层先于外层合成」⇒ ⇒ 嵌套场景 MUST 显式 `end()`）。

### D8. `priority_if` / `priority_if_then`：**物理移除**

与 `multi_if` **逐行等价** ⇒ 「优先级 / 互斥优化」的承诺**未兑现**⇒ 零价值 + 名字 over-promise ⇒ ⇒ 依 Zero-Debt + ADR-043 先例（零消费者 ⇒ 直接删、无别名期）⇒ ⇒ **`conditional_demo`**（在实施 change 范围内，§6 改动面已列）随之改用 `if_then`；**`if_demo`** 对 `priority_if_then` 的引用随删除**自然失效** ⇒ 归其**独立归位**（§4「不在本次范围」，卡在 `ch_in<ch_bool>` 绑定面）。

### D9. capability 归属：**新建** `chlib-conditional-semantics`

条件构造是**组合语义**（非时序、非 FSM）⇒ ⇒ **不**塞进 `chlib-state-machine-semantics`（那是 FSM 的契约）⇒ ⇒ 新建 capability（与 ADR-050 的 `chlib-state-machine-semantics` **同族**）。

### D10. 与 ADR-050 的关系：**同型不同层** ⇒ 共享「守卫贡献」组合器

ADR-050 的 `ch_state_machine::contrib` 与本 ADR 的语句块贡献是**同一个机制**（累积 `(守卫, 值)` + 合成**一条** select 链）⇒ ⇒ MUST 抽取**共享**组合器供两者使用 ⇒ ⇒ **一处实现、两处使用** ⇒ ⇒ 不重复实现同一语义（否则两份必然漂移）。

⚠️ **约束**：ADR-050 的既有金标 MUST **逐项不变**（`uart_tx` 的 `busy` 高 **3906** 拍 + `0xA5` 位序；`complex_e2e` 的 `0x96` MSB-first 16 位 + 状态序列 + `bit_count` 复位）⇒ ⇒ 抽取是**纯重构**，不是行为变更。

⚠️ **边界**：**不**把 ADR-050 的 `contrib` **泛化**成公开的通用 API（`core/` 与 `ch_reg`/`ch_out` 的公开契约**零变化**）⇒ ⇒ 共享的是**内部**组合器。

## 3. 备选方案（否决）

| 选项 | 结论 | 理由 |
|---|---|---|
| **S1** 保持现状 + 只改文档（说明「条件块内的赋值是无条件的」） | **否决** | 名字**不兑现**语义 ⇒ 用户按名字写出的代码**行为相反** ⇒ 比「没有这个 API」更危险（与 ADR-050 的 FSM 案例**同型**）⇒ 且测试**钉住**坏行为 ⇒ 长期假绿 |
| **S2** **物理删除**整个 `_if` 语句块 DSL（`if_stmt.h`） | **否决** | ① 它是**已发布** API（聚合器导出 + CTest 覆盖）⇒ 删除是破坏性变更；② 语句块形式是 **SpinalHDL `when` 块**的 parity 形态（根 `AGENTS.md` 明列 SpinalHDL API parity 为目标）⇒ 表达力（多个目标各自的优先级链）**确实**有；③ 两个 sample 的归位条件**指名**它 ⇒ ⇒ 实现它比删它更符合仓内方向 |
| **S3** **只**实现表达式形式（`if_then`）+ 删除语句块形式 | **否决** | 表达式形式**不能**表达「同一寄存器在多分支下有不同候选值、且未匹配时保持」之外的**多个目标**场景（每个目标都要手写一遍链）⇒ 收益不足 ⇒ 且语句块形式的**存在价值**恰恰在此 ⇒ 与 S2 同因 |
| **S4** 让 `ch_reg::operator->` 的 `next` 代理**感知**条件块（`core/reg.h` 改动） | **否决** | 需 `core/` 与 `chlib/` **双向耦合 + 全局状态**（与 ADR-050 的 S3 同因）⇒ ⇒ 保持 `core/` 零改动（D4 的拦截点 + D6 的静态护栏即可覆盖） |
| **S5** 用 **C++ `if` / 三元**决定「哪个分支发射」（即真·执行语义） | **否决** | 条件是**硬件值** ⇒ C++ 控制流在**建图期**求值（`ch_bool::operator uint64_t()` 只对 const 节点返真值）⇒ 恒假 ⇒ 分支体从不发射（根 `AGENTS.md` §ANTI-PATTERNS 已登记该类）⇒ ⇒ 条件**只能**落在表达式的取值里 |
| **S6** 条件块内**禁止**一切普通 `->next =`（**编译期**拒绝） | **否决** | 拦截点在 `chlib` ⇒ `core/reg.h` 的 `next` 代理**不感知**条件块 ⇒ 无法编译期拒绝 ⇒ ⇒ 退化为**静态护栏**（D6）—— 方向上宁可漏报、不误报 |
| **S7** 让 `priority_if` **真的**做「互斥优化」（减少 select 项数） | **否决** | 「互斥」是**运行期**性质 ⇒ 编译器**无法**证明 ⇒ 只能靠用户断言 ⇒ 优化不可靠 ⇒ ⇒ 反而保留一个**名字承诺**、实现**不兑现**的 API ⇒ 与 D8 的删除同向 |
| **S8** 把条件构造的契约塞进 `chlib-state-machine-semantics` | **否决** | 那是 **FSM** 的契约（状态守卫）⇒ 条件构造是**组合**语义 ⇒ 混在一起会让该 capability 边界失焦（ADR-046 的归属原则）⇒ 见 D9 |

## 4. Consequences

### 正面

- `_if` 的名字**兑现**它承诺的语义 ⇒ 「条件被忽略」这一**静默错误**被消除；
- 与 ADR-050 的贡献机制**同源**（共享组合器）⇒ 仓内「条件 / 状态守卫」只有**一份**语义实现；
- 表达式形式（`multi_if`）**不动** ⇒ 既有行为零变化；
- 爆炸半径**极小**（活消费者 0 + 测试 1 文件 + 2 个禁用 sample）⇒ 实施风险低；
- 静态护栏（D6）把「条件块内的普通 `->next =`」从**静默**变成**可抓**。

### 负面 / 代价

- **公开 API 改名**（`conditional_reg_assign` → `conditional_assign`）+ **移除** `priority_if` / `priority_if_then` ⇒ 破坏性变更（活消费者 0 ⇒ 成本≈0；但**已发布** ⇒ 属对外破坏）；
- **测试必须重写**：`test_if_stmt.cpp` 现**钉住**坏行为（§1.3）⇒ 且 10 个用例只断言 **C++ 标志** ⇒ ⇒ 需补**硬件覆盖**（真正的寄存器条件化）；
- 新增 capability ⇒ 主 spec 从 **28** 个增至 **29** 个 ⇒ 归档 MUST 带 delta（**禁** `--skip-specs`）；
- 抽取共享组合器 ⇒ **触碰 ADR-050 的已落地实现** ⇒ ⇒ 必须用其既有金标逐项证明**纯重构**（D10 的约束）。

### 风险

- ⚠️ **抽取共享组合器**若改变了 ADR-050 的链构造顺序 ⇒ 金标变化 ⇒ ⇒ MUST 以金标为闸门；
- ⚠️ **嵌套**（D7）若 active block 不是栈 ⇒ 内层 `end()` 清掉外层 ⇒ 静默丢候选值 ⇒ MUST 有嵌套用例；
- ⚠️ **输出端口缺 `else_`** 若不显式拒绝 ⇒ 组合环 ⇒ 拓扑排序 CHFATAL（**难**诊断）⇒ MUST 在**构建期**给出可行动的错误；
- ⚠️ **静态护栏**是**词法级** ⇒ 方向为「宁可漏报、不误报」⇒ **不得**扩展成语义分析（与 FSM 护栏同纪律）。

### 不在本次范围

- **`if_demo` sample** 的归位：它被另一个**框架层缺口**挡住（`ch_in<ch_bool>` **不能**绑到 `const ch_bool&` ⇒ `if_stmt.h` 的 `then/elif` 形参是 `const ch_bool&`）⇒ ⇒ **独立立项**（框架层绑定能力）⇒ 本 ADR **不**改 `ch_bool` 的绑定面；
- `samples/conditional_demo.cpp` 的**整体重写**（它用的是已不存在的 `seq_if` / `reg_if` / `.endif()` API）⇒ 属实施 change 的范围，**语义**由本 ADR 定；
- **表达式形式** `multi_if` / `if_then` 的任何行为变更 ⇒ 不变（D2 只要求语句块形式与之**等价**）。

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 条件化语义（D1 / D2 / D3）

- [ ] `_if(cond).then([&]{ conditional_assign(reg).next() = V; })` ⇒ **`cond` 为假时 `reg` 不被写**（**硬件级**断言，非 C++ 标志）；
- [ ] 多分支 ⇒ **先匹配者胜**（`c0` 与 `c1` **同时**为真 ⇒ 取 `c0` 的值）⇒ 与 `multi_if` 的同条件序列**逐位等价**；
- [ ] 无 `else_` 的**寄存器** ⇒ 未匹配时**保持**；
- [ ] **输出端口缺 `else_`** ⇒ **构建期抛异常**（`REQUIRE_THROWS_AS`）⇒ **不**是组合环 CHFATAL。

### 5.2 拦截与捕获（D4 / D5 / D7）

- [ ] `conditional_assign` 对 `ch_reg` **与** `ch_out` 都可用（**各一条**用例）；
- [ ] `end()` 遍历**所有**分支体 ⇒ 分支体在**建图期**执行 ⇒ 头注释 MUST 写明「分支体 MUST 无副作用」；
- [ ] **嵌套** `_if` ⇒ 内层先合成一条表达式再作为外层候选值 ⇒ 断言**内外两层**都正确；
- [ ] `conditional_reg_assign` **不再存在**（编译期断言：无可用重载）。

### 5.3 静态护栏（D6）

- [ ] `scripts/verify_if_stmt_wiring.sh` 主路径绿（检出条件块内**未走** `conditional_assign` 的 `->next =`）；
- [ ] `--self-test` fixture 全绿（含「闭包内未走 `conditional_assign` ⇒ 违规」与「走了 ⇒ PASS」两条**对照**）；
- [ ] 分析器与 `scripts/lib/fsm_wiring.py` **共用同一份闭包解析**（⇒ 口径不漂移）；
- [ ] 护栏对**真实历史缺陷**有效：把 `samples/conditional_demo.cpp:63-78` 的形态（`seq_if` / `reg_if` 体内的裸 `reg_count->next =`）搬进 fixture ⇒ 报出违规（⚠️ §1.2 的形态用的是 `conditional_reg_assign` = **走了**拦截点的旧名 ⇒ 对词法护栏**零信号** ⇒ 不可作参照）。

### 5.4 变异验证（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）

- [ ] M1 把 `end()` 的 `finalized_ = true` 移回遍历**之前** ⇒ §5.1 的「条件为假时不被写」**转红**（⇒ 证明该用例**真的**在测条件化，而非被坏行为掩盖）；
- [ ] M2 把捕获分支改回**空实现**（丢弃候选值）⇒ 转红；
- [ ] M3 把链构造顺序改成「后匹配者胜」⇒ 多分支用例**转红**；
- [ ] M4 去掉输出端口的「缺 `else_` ⇒ 抛异常」⇒ 该用例**转红**；
- [ ] M5 把 active block 改成**单层**（内层 `end()` 清外层）⇒ 嵌套用例**转红**；
- [ ] M6 抽取共享组合器时改变链构造顺序 ⇒ **`_if` 侧的「先匹配者胜」转红**
      （⇒ 证明**共享组合器**那份实现确实被钉住）。
      ⚠️ **实施期实测订正**（2026-10-30）：本条**初稿**写的是「⇒ **ADR-050 的既有
      金标**转红」⇒ ⇒ **该预测被实测证伪** ⇒ ⇒ 改为上面的形式，并**新增**一条
      事实：**链序对 ADR-050 侧不可观测** —— ADR-050 的守卫是**状态守卫**
      （`is_in(S)`），**同一时刻至多一个为真** ⇒ ⇒ 无论链序如何，结果**逐位相同**
      ⇒ ⇒ 「D10 是纯重构」的证据 MUST 是「**ADR-050 的既有金标逐项不变**（绿灯）」
      （实测 ✅），**不**可能是「链序变异下金标转红」。

### 5.5 回归与计数

- [ ] `ctest -LE perf` 零回归；`test_if_stmt` 绿（**补硬件覆盖**后用例数增加）；
- [ ] **计数锚点不变**（用例进**既有** target ⇒ `CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** / `verilator_e2e` **38** / non-perf **191**）；
- [ ] `include/chlib/AGENTS.md` 与两个头文件的注释**与实际行为一致**。

## 6. 实施追踪

- **change**：**待立项**（planner 建 change，带 `chlib-conditional-semantics` 的 ADDED delta ⇒ 归档**禁** `--skip-specs`）⇒ 其 `design.md` 承载逐拍推导 / 变异清单 / 探针证据；`tasks.md` 承载可勾选任务（**过程日志不进本 ADR**）；
- **改动面（预期）**：`include/chlib/if_stmt.h`（+ 共享组合器的新头 · `include/chlib/if.h` 移除 `priority_if` · `tests/chlib/test_if_stmt.cpp` 重写 · 新护栏 `scripts/verify_if_stmt_wiring.sh` · `samples/conditional_demo.cpp` 重写并从 `samples/disabled_samples.txt` 移除 · `include/chlib/AGENTS.md` + capability delta）；
- **爆炸半径**：**极小**（活消费者 **0** ⇒ 仅测试文件与 2 个禁用 sample）；
- **回滚路径**：**revert 实施 commit** 即可 ⇒ 旧语义 = §1.2（条件被忽略）⇒ **无迁移负担**（无持久化格式、无数据兼容面）。

## 7. 参考与审查记录

- **依据**：`include/chlib/if_stmt.h`（现状：`next_proxy::operator=` 条件分支为空 · `end()` 的 `finalized_` 时序）· `include/chlib/if.h`（`multi_if` 正确实现 = 权威参照；`priority_if` 与之逐行等价）· `tests/chlib/test_if_stmt.cpp:204-226`（**钉住坏行为**的用例 + 注释）· `samples/conditional_demo.cpp`（用**已不存在**的 `seq_if` / `reg_if` / `.endif()`）· `samples/disabled_samples.txt`（归位条件指名本项）· `docs/adr/ADR-050-chlib-state-machine-semantics.md`（**同型机制** + 「唯一写入点」纪律 + 「贡献 API」形态）· `scripts/lib/fsm_wiring.py`（可复用的闭包解析器）。
- **实测**：§1.2 的双重失效由**源码逐行核实**（`finalized_` 置位在遍历之前 ⇒ `is_active()` 在捕获期间恒 false）；§1.4 的消费者计数与「`priority_if` ≡ `multi_if`」由 **grep + 逐行比对**确认；`multi_if` 的「先匹配者胜」由其**从后往前**构链的代码结构确认。
- **对抗审稿（critic，`review_profile: arch`，2026-10-30，第 1 轮）**：**APPROVE_WITH_CONDITIONS**（**0 BLOCKER** / `mergeable: yes` / `scope_exceeded: false`）⇒ 6 SHOULD_FIX + 4 NIT 同批闭合：SF1 = D3 的拒绝判据 MUST 是 **target 级**（块有 `else_` 但该输出无默认候选 ⇒ 组合环仍可达）⇒ 改写 + §5.1 AC 同步；SF2 = D2 的「逐位等价」与 D3 在**无 `else_` 的寄存器序列**上冲突（`multi_if` 尾部 = 末分支值 vs D3 = 保持）⇒ 等价范围限定为「优先级顺序等价 / **带 `else_` 的**序列逐位等价」；SF3 = D8 与 §4「不在本次范围」对 `if_demo` 给出不同答案 ⇒ D8 只承诺 `conditional_demo`，`if_demo` 归独立归位；SF4 = **析构兜底 × 抛出路径**（`~conditional_block()` 兜底调 `end()` ⇒ 新增的构建期抛异常经析构触发 ⇒ `std::terminate` ⇒ 继承 ADR-050 实测③ 的 `noexcept(false)` + 展开期不抛）；SF5 = D6 的护栏**未覆盖输出路径**（裸 `io().x = v` 同型）⇒ 纳入（`RE_IO_ASSIGN` 已存在 ⇒ 成本 ≈ 0）；SF6 = §1.4 末条含草稿残留且引用了不存在的探针 ⇒ 改写为实际核实方法；NIT = §5.3 的参照改为 `samples/conditional_demo.cpp:63-78` 的裸 `->next =` 形态 / D6 的「共用」口径限定为**词法器 + 花括号配平**（`find_closures` 的入口表是 `on_active(` 专用）/ D7 的「显式 `end()`」表述改为**纪律**。
