# openspec capability `## Purpose` 占位符 —— 8 个 capability 的复核审查

**日期**: 2026-11-19
**审查对象**: `openspec/specs/` 下 8 个 `## Purpose` 仍为占位符的 capability
**触发**: 用户指出这 8 条是 `openspec validate --specs --strict` 的红项之一（仓级既有问题，非本批引入）
**审查角色**: build（实施前的规范面复核）· 经 **oracle**（对抗式审查）+ **metis**（gap 分析）双审
**本报告性质**: 审查结论 + 修订建议。**未修改任何 spec 文本**（见 §2 的流程裁决项）

---

## 1. 结论摘要（一句话）

**8 段 Purpose 该补，但「补 Purpose」不是这批工作的主体** —— 这 8 个 capability 里**至少 6 个含与实现相反 / 不可复验 / 把一次性历史写成常驻义务的 Requirement**，只补 Purpose 会让 strict 红项看起来减少而**规范仍在说谎**（假绿）。**两条路径必须分开**：Purpose 补写（本仓已有先例，可直改主 spec）；过期 Requirement 修正（受「spec 变更必须走 change」管辖，逐条立项）。

---

## 2. 治理裁决：本批该怎么改（最关键，先看这里）

### 2.1 事实链（全部实测）

| # | 事实 | 证据 |
|---|---|---|
| G1 | CLI 要求**直改主 spec** | `openspec validate <cap> --strict` warning 原文：「Replace it with what this capability is for, **editing the main spec directly**: a `## Purpose` in a delta is read only when the capability is created, so it cannot replace this one.」 |
| G2 | **delta 机制上不含 Purpose** | 仓内 delta 实测格式 = `## ADDED Requirements` / `## MODIFIED Requirements`（`openspec/changes/archive/2026-11-19-rtl-verification-single-source-2026-11/specs/examples-rtl-sim-coverage/spec.md`）；仓内护栏脚本自己也这么写：`scripts/verify_openspec_delta.sh:112`「只看**会被并入主 spec**的部分：… （`## Purpose` **不会被合并** ⇒ 不算违规）」 |
| G3 | AGENTS.md **禁止**直改主 spec | `AGENTS.md`「OPENSPEC WORKFLOW」段：「Spec 变更必须走 change 流程：**直接修改 `openspec/specs/<capability>/spec.md` 是禁止的**」 |
| G4 | ⇒ **change 流程对 Purpose 是死路** | G1 + G2 ⇒ 走 change 无法修掉已存在 capability 的 Purpose |
| G5 | ⇒ **本仓已裁决过一次**，且已执行 | `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md:277`：「delta 的 Purpose 不合并 ⇒ ⇒ **归档后手工写**（不是 delta 的事，但 MUST 登记）」；`:345`「**Purpose 的两处落点**（delta **不合并 Purpose** ⇒ ⇒ 归档后 MUST 手工写）」；`:484` ⑨ 把「两处 capability Purpose 的归档后手工写」列为 **builder 的任务**（判据 = 归档后 `grep` 零命中） |
| G6 | 该先例**已落地** | commit `1952ba5`（2026-11-17）「… + T17 两处 Purpose」：T17 = 「`rtl-emit-portability`：占位符 → **实质 Purpose**」+「`rtl-hierarchical-output`：Purpose 改写」，与 archive **同 commit** 落库 |
| G7 | 但先例**只覆盖 Purpose**，不覆盖「顺带改过期 Requirement」 | G5/G6 的任务面都只是 Purpose；G3 的禁令本意是防「绕过 delta 改 Requirement」⇒ ⇒ **Purpose 与 Requirement 必须分两批** |

### 2.2 裁决结论

- **Purpose 占位符 → 直改主 spec**：**合规、有先例、可做**（G5 + G6）。这是 8 个 TBD 的最小成本修法。
- **过期 Requirement 的修正 → 仍受 G3 管辖**，逐条走 openspec change（MODIFIED delta），**不在本批混做**。
- **不需要新的 ADR**（G5 已是裁决）⇒ 但建议把「Purpose 无 change 通道 ⇒ 归档后手工写」这条**补进 `AGENTS.md`「OPENSPEC WORKFLOW」段**（当前 AGENTS.md 只有「Purpose 仍是占位符」的 CLI 漂移登记，没有「怎么修」的通道说明）⇒ 避免下一轮 review 重演 2026-11-16 的相反判定（`.rddf/reviews/rv32i-privilege-capability-2026-11.md` 的 oracle SHOULD_FIX-5「**不**手改主 spec ⇒ 留给下一个触碰该 capability 的 change 顺带填」—— 那条针对的是「change 正在跑时顺手填」，不是「独立维护批直写」）。

### 2.3 范围裁决

| 批次 | 内容 | 归属 | 判据 |
|---|---|---|---|
| **批 A（本批）** | 8 段 `## Purpose`（纯散文，**零 Requirement 文本改动**） | 直改主 spec（G5 先例） | §5 的 AC-1…AC-7 |
| **批 B（逐条立项）** | 过期 / 矛盾 / 未实现的 Requirement 修正 | openspec change + MODIFIED delta | 每条一个 change，AC 下沉到 requirement 级 |
| **批 C（登记）** | 范围外发现（见 §8） | 登记表 / improvement | 不在本批修 |

---

## 3. 事实基线

### 3.1 strict 校验现状（2026-11-19 实测，CLI 1.14.3）

仓级：`Totals: 23 passed, 28 failed (51 items)`。

8 个 target 的逐项 warning（**注意：只有 4 个在补完 Purpose 后会翻绿**）：

| capability | Purpose 占位符 warning | 另有 `>500 字符` warning | 补完 Purpose 后 |
|---|:-:|---|---|
| `build-cpp-standard` | ✅ | — | **翻绿** |
| `jit-llvm-compat-tests` | ✅ | requirements[0][1][2] | 仍红 |
| `rv32i-privilege` | ✅ | requirements[0] | 仍红 |
| `verilator-backend-e2e` | ✅ | requirements[6][7][8] | 仍红 |
| `verilator-e2e-coverage` | ✅ | requirements[4] | 仍红 |
| `verilator-error-flow` | ✅ | — | **翻绿** |
| `verilator-hier-input` | ✅ | — | **翻绿** |
| `verilator-reg-init` | ✅ | — | **翻绿** |

⇒ ⇒ **「红项减 8」是伪 AC**（metis finding）。真实效果 = **23→27 passed / 28→24 failed**（51 items 不变）。
⚠️ `rv32i-privilege/spec.md:10-43` 单条 Requirement 正文已 >500 字符 ⇒ 补完 Purpose 后该 spec **仍红**（红因换成字符数）⇒ ⇒ 拿仓级 totals 当 AC 会误判为「没修好」。

### 3.2 护栏真空

- `scripts/verify_openspec_delta.sh:75-88` **只扫 `openspec/changes/` 下的 active change**；零 active change ⇒ exit 0 且脚本自述「ZERO signal」⇒ ⇒ **主 spec 无人看守**。
- CI **不跑** `openspec validate`（AGENTS.md 已载明）。
- `scripts/` 下引用 `openspec/specs` 的仅 3 个脚本（`verify_chlib_dtor_composability.sh` / `verify_no_duplicate_rv32i_headers.sh` / `lib/dual_backend_parity.py`），**全部是「把 spec 当数据源」**，没有一个对 spec 做质量门。
- ⇒ ⇒ **没有任何机械判据在守这 8 个 capability 的 Requirement 真值** ⇒ ⇒ 规范里的假话可以长期存活（本次实测到 6 处）。

---

## 4. 逐 capability 审查结论

### 4.1 总表

