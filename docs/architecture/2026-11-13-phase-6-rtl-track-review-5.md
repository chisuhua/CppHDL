# Phase 6 / RTL 轨复盘 #5（2026-11-13 · architect 域）—— 活跃任务与后续任务梳理

> **触发**：用户口令「切architect对活跃的任务和后续任务进行梳理，检查下我们要实现的目标，
> 为什么要实现这个目标, 已经完成了哪些, 后续要怎么做, 为什么要这么做, 目前的任务是什么,
> 然后切planner对后续还有哪些任务内容都梳理一遍, 再给出后续任务内容和实施路径, 再切builder进行实施」。
> **本文件的定位**：**当前态快照 + 目标链复述 + 本轮实测发现 + 边界裁决**（架构主题文档 · **非决策文档**）
> ⇒ ⇒ ⚠️ **不反转任何已签批决策**；但 §4 含 **2 条 architect 裁量**（① ③）—— 性质 =
> **边界判断**（不是产品架构决策）⇒ ⇒ ⚠️ **用户可随时推翻**
> ⇒ ⇒ 落盘点 = `.rddf/roadmap/phase-6-next-steps.md` 新增 **R23** 行 + 对应判据文本
> （承复盘 #4 §4.1 确立的「裁量 MUST 落盘 planner 域」先例）。本轮**不新建 ADR**。
> ✅ **2026-11-13 用户追认**：「architect 复盘 #5 的裁决」**全部追认**（① ③）⇒ ⇒
> ⇒ ⇒ 裁决 ③ 的**回写动作已执行** = `docs/adr/ADR-035-verilator-backend.md` 的 **v2.3 轻量修订**
> （**修订行 + 原位订正 + 审计轨迹**，形态承 ADR-052 v2）⇒ ⇒ 排除面 = **结构特征 + 5 个具名文件**
> ⇒ ⇒ 且「**永久失明**」的代价已登记为残差台账 `id=24`（`已知`）。
> ⚠️ **状态真相不在本文件** —— 任务真相 = `.rddf/roadmap/phase-6-next-steps.md`（planner 域）；
> 本文件 §2–§6 是 2026-11-13 的**实测快照**，会过期 ⇒ ⇒ 不得当状态真相用。
> **上游** = `2026-11-13-phase-6-rtl-track-review.md`（复盘 #4 · 同一口令的上一轮）
> · `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · T2 裁决）·
> `docs/adr/ADR-035-verilator-backend.md`（**v2.2 · R9**，2026-11-13 已实施）。
> **下游** = `.rddf/roadmap/phase-6-next-steps.md`（任务内容 + 实施路径的权威载体）。

---

## 1. 目标链（**做什么 → 为什么**）

### 1.1 顶层目标（**承复盘 #4 §1.1，本轮不变**）

`CppHDL` 的定位是 **C++23 的硬件描述库** ⇒ ⇒ 交付物不只是「能仿真的 C++ 代码」，还必须包括
**能被下游 FPGA/ASIC 工具链吃掉的产物**（Verilog + 端口清单 + 行为等价性）。

⇒ ⇒ 用户 2026-11-10 拍板的 7 项诉求（生成器产品化 · 固件装载 · examples RTL 覆盖 ·
层次化示例 · 端口名一致 · RV32I 组件收口 · RTL 卫生）**每一条都指向同一个病根**：
**「声称成立但无人可验证」** ⇒ ⇒ 所以这条线的统一解法**不是加功能**，
而是**给每个声称配一条可复算的机械判据**（护栏 / 产物级断言 / 变异自证）。

### 1.2 ⚠️ **本轮的增量：瓶颈已经从「功能」移到「判据链的可达性」**

⇒ ⇒ 复盘 #4 时的局面是「**缺判据**」（F6 是 `note`、gpio 断言不足、端口面无台账行）。
⇒ ⇒ 本轮把 **B3 立项（R13）+ P8 落地（R4）** 做完之后，暴露出的**不是新的功能缺口**，
而是：**已经存在的判据，有一大半在 CI 里从未真正执行过**。

实测（2026-11-13，本轮连续 5 轮 CI 全红的**真实原因链**）：

| # | 现象 | 真实原因 | 类别 |
|---|---|---|---|
| RC-1 | `samples-regression` / `examples-runtime` job 红 | apt 源行漏 `/jammy/` ⇒ **两个 job 的护栏从未跑过** | CI 侧 · 护栏不可达 |
| RC-2 | 主 job 12 个假红 | `BUILD_VERILATOR=OFF` 却跑全量 ctest ⇒ 87 个测试「全跳过」⇒ Catch2 exit 4 | CI 侧 · 口径错配 |
| RC-3 | `downstream_consumer` 红 | 下游工程用 `/usr/bin/c++`（g++-11）⇒ 与上游编译器口径不一致 | CI 侧 · 环境不一致 |
| RC-4 | `test_mem_port_arg_cross_layer_values` 红 | 需真 verilator 但**刻意不带**该标签 ⇒ 全跳过 ⇒ exit 4 | CI 侧 · 标签口径 |
| RC-5 | `input_port_additions` 自检红 | 自检依赖**真实仓历史** ⇒ 浅克隆（`fetch-depth: 1`）无 `HEAD~1` | **设计缺陷**（自检耦合真实历史）|
| RC-6 | `verify_flat_rtl_emit` 红 | 护栏 R0 对 verilator **fail-closed**（exit 2）而主 job 是 `OFF` ⇒ 必然红 | CI 侧 · 护栏不可达 |
| RC-7 | `verify_get_lnode_entry_set` 红 | 裸 `clang++` 在 Ubuntu 22.04 是 **clang-14** ⇒ 不认 `-std=c++23` ⇒ 六个同型护栏全中 | CI 侧 · 环境假设 |
| RC-8 | `verify_stream_ready_ownership` 红（exit 126） | 脚本**缺可执行位**而 CI 直接调用它 | 仓内 · 权限位 |

⇒ ⇒ **每一条都在「前半段红」的遮蔽下，把后面的 step 挡住** ⇒ ⇒ **后面的护栏一次都没跑过**
⇒ ⇒ ⇒ **「仓里有护栏」与「护栏在跑」是两件事**，而本轮 streak 期间（本仓 5 连红）主 job 的护栏**零执行**。

⇒ ⇒ ⚠️ **机制说明**（避免把相关性当因果）：遮蔽 = **GitHub Actions 的 step 失败即停**
（主 job 的 ctest step 在 `ci.yml:207`，**全部**护栏 step 在其后）⇒ ⇒ `fail-fast: false` 只作用于
**job 间**，job 内无缓解 ⇒ ⇒ **「step 重排 / 关键 step 加 `if: always()`」是更便宜的编排修法**
（已考虑并登记为中等优先的独立改进 ⇒ ⇒ 但它只解决「红时看不见」，**不**解决 RC-6/RC-7/RC-8 的前提缺陷）。
⇒ ⇒ ⚠️ **口径收窄**：这 8 条**异质**，不该倒进同一个篮子 —— 真正支撑「可达性是新瓶颈」的只有
**RC-6 / RC-7 / RC-8**（环境/权限前提不满足 ⇒ 必然红）；RC-1/RC-2/RC-3/RC-4 是**口径与编排**问题，
RC-5 是**设计缺陷**（自检耦合真实仓历史）。
⇒ ⇒ ⚠️ **不写「长期」**：CI 历史上绿过时所有 step 都真实执行过 ⇒ ⇒ 准确表述是
「**本轮 streak（5 连红）期间**主 job 排在 ctest 之后的护栏 step 零执行；
另有 **2 条脚本自入仓起**（git mode `100644`）零执行」。
⇒ ⇒ ⚠️ 这正是仓内既有纪律的**同型缺陷**：「两后端一致」不能当正确性证据（AGENTS.md 明写
「同构缺陷两后端一致地错」）⇒ ⇒ **「护栏存在」同样不能当「护栏在跑」的证据**。

---

## 2. 已完成哪些（**2026-11-13 本轮增量**）

### 2.1 R13 = B3 立项 ✅（B3 的真起点）

两个 change 均已立项，`openspec validate --strict` **exit 0** + `verify_openspec_delta.sh` **4/4 绿**：

| change | delta | 校验 |
|---|---|---|
| `openspec/changes/memory-timing-contract-2026-11/` | `rv32i-memory-timing`(**ADDED**,新) · `rv32i-commit-path`(MODIFIED) · `rv32i-isa-semantics`(MODIFIED) | ✅ |
| `openspec/changes/firmware-load-chain-2026-11/` | `rv32i-firmware-loading`(**ADDED**,新) · `rv32i-isa-semantics`(**ADDED**) | ✅ |

⚠️ **一处草案订正**：草案把 `firmware-load-chain` 的 `rv32i-isa-semantics` 标 `MODIFIED`，
但其内容是「**追加**」⇒ ⇒ openspec 的 `MODIFIED` 是**整体替换** ⇒ ⇒ 照草案写会**误替换**既有 ISA 条款
⇒ ⇒ 改用 `ADDED`（既有条款逐字保留）。

### 2.2 R4 = P8 ✅（运行期 A/B stub 的整体退役）

① **代码面**：三个符号 + `tick()` 的 WARN 块 + 两个只为它存在的局部量（`ab_result_a` / `run_ab_verify`）
**全删**（代码面 grep == 0）⇒ ⇒ **两种 JIT 构建各自编译通过**（`CH_JIT_ENABLE=ON/OFF` 实测）。
② **公开面移除登记四件套**（`public-api-surface.md` +1 行 · `public-api-removals.tsv` +2 探针 ·
`CHANGELOG.md` 一节 · 锚点 18/30 → **19/32**）。
③ **golden 覆盖下限**（**集合判据**，不是计数）⇒ ⇒ 新增 `scripts/verify_jit_golden_coverage.sh`
+ 基线 `.rddf/checks/jit-golden-coverage.md`（**39 分母 / 5 已覆盖**）+ **5 条自检投毒**
（含「**计数不变但集合变**」⇒ ⇒ 计数字段恒绿的形态被实测抓住）。
④ **文案**：根 `AGENTS.md`（**替换** · 顺带订正「`CH_JIT_ENABLE=OFF` 时符号不存在」这个**事实错误**）
+ usage guide + JIT_ROADMAP + ADR-005（ADR-035 加**带日期后注**，**历史措辞不改写**）。

### 2.3 R19 / R20 ✅（复盘 #4 的发现 1 / 发现 2）

- **R19（F6 判据缺口）**：护栏 F6 从「硬编码 `note`」改为**元判据**（承载判据的测试 / 它的 CTest 标签 /
  跑它的 CI job 三者俱在）⇒ ⇒ **原「逐字节比对」形态恒绿**（两者出自**同一个** `verilogwriter`）⇒ ⇒ 不可用。
- **R20（gpio 断言强度）**：**早已闭**（2026-11-12 D1 裁决已显式标注「`spec_checks` 保持 0（只判 impl-contract）」）
  ⇒ ⇒ ⚠️ 本轮**差点又当新发现**（同一纪律第二次生效）。

### 2.4 R22 ✅（C6 解析器回归 —— 本轮亲手抓到的真缺陷）

`verify_no_duplicate_rv32i_headers.sh` 的 C6 判据用 `inb && /\)/ { exit }` 终止解析 ⇒ ⇒ **任何含 `)` 的行**都终止
⇒ ⇒ 而复盘 #4 给 `ext_clk` 加的说明注释里含 `always_ff @(posedge …)` ⇒ ⇒ 解析在**第一条注释**就退出
⇒ ⇒ 一个端口都抽不到 ⇒ ⇒ 假红。**修法**：跳过 `//` 注释行 + 只在**收尾行**（行内容即 `)`）终止。