| capability | 正确性 | 合理性 | 是否过期 | 过期项数 | 处置归属 |
|---|---|---|---|:-:|---|
| `build-cpp-standard` | ⚠️ 事实对，约束形式错 | ⚠️ | **是** | 3 | 批 B（1 条降级为 NOTE）|
| `jit-llvm-compat-tests` | ✅ 实现一致 | ⚠️ Req 3 属**过程日志** | **是** | 1 | 批 B（Req 3 删）|
| `rv32i-privilege` | ✅ **实现与规范逐拍吻合** | ⚠️ 名实落差 | 否（内容）| 0 | 批 A（Purpose 补边界句）|
| `verilator-backend-e2e` | ❌ **多处与实现相反** | ❌ | **是** | 6 | 批 B（改动最大）|
| `verilator-e2e-coverage` | ❌ **内部两条 Requirement 互斥** | ❌ | **是** | 4 | 批 B |
| `verilator-error-flow` | ✅ 类型/变体/测试全对 | ⚠️ Req 5 三载体矛盾 | **是** | 1 | 批 B |
| `verilator-hier-input` | ⚠️ 行为对，**判据写错（危险方向）** | ⚠️ | **是** | 2 | 批 B |
| `verilator-reg-init` | ✅ 实现全部落地 | ✅ | 否（仅告警文案）| 1 | 批 B（低成本）|

### 4.2 逐条过期项（判据 + 建议处置）

**判据（本批通用）**
- **D1**：Requirement MUST 今天就可被**机械复验**（存在守卫测试 / grep / 脚本）⇒ 不可复验 ⇒ 是 aspiration ⇒ 移出 Requirement。
- **D2**：Requirement MUST NOT 引用**过程产物**（归档 change 的 `tasks.md`/`design.md`、`git diff HEAD~1`、某次 validate 的输出数字）⇒ 那些住在归档区。
- **D3**：Requirement 写**行为**；机制仅在与实现逐字一致时才允许出现（且接受它会过期）⇒ 规范与实现冲突时，**以「测试 + ADR 历史」背书的一方为准**改另一方，并留 dated 订正注记（范式：`rv32i-privilege/spec.md:60-62`「⚠️ 标题保留为可追溯性（openspec MODIFIED 语义要求 Scenario **同名**）⇒ ⇒ **判据以本 Scenario 正文为准**」）。
- **D4**：Scenario 标题被 MODIFIED 逐字匹配锁定时 ⇒ 保留标题 + 注记，正文写可判形态。
- **D5**：`MUST NOT` 为迎合过期规范而改代码（那是 `bf78367` 同型事故）⇒ 规范改，不改码。

| # | capability | 位置 | 现状（实测） | 建议 | 归属 |
|:-:|---|---|---|---|---|
| 1 | `verilator-backend-e2e` | `spec.md:9` | 「dlsym 解析 **5 个**符号」紧接着列 **7 个**；Scenario `:11` 又写「7 符号全部成功解析」；实现 `src/core/verilator_backend.cpp:822-834` **确为 7 个**；守卫测试 `tests/test_verilator_backend.cpp:522` `E2E DlopenSevenAccessors` | 改 5→7 | 批 B |
| 2 | 同上 | `spec.md:9` | 「在 `verilator --cc --build` 成功生成 `obj_dir/Vtop` 后」⇒ 实现已按 ADR-035 §M1.0（R1 修复）改两阶段：`verilator --cc` + `g++ -shared -fPIC -o libVtop.so`（`src/core/verilator_backend.cpp:748-797`）⇒ `obj_dir/Vtop` **不存在** | 改写为两阶段形态 | 批 B |
| 3 | 同上 | `spec.md:54` | 「检测 `~/.cache/cpphdl/verilator/<key>/Vtop` 存在」⇒ 实现缓存的是 **`libVtop.so`**（AGENTS.md VERILATOR BACKEND RULES 同款） | 改 `Vtop`→`libVtop.so` | 批 B |
| 4 | 同上 | `spec.md:67-77` | 「`enable_vcd(true)` 后**每个 cycle** 写入 `obj_dir/sim.vcd`」⇒ 全仓 `dump_vcd` **只有定义**（`src/core/verilator_backend.cpp:1008`）与**单测手动调用**（`tests/test_verilator_backend.cpp:500`）⇒ **生产路径每拍从不调用**（`examples/verilator/02_counter_with_input.cpp:51` 只调 `enable_vcd`）⇒ **该 Requirement 未被实现满足** | 改写为「显式调用 `dump_vcd(t)` 写出合法 VCD」（守卫 `VcdDumpWritesFile`）；「每拍自动」⇒ 若要则**独立立项**（这是**实现缺口**，不是文案问题） | 批 B + 缺口登记 |
| 5 | 同上 | `spec.md:79-89` | 引用**跨仓** `ChipForge tests/cpu/test_cpu_chmem_vendored_elf.cpp` 与 `rv32ui-p-add` ELF ⇒ 本仓**既无该文件也无该 ELF**（`find . -name "*.elf"` 零命中）⇒ **不可在本仓复验** | 改指本仓可复验形态（`verilator-rv32i-e2e` + `scripts/rv32i_rtl_sim_env.sh` 的 `checks=24 fails=0`） | 批 B |
| 6 | 同上 | `spec.md:183-197` | 「MUST 实施 **3 步**时钟序列（**exactly 3 calls** to `eval_fn_()` with clk=0/1/0）」⇒ 实现 `eval_sequential` 是 **2 步**（`*clk=1; eval; *clk=0; eval`）；AGENTS.md「VERILATOR BACKEND RULES」明写「**2-step clock toggle**」；守卫测试 `tests/test_verilator_backend.cpp:586` `TwoStepClockToggleIncrementOnePerTick` | 改写为 2-step + dated 订正注记（D3/D4 范式）+ 点名守卫测试。⚠️ 需求 #4「1 cycle = 3 次 eval」**仍成立**（`src/simulator.cpp:319-341`：组合 1 + eval 2 + 组合 1；守卫 `ThreeEvalTickClockModel`）⇒ 两者**不矛盾**，别一起改 | 批 B |
| 7 | `verilator-e2e-coverage` | `spec.md:162-169` | 「MUST 追加调用 `popen("verilator --lint-only -Wno-fatal <tmp>.v 2>&1")`」**与** `:184-186`「MUST 共享 `tests/external_tool_helpers.h` 的 helper」**并存且互斥**（实现已改用共享 helper `ch_test::lint_verilog()`，`tests/external_tool_helpers.h:97`）⇒ **规范自相矛盾**（非过期） | MODIFIED `:162` 为「共享 helper 形态」⇒ **本项优先级最高**（内部矛盾）。「≥8 个用例真 lint」这条**已满足**（arith 2 + compare 1 + shift 2 + gen 3）⇒ 保留行为判据 | 批 B（最高优先）|
| 8 | 同上 | `spec.md:199-218` | 「误导命名的 verilator 测试 MUST 改名」⇒ **已完全落地**（`tests/test_codegen_literal_mask.cpp` 存在 + `tests/CMakeLists.txt:114-116` 保留 `test_verilator_three_way` alias）⇒ 一次性动作留在常驻规范 = 纯历史 | 删（历史归归档目录）| 批 B |
| 9 | 同上 | `spec.md:237-245` | 「8 阶段 tasks.md MUST 包含全部覆盖补足」+ Scenario 要求阅读**归档 change** 的 `tasks.md` ⇒ 纯过程日志（D2）| 删 | 批 B |
| 10 | 同上 | `spec.md:247-251` | 断言「`openspec validate --specs --strict` 输出 **8/8 passed**」⇒ 实测仓级 `23/28/51` ⇒ **已可观测为假**（D2）| 删 | 批 B |
| 11 | 同上 | `spec.md:95` | 引用 `examples/axi4/axi4_full_example.cpp` ⇒ **该文件不存在**（`examples/axi4/` 实测只有 `axi4_lite_example_top.h` / `axi_soc_example_top.h` / `tb_*`）⇒ 悬空引用 | 改指现存文件 | 批 B |
| 12 | `verilator-error-flow` | `spec.md:52` + `:56` | 三载体不一致：正文「每个变体 SHALL 在 **`design.md`** 中标注」/ Scenario「每个变体上方有 `/// @adr ADR-035 §5 R<n>` 注释，n ∈ {1,5,8,10}」/ 实现 `include/core/verilator_backend.h:61-68` = **`// @adr`** × **5 个 `no-ADR`**（VerilogGeneration / PortBitwidthOverflow / PortAccessTable / FileIO / CacheWriteback）⇒ 规范要求的注释形态**不存在** | 二选一：① 对齐实现（3 个有 ADR 引用 + 5 个标 `no-ADR`）——**推荐**；② 删该 Scenario（无测试在守，且 `design.md` 是归档目录里的文件 ⇒ 不可复验）| 批 B |
| 13 | `verilator-hier-input` | `spec.md:8` | ① 「`print_header()` MUST filter」⇒ `print_header` 早已不扫 ctx（端口表来自 `ModuleView::ports`），过滤点在 `src/codegen_verilog.cpp:484` 与 `src/hierarchy_analysis.cpp:410` ⇒ **机制描述过期**；② 「（等价 `num_srcs() > 0`）」⇒ **事实错误且方向危险**：`src/hierarchy_analysis.cpp:405-411` 明写「『有 driver』的判据 MUST 用 `inputimpl::driver()`（**不是** `num_srcs()`）：`ch_in` 节点**自身**的 `num_srcs()` 恒为 0（驱动落在 **proxy** 上）⇒ ⇒ 用 `num_srcs()` 会把**每一个**被正常驱动的子组件端口误报成未驱动」⇒ ⇒ **按规范改代码会把合法设计判死** | 改为**只描述行为**（有 driver 的 `ch_in` 不出顶层 port）+ **删除「等价 `num_srcs() > 0`」** | 批 B（**危险项，优先**）|
| 14 | `jit-llvm-compat-tests` | `spec.md:66-89` | Req 3「MUST NOT 引入任何超出以下 4 项的构建/CI 矩阵改动」+ Scenario「`git diff --stat HEAD~1 HEAD` 只显示这 4 个文件」⇒ **把某一次 commit 的 diff 快照写成常驻规范** ⇒ 任何后续触碰这些文件的 change 都「违反」它 ⇒ 恒假 | 删该 Scenario；若「不扩 CI 矩阵」的意图要保留 ⇒ 改写为**行为判据**（「新增 JIT 测试 MUST NOT 引入 `JitCompiler` 现有范围之外的 LLVM 依赖」⇒ 可用 CMake 列表 / `ldd` 机械验）| 批 B |
| 15 | `build-cpp-standard` | `spec.md:17` | 「**25 个**二级 CMakeLists 同步」⇒ 实测（排除 `build/` 与 `third_party/`）：项目级 CMakeLists 共 **30** 个（根 1 + 二级 26 + `external/nameof` 3）；其中带 `CXX_STANDARD`/`cxx_std_2` 的是 **24** 个（含根）⇒ **二级 = 23 个** ⇒ 「25」已过期 | **不是** 25→23（下一轮又漂）⇒ 改写为**继承判据**（根 `CMAKE_CXX_STANDARD` + `CXX_STANDARD_REQUIRED ON` + `CXX_EXTENSIONS OFF` 三元组为唯一权威；子目录 **MUST NOT** 覆写）+ 一个枚举守卫 | 批 B |
| 16 | 同上 | `spec.md:21-30` | 「8 个 C++23 破坏点的审计结论 SHALL 记录在 commit message 或 design.md 中」⇒ **一次性历史动作**（git 历史事实）⇒ 作为常驻 Requirement **不可复验**（D1）⇒ 降级为 Purpose 里的一句 NOTE（证据已固定在归档 change）| 降级 | 批 B |
| 17 | 同上 | `spec.md:33` | 把 6 个**具体文档路径**硬编码成义务 ⇒ 新增文档即「违规」⇒ 改为「四类载体」（规范 / 构建 / 入门文档 / 架构文档）不点具体文件 | 改写 | 批 B |
| 18 | `verilator-reg-init` | `spec.md:56` | Scenario 引用的告警文本 `[CHWARN] ch_reg '<name>' init_node is not type_lit (type=N)` ⇒ 实现实际文本 `[print_reg] ch_reg init_node is not type_lit (type=%d, size=%u, name=%s); reset branch falls back to 1'b0`（`src/codegen_verilog.cpp:1178-1181`）⇒ **不一致** | 逐字对齐（或改判据为「MUST 产出含 `init_node is not type_lit` 的告警」⇒ 抗文案漂移）| 批 B（低成本）|
| 19 | `rv32i-privilege` | 全篇 | **无过期项**。实现与规范**逐拍吻合**（`include/cpu/riscv/rv32i_exception.h` 的「拍 1 写 mepc + 锁存 cause / 拍 2 写 mcause」；`CsrBank` 单写口 `csr_addr/csr_wdata/csr_write_en`）| — | 批 A（只补 Purpose）|

### 4.3 命名问题：`rv32i-privilege` 名实落差 —— **建议不改名**

- 规范正文**已有**显式范围声明（`spec.md:28-43` 逐条列出 `mtval` / `mtvec` / `mstatus` / `MRET` / 并发 5 项排除 + 理由 + 与冻结语义的接缝声明）⇒ 「名实不符」**只在读者不读正文时成立**。
- **改名代价**：引用面 ≥6 处（`.rddf/roadmap/phase-6-next-steps.md` · `.rddf/improvements/rv32i-privileged-csr-2026-11.md:146` · `.rddf/improvements/rv32i-isa-gap-closure-2026-11.md` · `.rddf/reviews/rv32i-privilege-capability-2026-11.md` · `AGENTS.md` · `include/cpu/AGENTS.md`）；且 openspec **无 capability rename 机制**（只有 delta 内的 `## RENAMED Requirements`）⇒ 改名 = 手工 `mv` + 改 6 处 + **让已登记的扩展计划失效**。
- ⇒ **处置**：**不改名**（**最低成本 = Purpose 里加边界句**，见 §6.3）；真正覆盖 CSR 指令时再一并考虑。

---

## 5. 批 A 的验收判据（AC）

**AC-0（否决性）**：**不以仓级 strict totals 当 AC** ⇒ 修完 8 段 Purpose 后 `rv32i-privilege` / `jit-llvm-compat-tests` / `verilator-backend-e2e` / `verilator-e2e-coverage` **仍红**（`>500 字符`）⇒ totals = **27 passed / 24 failed / 51 items**（不是 31/20）。