⇒ ⇒ ⚠️ **这个回归自 2026-11-13（`2d247d1`）起就存在，但零信号** —— 因为该护栏在主 job 的**靠后**位置，
前面一直红 ⇒ ⇒ 它**从未真正跑过** ⇒ ⇒ **同 §1.2 的盲区**。

### 2.5 R17.2 ✅（用户 ⑤ 的原始诉求首次真实消费）

`Rv32iPipelineTop` 的 **32 个顶层端口全部具名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`
⇒ ⇒ 端口数量 / 方向 / 宽度 / 顺序**全部不变** ⇒ ⇒ 行为零变化 ⇒ ⇒ 棘轮基线 1484 → **1452**。

### 2.6 CI 红：RC-1 … RC-7 全部定位并修（**RC-8 待修** · 表见 §1.2）

⇒ ⇒ Debug/Release 的 ctest 已从 12 个假红降到 **0** ⇒ ⇒ **RC-2 口径修正已由 CI 验证**。
⇒ ⇒ ⚠️ **口径与锚点**（critic SHOULD_FIX）：该 **175** = CI job 的 **ctest 子集**口径
（**263 − 87**〔76 个 `verilator_e2e` + 11 个 `perf`〕**− 1**〔`test_mem_port_arg_cross_layer_values`〕）
⇒ ⇒ **不是**仓内 `CTEST_TOTAL: 263` 锚点 ⇒ ⇒ 复核 MUST 查 CI 日志的 **run 号**，不查锚点。
⇒ ⇒ **run 锚点** = run **`43389801471`**（push `8333da6`）⇒ ⇒ ⚠️ CI 日志**有保留期** ⇒ ⇒ 此证据**会过期**
⇒ ⇒ ⇒ 这正是裁决 ①-b「跑过一次绿是**易逝**证据」的活例。

---

## 3. ★ 本轮实测发现（**4 条** · 每条都有可复跑证据）

### 3.1 发现 1 🔴：**「护栏存在」≠「护栏在跑」**（本轮最重要的架构级发现）

- **证据**：RC-1/RC-6/RC-7/RC-8 四条**各自**遮蔽了一整段护栏链；`verify_flat_rtl_emit`
  在 `BUILD_VERILATOR=OFF` 的主 job 里**必然** exit 2 ⇒ ⇒ 主 job 的护栏里，**至少 3 个从未执行过**。
- **影响面**：不止 CI ⇒ ⇒ 本地也是同一形态 —— 「写了护栏」容易，「护栏在**这次**跑过」没人记。
- **根因**：护栏的**可达性**（工具齐备 · 标签口径 · 编译器口径 · 权限位 · 历史深度）**没有验收项**。
- ⇒ ⇒ **对策见 §4 裁决 ①**。

### 3.2 发现 2 🔴：**两个护栏脚本缺可执行位**（RC-8）

- `scripts/verify_stream_ready_ownership.sh` · `scripts/verify_stream_domain_closure.sh`
  ⇒ ⇒ mode 644 ⇒ ⇒ CI 以 `scripts/xxx.sh` 直接调用 ⇒ ⇒ **exit 126 Permission denied**。
- 实测口径：git 内 `scripts/**/*.sh` 共 **50** 个 —— **47** 个 mode `100755` · **3** 个 `100644`：
`scripts/lib/cxx23_compilers.sh`（**被 `source` 的库文件** ⇒ 不需可执行位 ⇒ **合规**）
+ 上述两个（**被 CI 直接调用** ⇒ ⇒ **缺陷**）⇒ ⇒ 即：**CI 直接调用的 36 个脚本里，2 个不可执行**。
- ⇒ ⇒ 这是**仓内**问题（不是 CI 侧）⇒ ⇒ 修复成本 = 两条 `chmod` ⇒ ⇒ 但**必须配一条判据**（§4 裁决 ①）。

### 3.3 发现 3 🟡：**improvement 状态漂移**（planner 域）

`memory-timing-contract-2026-11.md` 的 `状态:` 仍是 **`draft`**，而它的 🚪 门（R5 裁决 + R6 前置登记）**已闭**
且 **change 已立项** ⇒ ⇒ 状态与现实脱节 ⇒ ⇒ 与 architect 复盘 #3 的 finding F2 **同型**。

### 3.4 发现 4 🟡：**「文档面 0 命中」判据与「带日期的评审快照」冲突**

ADR-035 v2.2 的下游判据 (b) 要求「`AGENTS.md` + `docs/` 里每条命中 MUST 在同一段内含『已移除』或『已删除』」
⇒ ⇒ 但 `docs/architecture/2026-*` 是**带日期的评审快照**（当日事实记录）
⇒ ⇒ 改写它们 = **篡改历史**；不改 = 字面执行不满足。
⇒ ⇒ **裁决见 §4 裁决 ③**（口径澄清 + 排除口径写明，不追溯改写）。

---

## 4. 边界裁决（**architect 域的判断** —— 决定「什么算欠项、什么算门」）

### 4.1 裁决 ①：**护栏的「可达性」升为一等验收项**（✅ **用户 2026-11-13 追认**）

> ⚠️ **性质**：这是**护栏写作规范**（process），**不是**产品架构决策 ⇒ ⇒ 载体 = ①-a 的静态护栏
> + 一句 CONVENTION + `next-steps.md` 的 **R23** 行；**不**立 ADR（若用户追认时要求 ADR 形态，再立不迟）。

**①-a（机械层 · 静态 ⇒ 可做 fail-closed 护栏）**
**任何**被 CI 引用的脚本，其**静态前提**（权限位 · 工具+版本 · 标签口径 · 编译器能力 · 历史深度）
MUST 声明在脚本头或登记表，且 MUST 与**引用它的那个 step 的实际环境**逐项对齐
（工具名要**版本化**、标签口径要**闭合**、编译器要**实测能力**而不是假定、
脚本要**可执行**、自检要**不依赖真实仓历史**）。
⇒ ⇒ **判据 = 纯静态检查**（解析 `ci.yml` × 文件权限位 × 工具探测）⇒ ⇒ **无逆递归**（静态层自洽即终止）。

**①-b（流程层 · 一次性登记）**
新护栏首次合入时，其 CI run MUST 显示该护栏 step **executed（非 skipped）且绿**
⇒ ⇒ 落 `.rddf/checks/guard-reachability.md` 一行。
⇒ ⇒ ⚠️ **skipped step 不满足 ①-b** —— RC-6 的 gate 形态合法，但**必须**登记进**闭合的排除集合**
（排除集合本身是判据的一部分 ⇒ ⇒ 「想排除就排除」不成立）。
⇒ ⇒ ⚠️ 「跑过一次绿」是**易逝**证据（CI 日志有保留期）⇒ ⇒ ①-b 是**一次性登记**，**不是**常驻动态判据
（动态元护栏自己就是第一个「无法自证」的护栏 ⇒ ⇒ 不取）。

**为什么这条是架构级**：它把「判据」从「**存在性**」升级到「**有效性**」
⇒ ⇒ 与仓内既有纪律同族（「两后端一致」不能当正确性证据 ⇒ 「护栏存在」不能当「护栏在跑」的证据）。

### 4.2 裁决 ②：**R21（目标 ④ = RV32I 层次化 RTL 示例）维持独立立项**（承复盘 #4 裁决 ③）

- 默认 inline 是**已签批的契约行为**（`tests/test_hierarchy_equivalence.cpp` 有测试钉着）⇒ ⇒ **不得**改发射器。
- 真正的缺口 = **诉求 ④ 未兑现** + **跨 module 隐式端口合成未实现 ⇒ 分析层 fail-closed 抛异常**。
- ⇒ ⇒ **范围 MUST 先侦察**（探针：给 RV32I 声明一层 ⇒ ⇒ 看它是否撞 fail-closed）⇒ ⇒ 侦察结果出来前**不立项**。

### 4.3 裁决 ③：**带日期的评审快照 = 审计轨迹，不追溯改写**（✅ **用户 2026-11-13 追认**）

- `docs/architecture/2026-*` 的复盘快照与 `docs/adr/ADR-035-*` 的 v2.2 补丁章一样，是**当日记录**
  ⇒ ⇒ 「文档面 0 命中」类判据 MUST **显式排除**它们，否则该判据与「历史不改写」**直接互斥**。
- ⚠️ **排除面 MUST 按「结构特征」而非「目录名 / 日期前缀」**：
  复盘/中期评审快照 = 文件名匹配 `^\d{4}-\d{2}-\d{2}-.*(review|midterm).*\.md$` 的**具名清单**
  ⇒ ⇒ **活主题文档不在其列**（`2026-11-02-hierarchical-rtl-output.md` 是
  `verify_arch_ledger_residuals.sh` 的**消费对象** ⇒ ⇒ 裸 glob 排除会让它**永久免疫**）
  ⇒ ⇒ **禁**按目录/日期**自动归类**（快照写得越多 ⇒ ⇒ 判据强度单调下降 ⇒ ⇒ 恒绿）
  ⇒ ⇒ 新增排除 = 改判据 = **diff 可见 + 可评审**。
- ⚠️ **豁免 MUST 登记代价**：`.rddf/checks/arch-residual-registry.md` 登记一行
  「判据 (b) 对快照面**永久失明**（快照内的过期引用不被清零要求覆盖）」⇒ ⇒ 否则「豁免变遗忘」。
- ⇒ ⇒ 与 ADR-035 已有的排除口径**同型**（它已排除 ADR-035 自身）⇒ ⇒ **只扩口径，不改历史**。

---

## 5. 目前的任务是什么（**按可执行性排序**）

| # | 任务 | 归属 | 为什么是它（为什么现在） |
|---|---|---|---|
| **1** | **RC-8 收尾**：两个脚本 `chmod +x` + **可达性判据** | builder | CI 现在**只差这一条**红 ⇒ ⇒ 它是「让 CI 变绿」的**最后一环** ⇒ ⇒ 不做它，其余一切都在红信号下推进 |
| **2** | **CI 全绿验证**（**4 个 job**） | builder | 前 8 条红**全部**是 CI 侧 ⇒ ⇒ 本轮的核心交付 = **让 CI 重新可信**（红信号本身会掩盖后续所有回归）⇒ ⇒ ⚠️ 验收面 MUST 含 **`samples-regression` + `examples-runtime`**（RC-1 的修复在这两个 job）⇒ ⇒ ⚠️ `pr-verilator-fast` **仅 PR 事件**可达 ⇒ push 后**不验证** |
| **3** | **R14 = B3 实施**（两个 change） | builder | 🚪 门已闭 · 工件已齐 ⇒ ⇒ **主线**；且 MUST 在 R15 之前（时序金标只算一遍） |
| **4** | **R3 = P7 ②⑤**（可与 #3 并行） | builder → architect | 写点 `src/hierarchy_verilog.cpp` ⇒ ⇒ 与 #3 **函数级不重叠** ⇒ ⇒ 独立推进 |
| **5** | **R18 = R1 端口面残留登记** | architect | 台账在 `docs/architecture/**`（arch_paths）⇒ ⇒ **builder 不能写** ⇒ ⇒ 需切 architect ⇒ ⇒ ⚠️ **启动条件 = 用户对 `decision-request-port-face-2026-11.md` R1 的裁决** ⇒ ⇒ **不在 builder 序列控制内**（用户裁决迟来 ⇒ ⇒ 本项**静默悬置**，须显式标注） |
| **6** | **R21 侦察 → 立项** | builder(探针) → architect(立项) | 承 §4.2 裁决 ② ⇒ ⇒ 侦察 = **只读探针** ⇒ ⇒ **可与 CI 等待时间并行** |

⚠️ **#3 的实施粒度提醒**（不是裁决，是已识别的工程约束）：**T1（D-TCM 改 `sread`）单独落地会中间态红**
⇒ ⇒ 建议 **T1 + T2（A0 的三处强制结构变更）同 commit** ⇒ ⇒ 且 T2 的三处**同批同 commit**
（漏任一 ⇒ ⇒ **静默错值**、零报错 ⇒ ⇒ 只有判别性用例能抓）。

---

## 6. 后续要怎么做（**为什么**）—— 落点

| 任务 | 为什么（不做会怎样） | 归属 | 前置 |
|---|---|---|---|
| **R15 = B4 实施** | B4 改 ID/EX 级 ⇒ ⇒ 改时序金标 ⇒ ⇒ **必须等 B3 闭**（否则金标算两遍） | builder | **R14 闭** |
| **R3 = P7 ②⑤** | 三档 `PortPolicy` 在可合成形态上**未分化** ⇒ ⇒ ADR-060 v1.18 的契约**未落地** | builder → architect | — |
| **R18 = R1 端口面登记** | 扁平顶层 **80** 个内部 `ch_out` 暴露在顶层 ⇒ ⇒ 台账**无行** ⇒ ⇒ 缺口**不可见** | architect | 用户裁决（已立单） |
| **R21 侦察 → 立项** | 诉求 ④ 未兑现 ⇒ ⇒ 「RV32I 上没有成层示例」 | builder(探针) → architect | §4.2 |
| **R7 + R11 = FENCE** | 固件需要内存屏障语义 ⇒ ⇒ 但**语义裁决** planner 无权拍板 | planner → architect | R7 侦察 |
| **R16 = 独立立项 12 条** | 批次表 §4 的 11 行 + 台账 id=22 ⇒ ⇒ **逐项写点**（不可笼统写） | planner → architect | — |
| **R17 剩余 = §4「N3 补遗」** | 端口名棘轮的最后一个子项 | builder | — |
| **裁决 ③ 的回写动作** | 裁决 ③ 的排除面若只落在 `arch-residual-registry.md` + R23 行 ⇒ ⇒ **ADR-035 正文的判据字面**仍与将来的执行口径不一致 ⇒ ⇒ 下一个按字面执行 (b) 的人会**先踩红**再去翻豁免 ⇒ ⇒ ⇒ **双真相窗口** | architect（**待用户追认后**） | ⚠️ **本轮不擅自改已签批 ADR**（裁决 ③ = **待追认**的 architect 裁量 ⇒ ⇒ 写进 ADR = 把未决写成已决）⇒ ⇒ 动作 = 用户追认时按 **ADR-052 v2 的轻量修订形态**（**修订行 + 原位订正 + 审计轨迹**）补一条带日期后注 |
| **R9 = P11 逻辑算子物理移除** | `&&`/`\|\|` → `&`/`\|` 的**物理**移除 | builder | 🔒 日历锁 **2027-04-17**（本窗口不做） |

---

## 7. 判据自检（**可复跑**）

```bash
cd /workspace/project/CppHDL
# ① 护栏可达性（RC-8 的形态）= 裁决 ①-a 的**种子**（静态层 ⇒ 无逆递归）
#    ⇒ ⇒ 落地件 = scripts/verify_guard_reachability.sh（含投毒自检）
python3 - <<'PY'
import yaml, re, os, pathlib
d = yaml.safe_load(open('.github/workflows/ci.yml'))
bad = [m.group(1) for j in d['jobs'].values() for s in j['steps']
       for line in (s.get('run') or '').split('\n')
       for m in [re.match(r'^(scripts/[\w./-]+\.sh)\b', line.strip())] if m
       and os.path.exists(m.group(1)) and not os.access(m.group(1), os.X_OK)]
print('NOT EXECUTABLE:', bad or '无')
PY
# ② 两个 change 的 delta 自检
bash scripts/verify_openspec_delta.sh memory-timing-contract-2026-11 firmware-load-chain-2026-11
# ③ 删 stub 的代码面零命中
grep -rn 'set_ab_verification\|is_ab_verification\|ab_verification_\|run_ab_verify\|ab_result_a' include/ src/ | wc -l   # MUST = 0
# ④ golden 覆盖下限（集合判据 + 自检）
bash scripts/verify_jit_golden_coverage.sh --self-test
# ⑤ CI 现状（⚠️ MUST 看**结论**，不是「有 run」—— 「有 run」正是「存在 ≠ 在跑」的自指形态）
gh run list --limit 1 --json databaseId,status,conclusion
#    ⇒ ⇒ 验收口径 = **每个 job 的 conclusion == success**（不是 run 存在）
```

---

## 8. 与既有文档的关系

| 文档 | 关系 |
|---|---|
| `2026-11-13-phase-6-rtl-track-review.md`（复盘 #4） | **上游** —— 目标链的完整论证在它那里；本文件只补「瓶颈已从缺判据移到判据可达性」 |
| `.rddf/roadmap/phase-6-next-steps.md` | **下游**（planner 域）—— **任务内容 + 实施路径的权威载体**；本文件不复制状态格 |
| `.rddf/roadmap/phase-6-rtl-productization.md` | **批次编排**（B0→B4）—— B3 → B4 不得乱序的依据 |
| `docs/adr/ADR-035-verilator-backend.md` · `ADR-069-mem-data-alignment.md` | **已签批决策** —— 本文件**不反转**任何一条 |
| `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 + `.rddf/checks/arch-residual-registry.md` | **残差台账两侧** —— R18 / R3.5 要写它们（architect 域） |