| # | AC | 判定方式 |
|:-:|---|---|
| AC-1 | 8 个 target 的 **Purpose 占位符 warning 清零**（per-item，不看 totals） | 对 8 个 target 逐个 `openspec validate <c> --type spec --strict` ⇒「Purpose section is still a placeholder」**零命中** |
| AC-2 | Purpose 段非占位符（**与 CLI 文案解耦**，防 CLI 改措辞失明） | 8 个文件的 `## Purpose` 段：长度 ≥ 120 字符 ∧ 不含 `TBD`/`TODO`/`placeholder`/`created by archiving` |
| AC-3 | **信息量下限**（防「Requirement 标题拼接」）| Purpose 段与该文件任一 `### Requirement:` 标题的**最长公共「实词片段」MUST < 4 个实词**（实词 = 剔除 `MUST`/`NOT`/`SHALL`/冠词/介词/标点后的词元；**按连续片段**计）⇒ ⇒ ⚠️ 阈值 **10 字符**的旧版判据会把「 MUST NOT 」这类**功能词碎片**误判 ⇒ ⇒ 已于 2026-11-19 改为实词计数（见 §11.8）| Purpose 段长度 ≥ 120 字符 |
| AC-4 | **纯散文改动**（不夹带 Requirement 改动）| `git diff -U0` 中 8 个文件**只在 `## Purpose` 块内**有 `+` 行；`grep '^[+-]### Requirement'` 零命中 |
| AC-5 | **指针对得着**（防悬空引用扩散到第一屏）| Purpose 中出现的反引号路径 `test -e` 必须成立；出现的符号必须在 `include/`/`src/`/`tests/`/`examples/` 有定义 |
| AC-6 | **不破坏既有门** | `openspec validate --specs`（**非 strict**，ADR 判据面的人工 MUST）仍 exit 0；`bash scripts/verify_openspec_delta.sh` 仍绿 |
| AC-7 | **簿记一致** | `AGENTS.md:479-482` 的「只登记、不消除」口径**已更新**（补：Purpose 占位符已清零 + 剩余红项仅 `Requirement >500 字符`）；快照数字从 `21 passed / 23 failed (44 items)` **订正**为实测 |

---

## 6. 批 A 的 8 段 Purpose 草稿

**写法约定**（对齐 ADR-072 T17 已落地的写法 + 本仓健康 Purpose 样例）：
- 三段式 = **覆盖什么（行为面）** → **为什么存在** → **边界（不覆盖什么 / 承接者）**；
- **不逐条复述 Requirement 标题**（AC-3 会抓）⇒ 不写成「本 capability 定义 X、Y、Z」；
- **不点名易过期的内部函数**（除非它就是被规范的**公开契约**）⇒ 「机制」归 Requirement，「存在理由 + 边界」归 Purpose；
- 不写「我们修过什么」（一次性事件不写进常驻目的）；
- 允许出现 `MUST`（本仓既有健康 Purpose 有此用法，如 `ledger-residual-registry`），但 **Purpose 不承担新的可判定义务**（避免第二规范面）。

### 6.1 `build-cpp-standard`

> 定义 CppHDL 的 **C++ 语言基线**契约 —— 整个仓库（库 / 示例 / 测试 / benchmark）MUST 以 **C++23** 为唯一的 C++ 标准基线，且**不得**由任何子目录单独降级或改写。
>
> - **唯一权威**：根 `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD` / `CMAKE_CXX_STANDARD_REQUIRED` / `CMAKE_CXX_EXTENSIONS` 三元组；子目录 CMakeLists **MUST NOT** 覆写这三项（⇒ ⇒ 覆写即视为违反本 capability）。
> - **为什么存在**：C++ 标准差异会改变**语言语义**（`char8_t` / `auto{x}` / `consteval` 收紧）⇒ ⇒ 同一份源码在不同子目录用不同标准编译 ⇒ ⇒ 「能编过」与「行为一致」不再等价 ⇒ ⇒ 标准基线必须有单点权威。
> - **边界**：本 capability **不**管辖具体特性的可用性（`std::expected` 的平台限制 ⇒ 归 `verilator-error-flow`）⇒ **不**管辖第三方代码（`third_party/**` / `external/**` 保留其自带标准）⇒ **不**管辖「升级到新标准时的破坏点审计」——那是**一次性动作**，其结论保留在归档 change 中。

### 6.2 `jit-llvm-compat-tests`

> 定义 CppHDL **JIT 后端与 LLVM 版本之间的兼容性红线**契约 —— 仓库必须携带一组**可判定的版本探测测试**，使「换一个 LLVM 大版本会不会静默把 JIT 打坏」这件事在构建矩阵里**可见**，而不是等到某个设计跑不出结果时才发现。
>
> - **覆盖**：① **版本区间判定**（当前受测区间 = LLVM 17–22）⇒ ⇒ 区间内 JIT **MUST** 可用；区间外 **MUST** 显式 SUCCEED 并**报出版本号**（⇒ ⇒ 「未来版本需要显式 opt-in」而不是「静默失败」）；② **构造期不崩**（构造 + 可用性报告）；③ **关闭 JIT 的构建矩阵不受影响**（该配置下这些测试 **MUST NOT** 链接 JIT 实现）。
> - **为什么存在**：LLVM 的 C 风格 `LLVMInitializeNative*` 家族在大版本间被移除 ⇒ ⇒ 故障只在**编译期**暴露 ⇒ ⇒ 需要一个**每次构建都跑**的红线测试，把「编译不过」变成一条明确的红线。
> - **边界**：本 capability **不**管辖 JIT 的**性能**（⇒ 归 perf 基线）⇒ **不**管辖 JIT 的**正确性**（⇒ 归 golden 测试）⇒ **不**规定「JIT 应当支持哪些 LLVM 版本」这个**产品决策**（⇒ 属 roadmap）；只要求「区间判定 + 区间外的显式 opt-in 机制」存在。

### 6.3 `rv32i-privilege`

> 定义 RV32I **机器级陷阱入口的异常上下文写入**契约 —— 陷阱被接收时，`mepc`（异常指令 PC）与 `mcause`（异常原因码）MUST 以**同一次异常的一对**被写入 CSR 存储体，并且 MUST 可从 CSR 存储体**读回**。
>
> - **为什么存在**：陷阱上下文一旦丢失或**配对错位**（`mcause` 写的是另一次异常的原因码），软件侧就**无法**判定「为何陷入、陷入在哪」⇒ ⇒ 缺陷发生在**硬件层**却**只由软件可见** ⇒ ⇒ 必须在组件级把「写入 + 配对 + 读回」钉死。
> - **覆盖**：写入的**两拍配对**语义（陷阱当拍写 `mepc`、其后一拍写 `mcause`）+ 从 CSR 存储体**读回**的闭环验收 + `exception_valid` 为**电平**时的重复写入行为。
> - **⚠️ 边界（名与实的落差）**：本 capability 名字带 `privilege`，但**只覆盖陷阱入口的异常上下文写入**；**不**覆盖 CSR 指令语义（`csrrw`/`csrrs`/`csrrc`）· `ECALL`/`EBREAK` · `mtval` · `mtvec` 取值语义 · `mstatus` 的陷阱侧更新（`MIE`↔`MPIE`）· `MRET` 的 PC 恢复 · 并发异常的排队语义 ⇒ ⇒ 这些**当前不存在**，属独立立项（登记在 `.rddf/improvements/rv32i-privileged-csr-2026-11.md`）。
> - **接缝**：本 capability **不**改变 `rv32i-isa-semantics` 的「未知 / 未实现 opcode ⇒ 流水线级冻结」条款 ⇒ ⇒ 未来把陷阱入口接进流水线时，**MUST** 以 MODIFIED delta 修订那条冻结条款（触发信号已登记）。

### 6.4 `verilator-backend-e2e`

> 定义 **Verilator 后端与 CppHDL 解释器在端到端行为上的对齐契约** —— 同一份设计分别跑解释器与 Verilator 仿真时，两者在**可观测顶层端口**上 MUST 给出一致的结果。
>
> - **覆盖**：① **符号解析**（后端 MUST 真实 `dlopen` 生成的共享库并解析出**全部 7 个**入口符号，缺一 MUST 优雅失败并报出缺失符号名）⇒ ⇒ ② **数据同步**（设计状态与仿真模型之间的双向读写 MUST 是真的读写，不是日志）⇒ ⇒ ③ **时钟模型**（1 cycle MUST 等于 3 次求值调用：组合求值 1 次 + 时钟沿相关求值 2 次）⇒ ⇒ ④ **缓存**（同一份设计的第二次初始化 MUST 命中缓存，MUST NOT 重跑编译子进程）⇒ ⇒ ⑤ **字面量宽度**（生成的 Verilog 字面量 MUST 落在目标 wire 宽度内 ⇒ ⇒ 否则 Verilator 报 `Too many digits` 硬错）⇒ ⇒ ⑥ **输出传播**（组合 `assign` 与 `always_ff` 两条路径的值 MUST 都回到可观测状态）。
> - **为什么存在**：Verilator 后端是**第二条执行路径** ⇒ ⇒ 它与解释器不一致时，缺陷会在「只在 Verilator 跑」的交付面上暴露 ⇒ ⇒ 本 capability 是那条路径的一致性锚点。
> - **边界**：本 capability **只管「两后端一致」**；**不**规定 Verilator 的**具体建模方式**（时钟序列 / 缓存键算法 / 生成命令属实现细节，会随 ADR-035 演进 ⇒ ⇒ 本规范只钉**可观测行为**）⇒ **不**覆盖「覆盖率的充分性」（⇒ 归 `verilator-e2e-coverage`）⇒ **不**覆盖「失败怎么被报告」（⇒ 归 `verilator-error-flow`）。

### 6.5 `verilator-e2e-coverage`

> 定义 **Verilator 双后端仿真的覆盖面在 CTest 矩阵中可见**的契约 —— 「哪些设计类别被双后端仿真覆盖了」这件事 MUST 在 `ctest -N` 里**一眼可见**，而不是只藏在某个测试文件里。
>
> - **覆盖**：① **类别维度**（trace parity / bundle / stream / memory / 复杂设计（流水线 + FSM）各自 MUST 至少有一组可执行用例）⇒ ⇒ ② **命名维度**（误导性的测试名 MUST 被纠正，且旧名 MUST 保留为 CTest alias 以免打断历史命令）⇒ ⇒ ③ **判据维度**（每条覆盖 MUST 同时有**双后端 parity 断言**与**手推的绝对语义金标** ⇒ ⇒ 两端由**同一份** fixture 生成 ⇒ ⇒ parity 对「同构缺陷」**结构性失明** ⇒ ⇒ 金标不可省）⇒ ⇒ ④ **判别力前提**（字节序方向 MUST 用**非回文**字节 ⇒ ⇒ 回文字节使「位序」这一维度零判别力）。
> - **为什么存在**：覆盖不足在 CI 上是**静默**的（少一个测试，ctest 照样全绿）⇒ ⇒ 必须让「覆盖了什么」成为**可枚举**的矩阵，而不是靠人记。
> - **边界**：本 capability **不**规定「具体覆盖了哪些 DUT」（⇒ 那会立刻过期）⇒ ⇒ 只规定**维度**；**不**管辖「两后端一致性的深度」（⇒ 归 `verilator-backend-e2e`）⇒ **不**管辖环境矩阵本身（平台范围 / CI 拓扑）。

### 6.6 `verilator-error-flow`

> 定义 **Verilator 后端失败时的错误传递契约** —— 后端的每一个失败点 MUST 以**结构化错误**（机器可判的错误码 + 人类可读消息 + 源码位置）返回给调用方，而不是以布尔值 + 散落的日志存在。
>
> - **覆盖**：① **错误类型契约**（错误码变体集合 + 消息 + 源码位置三元组）⇒ ⇒ ② **失败点覆盖面**（后端的**全部**失败路径 MUST 走同一条结构化通道，MUST NOT 有「某个失败点只返回 false 而不携带原因」的漏网）⇒ ⇒ ③ **向后兼容**（公共初始化入口仍返回布尔 ⇒ ⇒ 旧调用点 MUST NOT 被迫改写）⇒ ⇒ ④ **风险可追溯**（错误码与已登记风险项的对应关系 MUST 可见；**不**对应任何已登记风险的错误码 MUST 显式标注「无对应风险」）。
> - **为什么存在**：后端失败在流水线里最常见的形态是「初始化返回 false ⇒ 测试 SKIP ⇒ 全绿」⇒ ⇒ 失败原因一旦不可见，**真实故障会被伪装成「环境不支持」** ⇒ ⇒ 错误码 MUST 可被断言。
> - **边界**：本 capability **只管「失败怎么被报告」**；**不**规定「失败之后该怎么办」（重试策略 / 是否致命 ⇒ 属调用方）⇒ **不**管辖 Verilator 工具链自身的诊断输出。

### 6.7 `verilator-hier-input`

> 定义 **层次化设计中「内部被驱动的输入端口」不得泄漏为顶层端口**的契约 —— 一个 `ch_in` 若在设计内部已经有了驱动（来自父层或同层信号），它 MUST NOT 出现在生成的顶层 Verilog 端口表里；它 MUST 退化为一条内部连线，其值由那条驱动决定。
>
> - **为什么存在**：泄漏会把**内部信号**误升级为**外部契约**（外部 TB 会看到一个本不该存在的端口）⇒ ⇒ 更糟的是生成侧与仿真侧对这个端口的认知会分叉（一边当作顶层输入等外部驱动、一边当作内部连线）⇒ ⇒ 子模块的输入**永远拿不到值** ⇒ ⇒ 表现为「下游恒为未驱动 / 恒 0」。
> - **覆盖**：① **判据口径**：判断「有无驱动」MUST 用**端口自身的驱动关系**判定（`ch_in` 节点自身的连线数**恒为 0**，驱动落在代理节点上 ⇒ ⇒ 用连线数判会把**每一个**被正常驱动的子端口误判成未驱动）⇒ ⇒ ② **两侧一致**：生成侧（端口表 + 内部连线 + 仿真主程序的端口写表 + 端口访问表）MUST 用**同一套**过滤口径 ⇒ ⇒ 任何一处漏过滤都会让仿真侧引用一个**不存在**的端口字段。
> - **边界**：本 capability **只管「被驱动的输入端口的可见性」**；**不**管辖「未驱动输入端口的处置」（⇒ 归层次化输出契约）⇒ **不**管辖端口命名规则（⇒ 归端口命名契约）。

### 6.8 `verilator-reg-init`

> 定义 **Verilator 后端与解释器在寄存器初值上的对齐契约** —— 带非零初值的寄存器，在两个后端上 MUST 从**同一个初值**开始，且**复位**语义 MUST 一致。
>
> - **覆盖**：① **初值对齐**（`ch_reg` 的初值 MUST 经「一次复位脉冲」载入仿真模型 ⇒ ⇒ 否则模型从 0 起、解释器从初值起 ⇒ ⇒ 首个观察点就分叉）⇒ ⇒ ② **复位脉冲的单一实现**（初始化路径与复位路径 MUST 复用同一套脉冲序列 ⇒ ⇒ 两条路径各写一遍是漂移的来源）⇒ ⇒ ③ **字面量宽度**（复位分支的字面量 MUST 按寄存器宽度归一 ⇒ ⇒ 否则窄宽字面量会让复位值随字面量写法漂移）⇒ ⇒ ④ **幂等**（重复初始化 MUST NOT 产生累积副作用）。
> - **为什么存在**：初值分叉是**静默**的 —— 首个差异往往出现在「初值非零的设计」上，而这类设计在测试里通常不占多数 ⇒ ⇒ 不钉住就会在集成期才暴露。
> - **边界**：本 capability **只管「寄存器初值 / 复位」这一条对齐**；**不**管辖「复位后的功能行为」（⇒ 归各组件自己的 capability）⇒ **不**管辖「复位信号本身的产生方」（⇒ 归 SoC / 组件层）。

---

## 7. 护栏建议（防复发）

**问题**：新建 capability 归档时 CLI 会自动写占位符 ⇒ ⇒ **「全仓 `grep TBD` 零命中」的单向判据会在下一次归档时转红** ⇒ ⇒ 不能做单向判据。

**建议形态**（本批可一并落，或随批 B 落）：
- `scripts/verify_openspec_purpose.sh`（fail-closed：脚本缺失 / 0 行 ⇒ exit 2，**禁恒 0**）：
  - **R1 双向**：占位符 capability 集合 == 登记表 `.rddf/checks/openspec-purpose-placeholders.tsv` 的行集合（**双向**，不是单向）；
  - **R2**：登记表每行有 `owner` / `why_open` / `source_change`；
  - **R3**：`--self-test` 投毒（把一个实质 Purpose 换回占位符 ⇒ 必红；登记表插一行不存在的 capability ⇒ 必红）；
  - **R4**：可选第二条 —— 主 spec **MUST NOT** 引用 `openspec/changes/archive` 路径（抓「过程日志类 Requirement」复发）。
- ⚠️ **不要**把「全仓零 TBD」做成 CI 门（见上）。

---

## 8. 范围外发现（登记，不在本批修）

| # | 发现 | 证据 | 建议归属 |
|:-:|---|---|---|
| O1 | `tests/test_verilator_backend.cpp:480-486` 残留陈旧注释「the implementation has **no cache write-back path**」⇒ 与现 `writeback_to_cache()`（`src/core/verilator_backend.cpp:1147`）相反 | 实测 | 随批 B 的 #3 一并修 |
| O2 | `src/core/verilator_backend.cpp` 内部注释自相矛盾：`:445` 写「the **2-step** clock toggle」vs `:917` 写「the **3-step** clock toggle in eval_sequential」 | 实测 | 随批 B 的 #6 一并修（**注释也是机制描述 ⇒ 同样会漂**）|
| O3 | `verilator-backend-e2e` 的 VCD「每拍自动 dump」是**实现缺口**（不只是文案过期）⇒ 若要真做 = 新增生产路径调用点 | 实测 | 独立 improvement（= 「实现缺口」而非「文档债」）|
| O4 | `rv32i-privilege` 改名的长期选项（留到真正覆盖 CSR 指令时再一并改） | 引用面 ≥6 处 | improvement `rv32i-privileged-csr-2026-11` 内决策 |
| O5 | `AGENTS.md:479-482` 的 strict 快照数字（`21 passed / 23 failed (44 items)`）**已陈旧**（实测 `23/28/51`）⇒ 与本次同 commit 订正 | 实测 | 批 A 的 AC-7 |
| O6 | `openspec/config.yaml` 的 `rules.specs` 只有 3 条 ⇒ **仓内没有任何「Purpose 该怎么写」的规范** ⇒ ⇒ 建议把写法约定落到 `AGENTS.md`「OPENSPEC WORKFLOW」段（config 的 rules 无执行者 ⇒ 写了也是装饰） | 实测 | 批 A 或批 B |

---

## 9. oracle / metis 审查记录与逐条应答

### 9.1 oracle（对抗式审查 · 审查域 = 其他审查）

**判定行**：`判定: REJECT`（对「8 个 capability 的 Requirement 现状」与「只补 Purpose」的窄改法）—— 依据：① 8 个中 ≥6 个含与实现相反或不可复验的 Requirement，只补 Purpose = strict 部分转绿而规范持续说谎的**假绿**；② 流程冲突未裁决前，两条执行路径分别是「治理静默违规」与「归档成功但 Purpose 仍是 TBD」的**双重假绿**；③ 升级为 APPROVE 的条件 = 按本报告的批 A/B 分离 + 行为优先改写 + 机械护栏。

**oracle 的 3 处新发现（均已独立复核为真）**：
- **N1**：`verilator-hier-input/spec.md:8` 的「等价 `num_srcs() > 0`」是**事实错误且方向危险**（`src/hierarchy_analysis.cpp:405-411` 明文纠正）⇒ 已列为 §4.2 #13（**优先项**）。
- **N2**：`eval_sequential` 的**代码注释自身也在漂**（2-step vs 3-step）⇒ 已列 §8 O2。
- **N3**：`tests/test_verilator_backend.cpp:480-486` 陈旧注释 ⇒ 已列 §8 O1。
- **N4**：oracle 因 shell 权限被拒未能复跑 CMakeLists 计数 ⇒ **本报告已代为复测**（§4.2 #15：二级 **23** 个，含根 24 个，项目级 CMakeLists 共 30 个）⇒ 修法**不是**改数字，而是改写为继承判据。

**对 oracle 建议的应答**：
- 「先裁决流程冲突再动文本」⇒ **已裁决**（§2.2）：不是新 ADR，而是引用**已有** ADR-072 §6 ⑨ + 已执行的 commit `1952ba5` ⇒ 只需把通道说明补进 AGENTS.md。
- 「8 段 Purpose 用三句式模板」⇒ **采纳**（§6 写法约定 + 8 段草稿）。
- 「6 类过期形态逐条处置」⇒ **采纳**（§4.2 19 条逐条表 + §4.2 判据 D1–D5）。
- 「新增 `verify_spec_purpose.sh` 机械门」⇒ **采纳但修正形态**（§7）：必须是**双向**集合校验（单向「全仓零 TBD」会在下次归档时转红 —— metis E1 同款纠正）。
- 「修订后 strict 应为约 31/20/51」⇒ **数字有误，已纠正**：只有 4 个 spec 会因补完 Purpose 而翻绿 ⇒ 真实值 **27 passed / 24 failed / 51 items**（§3.1）。

### 9.2 metis（gap 分析）

**metis 的核心判断（全部采纳）**：
- **隐藏意图**：用户真实要的不只是「补 8 段散文」，还要「让『已登记』变成可核的」⇒ ⇒ 护栏（§7）不是可选项。
- **范围切分**：批 A（Purpose 散文）vs 批 B（规范命题）维护成本差 ~10×，且改 Requirement 会进入「每 capability 一个 change + 5 段 improvement + oracle/critic」的流程 ⇒ ⇒ **必须分批**（§2.3）。
- **A2（过程日志三选一）**：metis 倾向「删」；本报告采纳为 §4.2 判据 D2 + §4.2 #8/#9/#14 的处置。
- **A4（反驳「改名」）**：metis 指出规范正文**已有**范围声明 ⇒ 「名实不符」只在不读正文时成立；且 `.rddf/improvements/rv32i-privileged-csr-2026-11.md:146` 已登记「**扩展**」⇒ ⇒ **改名与已登记计划正面冲突** ⇒ 本报告采纳「**不改名**」（§4.3）。
- **A5**：「25 个」不是计数过期而是**类别错误**（派生计数写进规范 ⇒ 新增一个 CMakeLists 即违规）⇒ 采纳为 §4.2 #15 的改写方案。
- **A6**：Purpose 里**不应**含 `MUST` ⇒ **部分采纳**：本仓既有健康 Purpose 有此用法（`ledger-residual-registry` / `rtl-emit-portability`）⇒ 改为「允许但**不承担新的可判定义务**」（§6 写法约定）。
- **A7（metis 的事实订正）**：`verilator-e2e-coverage` 的问题不是「机制描述过期」而是「**两条互斥 Requirement 并存**」（`:162` 的 `popen` vs `:184` 的共享 helper）⇒ **本报告已按此定性**（§4.2 #7，优先级最高）。
- **S1–S6（AI slop 风险）**：全部采纳为 §6 写法约定 + AC-3/AC-5。特别是 **S5（把假话写进 Purpose）** ⇒ 8 段草稿**刻意不写** VCD、3 步时钟、`@adr` 注释形态、`obj_dir/Vtop`（§6.4 已核对：Purpose 只写 7 符号 / 3 次求值 / 缓存命中，不提缓存文件名）。
- **E1（archive 不会写回占位符，但新建 capability 一定会写）** ⇒ 采纳为 §7 的护栏形态约束。
- **E8（多会话并发）**：metis 提示「直接改 8 个主 spec 会与进行中的 change 静默冲突」⇒ 采纳为执行前置：**开工前 `git status` + `ls openspec/changes/` 确认无 active change 触碰这 8 个文件**。
- **缺失 AC**：metis 主张「不要用 strict totals 当 AC」⇒ 采纳为 §5 的 **AC-0**（否决性条款）。

---

## 10. 执行顺序建议

1. **前置检查**（E8）：`git status` 干净 + `ls openspec/changes/` 无 active change 触碰这 8 个 spec。
2. **批 A**：按 §6 逐个 capability 写 Purpose（**建议顺序**：`verilator-reg-init`（几乎只补 Purpose，热身）→ `build-cpp-standard` → `rv32i-privilege` → `verilator-hier-input` → `verilator-error-flow` → `verilator-backend-e2e` → `verilator-e2e-coverage` → `jit-llvm-compat-tests`）⇒ 同 commit 落 AGENTS.md 快照订正（AC-7）。
3. **批 A 验收**：AC-1…AC-7 全过；`openspec validate --specs`（非 strict）仍绿；`scripts/verify_openspec_delta.sh` 仍绿。
4. **护栏**：`scripts/verify_openspec_purpose.sh`（双向集合校验 + `--self-test`）⇒ 接 CI。
5. **批 B 立项**（按 §4.2 优先级）：
   `verilator-hier-input` #13（**危险**：判据写错会误杀合法设计）→ `verilator-e2e-coverage` #7（**内部矛盾**）→ `verilator-backend-e2e` #6 #1 #2 #3 #4 #5 → `verilator-error-flow` #12 → `jit-llvm-compat-tests` #14 → `build-cpp-standard` #15 #16 #17 → `verilator-reg-init` #18 → `verilator-e2e-coverage` #8 #9 #10 #11。
6. **批 C 登记**：§8 的 O1–O6。

---

## 11. ⚠️ 2026-11-19 订正（文档重组后的复核）

**触发**：仓内出现文档目录重组（commit `0ab3263`「docs: 文档目录重组 —— 活文档区 483→40 篇，历史材料归档，usage_guide 重编号」），根 `AGENTS.md` 的路径引用随之改动（`docs/usage_guide/05-component-design.md` → `docs/usage_guide/02-component-design.md`）⇒ ⇒ 需要复核本报告的结论是否仍然有效。

### 11.1 复核结论：**全部有效**（零条被推翻）

| 复核项 | 结果 |
|---|---|
| 重组是否触及 `openspec/specs/**` | **否**（`git show --stat 0ab3263 -- openspec/` 为空）⇒ ⇒ **8 个 capability 的文本一字未动** ⇒ §4 的 19 条过期项与 §6 的 8 段草稿**全部成立** |
| 重组是否触及 `src/` `include/` `tests/` | **否**（commit 自述「零 `src/**` · 零 `include/**` 改动」）⇒ ⇒ §4.2 的全部**实现侧证据**（`verilator_backend.cpp:822-834` / `:748-797` / `:1008` / `hierarchy_analysis.cpp:405-411` / `codegen_verilog.cpp:484` `:1164-1171` / `verilator_backend.h:61-68`）**未移位** |
| 本报告引用的行号是否移位 | **否**：`AGENTS.md` 的 strict 快照仍在 `:479-480`；`ADR-072` 的「Purpose 的两处落点」仍在 `:345`、「⑨」仍在 `:484`；`.rddf/improvements/rv32i-privileged-csr-2026-11.md:146`（`rv32i-privilege` = **扩展**）内容未变 |
| 本报告引用的文件是否还在 | **是**：`rv32i-privilege-capability-2026-11.md` / `rv32i-privileged-csr-2026-11.md` / `openspec-specs-guide.md` / `scripts/verify_openspec_delta.sh:112` 全部在位 |
| 悬空引用断言是否仍成立 | **是**：`examples/axi4/axi4_full_example.cpp` 仍**不存在**（该目录只有 `axi4_lite_example_top.h` / `axi_soc_example_top.h` / `tb_*`）⇒ §4.2 #11 成立 |
| 是否存在进行中的 change 冲突（metis E8 前置） | **否**（**权威口径**）：`openspec list` ⇒ 「No active changes found.」；`openspec/changes/` 下**只有** `archive/`（173 项）⇒ 零 active change ⇒ §10 步骤 1 的前置条件当前满足 |

### 11.2 §3.2「护栏真空」的表述**精化**（结论方向不变，枚举修正）

原文说「`scripts/` 下引用 `openspec/specs` 的仅 **3 个**脚本……**全部是「把 spec 当数据源」**」⇒ ⇒ 逐个复核后精化：

| 脚本 | 对主 spec 的实际用法 | 是否「判红」 |
|---|---|---|
| `scripts/verify_openspec_delta.sh:141` | 读主 spec **仅为**匹配 MODIFIED delta 的 Requirement 标题（**合并正确性**） | ❌ 不判 spec 内容真伪 |
| `scripts/lib/dual_backend_parity.py:252` | 读 `openspec/specs/dual-backend-test-coverage/spec.md` 提取「契约锁定文件清单」⇒ 作为**契约源** | ⚠️ 判红，但覆盖的是**契约措辞的存在性**，且**不在本批 8 个之内** |
| `scripts/verify_chlib_dtor_composability.sh:149` | grep `chlib-state-machine-semantics` + `chlib-conditional-semantics` 的**契约 token** | ⚠️ 同上，**不在本批 8 个之内** |
| `scripts/verify_no_duplicate_rv32i_headers.sh:1076` | **仅注释**里举例 `openspec/specs/build-cpp-standard/`（说明 `build*` glob 危险） | ❌ 无实际依赖 |

⇒ ⇒ 修正后的准确表述：**没有任何护栏对主 spec 的「规范 vs 实现」一致性判红**；引用主 spec 的护栏共 3 个，作用分别是「delta 标题合并校验」与「两个 capability 的措辞锚点」⇒ ⇒ **本批这 8 个 capability 的内容零护栏覆盖**（结论不变，且更精确）。

⚠️ **并且这次重组让这条结论更强**：2026-11-19 新增的文档链接护栏（`scripts/verify_docs_links.sh` + `scripts/lib/docs_link_scope.py` + `.rddf/checks/docs-link-scope.md`）在**范围登记表 `:69` 里明确把 `openspec/**` 列为 out-of-scope** ⇒ ⇒ 连新增的护栏都**明确不覆盖主 spec** ⇒ ⇒ §7「批 A 完成后必须补 Purpose 护栏」的必要性**不降反升**。

### 11.3 §7 护栏建议**强化**（仓内已有同型模板可抄）

2026-11-19 新增的 `scripts/verify_docs_links.sh` 恰好是**本报告 §7 建议的同型范式**：
**薄壳脚本 + 范围登记表（`.rddf/checks/docs-link-scope.md`）+ 逻辑载体（`scripts/lib/docs_link_scope.py`）+ `--self-test` 投毒自检** ⇒ ⇒ §7 的 `scripts/verify_openspec_purpose.sh` **不必另发明**，照该四件套抄即可（登记表换成「占位符 capability 集合」）。

### 11.4 新登记（O7）：根 `AGENTS.md` 的路径引用**无护栏覆盖**

- `scripts/lib/docs_link_scope.py:23`：`SKIP_FILE_NAMES = {"AGENTS.md"}` ⇒ **AGENTS.md 家族被显式排除**（`.rddf/checks/docs-link-scope.md:69` 同款口径）。
- ⇒ 本次 `05-component-design.md → 02-component-design.md` 的修正**是人工做的**（重组 commit 里「L1 活文档（含 AGENTS.md / README / 各级索引）全部修」= 人工修）⇒ ⇒ 根 `AGENTS.md`（**每次会话都注入上下文**的文件）的链接漂移属**无护栏区** ⇒ 同类漂移会复发。
- ⇒ **建议**（不属本批 A/B）：把「根 `AGENTS.md` 的路径引用」纳入某个**已有**护栏的扫描面，或至少给它一条**枚举式**判据（AGENTS.md 的路径引用 **MUST** 全部 `test -e` 成立）⇒ 登记为 O7。

### 11.5 复核到的**非问题**（避免下轮误判）

- `.rddf/improvements/soc-rtl-product-and-tb-2026-11.md`（4 处）与 `.rddf/improvements/hierarchical-corpus-regression-2026-11.md`（1 处）仍引用旧路径 `docs/usage_guide/05-component-design.md` ⇒ ⇒ **但两者均已有对应的归档 change**（`2026-11-17-soc-rtl-product-and-tb-2026-11` / `2026-11-07-hierarchical-corpus-regression-2026-11`）⇒ ⇒ 属**已落地工作的历史记录** ⇒ ⇒ 与重组的治理口径（「历史归档区 / openspec / `.rddf` 按治理口径**不追**」）**一致** ⇒ ⇒ **不构成新增债**，仅登记。
- `docs/CHANGELOG.md:268` 引用旧路径 ⇒ 历史条目 ⇒ 正确做法就是**保留**。

### 11.6 本次 AGENTS.md 改动与本报告的关系（一句话）

**无影响**。该改动是**文档重组**的一个副作用（usage_guide 重编号），与 8 个 capability 的 Purpose / Requirement **零交集**（本报告与 8 个 spec 均**未**引用 `usage_guide` 路径，已 grep 确认）⇒ ⇒ 本报告的**结论、证据、草稿、验收判据全部维持原样**；仅 §3.2 表述精化 + 新增 O7 与 §7 的模板引用。

### 11.7 ⚠️ 2026-11-19 二次订正：**自纠一条「引错判据载体」的错误**

**触发**：用户以 `openspec list` 复核「无活跃 change」，指出本报告 §11.1 的判据选错了载体。

**错误**：§11.1 原写「`.rddf/state/iteration.json` 不存在 ⇒ 无活跃 change」⇒ ⇒ 这是**用非权威载体做推论**：
- `.rddf/state/iteration.json` 是 **rdd-workflow** 的状态文件（且本仓**从未入库** —— `git log --all -- .rddf/state/iteration.json` **零命中**）⇒ ⇒ **它不存在既不能证明、也不能否证「无活跃 change」**；
- openspec 的权威活跃态 = **`openspec/changes/` 下非 `archive/` 的子目录**（`openspec list` 即按此口径）。

**已修正为**（§11.1 该行）：`openspec list` ⇒ 「No active changes found.」+ `openspec/changes/` 下只有 `archive/`（173 项）⇒ 零 active change。

**影响范围**：**结论不变**（前置条件仍满足），**仅证据出处改正**。
⇒ ⇒ 附带一条**通用纪律**（与本仓「判据载体 MUST 可解析」惯例一致）：**「某文件不存在 ⇒ 某状态为真」这类推论，只有在该文件是**权威状态载体**时才成立** ⇒ 引用前 MUST 先确认它是权威载体（本次是**我没确认就引用**）。

### 11.8 ⚠️ 2026-11-19 三次订正：**AC-3 判据本身有缺陷**（跑预检才发现）

**触发**：为让用户能**逐字批准**而建立载体型 change（`openspec/changes/openspec-purpose-placeholders-2026-11/`）⇒ ⇒ 把 §5 的 AC-3 写成脚本**预检** ⇒ ⇒ **8 段里有 4 段「未过」**。

**逐条查明**（最长公共子串 ⇒ 命中的 Requirement 标题）：

| capability | 公共子串 | 命中标题 | 判定 |
|---|---|---|---|
| `verilator-e2e-coverage` | 「Verilator 仿真覆盖深度」（16 字符） | 「Verilator 仿真覆盖深度 MUST 在 ctest 中可见」 | ❌ **真问题** —— Purpose 开头是 Requirement 标题的近义复述 ⇒ **已改写**为「Verilator 双后端仿真的覆盖面在 CTest 矩阵中可见」 |
| `verilator-backend-e2e` | 「目标 wire 宽度内」（11 字符） | 「codegen 字面量值 MUST mask 到目标 wire 宽度内」 | ⚠️ **技术术语重合**，非 slop（Purpose 并非标题的复制） |
| `verilator-hier-input` | 「 MUST NOT 」（10 字符） | 「…fixture MUST NOT 走 documented SKIP…」 | ⚠️ **纯功能词碎片** ⇒ 旧判据的**误报** |
| `verilator-reg-init` | 「字面量 MUST 按」（10 字符） | 「print_reg 字面量 MUST 按 reg 宽规范化」 | ⚠️ **功能词 + 单个实词** ⇒ 旧判据的**误报** |

⇒ ⇒ **根因**：AC-3 的「最长公共子串 < **10 字符**」是**按字符数**算的 ⇒ ⇒ 它把**功能词碎片**（「 MUST NOT 」「 MUST 按」）与**技术术语**也算成「标题拼接」的证据 ⇒ ⇒ **判据不可用**（假阳性 3/4）。

⇒ ⇒ **已改判据**（§5 的 AC-3 行已就地更新）：**最长公共「实词片段」MUST < 4 个实词**（实词 = 剔除 `MUST`/`NOT`/`SHALL`/冠词/介词/标点后的词元，按**连续**片段计）⇒ ⇒
- 「Verilator 仿真覆盖深度」= 4 实词 ⇒ **命中** ⇒ 判红（真 slop，会被抓）；
- 「目标 wire 宽度内」= 3 实词 ⇒ 不命中；
- 「 MUST NOT 」= 0 实词 ⇒ 不命中；
- 「字面量 MUST 按」= 1 实词 ⇒ 不命中。

⇒ ⇒ **两条通用纪律**（与本仓「判别力自证」惯例一致）：
1. **判据必须先在真实数据上跑一遍** ⇒ 否则「判据本身有缺陷」会被当成「产物不合格」⇒ 本次若照旧阈值执行，会**逼着执行者去改 3 段本来合格的 Purpose**（假阳性 ⇒ 真损伤）。
2. **字符数阈值 ≠ 语义相似度** ⇒ 凡以「共有 N 个字符」为判据者，MUST 先看它会不会被功能词/术语触发。

⇒ ⇒ **对 §5 其它 AC 的同类体检**：AC-2（长度 + 关键词）、AC-4（diff 范围）、AC-5（路径存在）、AC-6（门绿）、AC-7（簿记）**均为「可机械判定的结构/事实型」** ⇒ 不受本条缺陷影响 ⇒ **无需改**。
