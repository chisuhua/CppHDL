# T7 执行计划：`dual-backend-full-coverage` Phase 5 全量（批次清单 + 墙钟预算）

> **文档性质**：**执行侧**计划（对应 `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md`
> 的 **T7**）⇒ ⇒ ⇒ **判据属 planner 职责，build 不得自行改**（见 §2）。
>
> **kind**: change-plan（一个 change 一份 plan 的惯例）  **source**: T7 + gap C
> **covers**: T7 步骤 1（量化盘点）· 步骤 2–3（分批推进 + 计数同步）

---

## 1. 目标与口径（**逐字引用 objective，避免口径漂移**）

- objective：`objective-test-coverage-100pct`（**planner 职责**）
- **Go 判据**：**双后端 parity 源文件数 ≥ 60**（2026-10-30 修订，取代旧的
  「`ctest -L verilator_e2e` ≥ 100」）
- **口径定义**（机械可核验）：仓内测试源文件（`tests/**`、`examples/**`）中
  满足**任一**：
  1. **剥注释后**含 `DUAL_BACKEND_TEST_PAIR(` 的**调用**（⚠️ 必须是**调用**，
     不是**提到**）；或
  2. ∈ hand-crafted 真 dual 白名单（当前 **1** 个：
     `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）
- **现状（2026-11-01 实测）**：**29**（28 macro 文件 + 1 hand-crafted）
  ⇒ ⇒ **距阈值差 31**
- **护栏**：`scripts/verify_dual_backend_parity.sh`（源文件数 + macro 调用数
  **两个锚点**：`DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS`）
  + 根 `AGENTS.md` 的 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`

---

## 2. 🔴🔴 §2 的**盘点结果 = gap C 的答案**：**转换路线上限 58 < 阈值 60**

### 2.1 筛选依据（**权威**，逐字复用 capability R6 的 grep 模式）

```bash
grep -l "ch_device\|ch_module" tests/test_*.cpp tests/chlib/test_*.cpp | \
  xargs grep -L "STATIC_REQUIRE" | \
  xargs grep -L "ch_out<ch_uint<[0-9]\{3,\}>>"
```
⇒ ⇒ 候选 **37** ⇒ ⇒ 减：契约锁定的 7（在候选内）· 已是 dual 的 1
⇒ ⇒ ⇒ **可转 dual = 29**

### 2.2 两条排除的**实测订正**（相对 2026-10-28 的盘点）

| 项 | 2026-10-28 盘点 | **2026-11-01 实测** | 差异原因 |
|---|---|---|---|
| 可转 dual 条目 | **33** | **29** | 盘点把「**无 runtime 语义**」的 4 个算在内（实测 **4** 个，盘点说 3）⇒ ⇒ **差 4** |
| 「Catch2 SECTION 重入」排除 | 列为排除项 | ⚠️ **该排除项的措辞可能已陈旧** | 宏是**两个独立 TEST_CASE** ⇒ ⇒ SECTION 在每个后端**各跑一次** ⇒ ⇒ **语义正确** ⇒ ⇒ **该排除项应复核**（**不在本 change 擅自改契约**） |

### 2.3 ⇒ ⇒⇒ **可达上限（路线 A = 转换既有文件）= 29 + 29 = 58 < 60**

⇒ ⇒ ⇒ **⇒ T7 步骤 1 的「planner 决策门」触发** ⇒ ⇒ ⇒
**build 不得自行改 objective 的 Go 判据** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 本计划只交数字
+ 交两条正当路线，由 planner / 用户裁决**：

| 路线 | 内容 | 上限 | 是否需要改判据 |
|---|---|---|---|
| **A** | **转换既有文件**（新增 `_behavior` 文件） | **58** | ❌ **上限 < 60** ⇒ ⇒ 路线 A **单靠转换到不了阈值** |
| **B** | **新增有意义的 dual 文件**（新测试内容） | **无上限** | ❌ **不需要**（objective §5 的 **axis ③**：「新增有意义的 dual 文件仍是 Phase 5 的正题」；⚠️ §5 反例**只**禁止「给任意条目加 `verilator_e2e` 标签」凑数） |

⇒ ⇒⇒ **⇒ 本计划采用「A 打底 + B 补足」的组合** ⇒ ⇒ ⇒
**⇒ A 的 29 个文件本身有独立价值**（把既有语义纳入双后端）⇒ ⇒ ⇒
**⇒ B 只需补足 ≥ 2 个文件即可达 60** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ B 的每个文件
MUST 是【有独立价值的新测试】**（⇒ ⇒ **不是**转换的复刻 ⇒ ⇒ **不是**加标签）。

---

## 3. 成本模型（**实测**，2026-11-01）

| 场景 | 墙钟（实测） | 说明 |
|---|---|---|
| **冷缓存 · 小 DUT**（纯组合逻辑） | **13.5 s** | 试点 `test_blt_debug_behavior` |
| **冷缓存 · mem 类 DUT**（`ch_mem` + FIFO） | **72.0 s** | `test_stream_fifo_dual`（假 HOME 测得） |
| 热缓存 · 小 DUT | 0.6 – 3.0 s | `test_stream_fifo_dual` / `test_cache_sync` |
| 热缓存 · RV32I 流水线 | 7.7 s | `test_rv32i_pipeline_behavior` |

⇒ ⇒⇒ **两条成本结论**：
1. **冷编译是主成本**（10 s ~ 60 s / DUT）⇒ ⇒⇒ **CI 每次都是冷缓存**
   ⇒ ⇒ ⇒ **CI 墙钟 ≈ 不同 DUT 类型数 × 平均编译成本**；
2. **同 DUT 的多个 TC 只编译一次**（缓存按 SHA-1 key）⇒ ⇒⇒ **⇒ 必须按
   「DUT 聚合」组织批次** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 同 DUT 的 TC 放同一文件**。

⚠️⚠️ **CI 硬约束**：`.github/workflows/ci.yml` 的 verilator_e2e job
**`timeout-minutes: 45`** ⇒ ⇒⇒ **且 `RESOURCE_LOCK "verilator_backend"`
⇒ ⇒ 全部 dual 测试【串行】** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 累计墙钟直接吃这个预算**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这正是 objective §9.2 的 No-Go 判据**（「PR feedback 墙钟
持续 > 45 min ⇒ 需降级」）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 本计划的批次划分把「CI 墙钟」
当作【首要约束】**。

---

## 4. 批次清单（**每批一个 change**；批次内按 DUT 聚合）

⚠️ **每批的验收模板**（**逐批复制**）：
- [ ] 新增的 `_behavior` 文件**真跑 verilator**
      ⇒ ⇒ **判据 = 「总断言数 == 2 × pair 数」**
      ⇒ ⇒ ⇒ **⇒ 前提：每个 pair 体 MUST 恰好 1 条 `REQUIRE`**
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 理由（critic SHOULD_FIX #1 · 2026-11-01）**：
      「断言数 = 2 × pair 数」**只在每 pair 恰 1 条 REQUIRE 时成立**
      ⇒ ⇒ ⇒ Catch2 的**汇总输出不区分 per-TEST_CASE 断言数**
      ⇒ ⇒ ⇒ ⇒ **⇒ 多断言批次下该判据要么恒假红、要么退化成无判别力**
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 泛化 = 约定「每 pair 一条断言」**
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 代价接近零**：同一 DUT 的多个 pair **只编译一次**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ （缓存按 SHA-1 key）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 拆 pair 不增加墙钟**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **「TEST_CASE 数」不能替代本判据** ——
      verilator 缺席时 macro 的 verilator 半边**仍然注册**（只 WARN + return）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ TEST_CASE 数恒为 2 × pair 数 ⇒ ⇒ 无判别力**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ MUST 用【断言数】而非【用例数】**
      ⇒ ⇒ **若 verilator 缺席则断言数只有 1 倍 ⇒ ⇒ 假绿**）
- [ ] `ctest -LE perf` 全绿（零回归）
- [ ] **计数同步**：`CTEST_TOTAL`（每批 +N 个 ctest 条目）·
      `VERILATOR_E2E_TOTAL`（每批 +N）· `DUAL_BACKEND_PARITY_TOTAL`（每批 +N 文件）·
      `DUAL_BACKEND_PARITY_PAIRS`（每批 +M）⇒ ⇒ **`scripts/verify_test_counts.sh`
      与 `scripts/verify_dual_backend_parity.sh` 双绿**
- [ ] 全部护栏 exit 0
- [ ] **本批冷缓存墙钟实测**（假 HOME）⇒ ⇒ 累计墙钟记入 §5

| 批 | 文件 | DUT 数 | 冷编译预估 | 风险 | 备注 |
|---|---|---|---|---|---|
| **A**（✅ 已完成） | `test_blt_debug_behavior.cpp` | 1 | 13.5 s | 低 | 试点；**2 断言 / 2 用例** ⇒ ⇒ verilator 半边真跑 ✔（⚠️ **零 DUT 复制** —— DUT 是库头里的既有类型 `riscv::BranchConditionUnit` ⇒ ⇒ 宏第一个参数是**类型名**）|
| **B** | `test_axi_pwm_simple` · `test_bool_connection` · `test_operator_connection` · `test_jit_concat` · `test_interpreter_jit_compat` · `chlib/test_assert` · `test_wires_connection` · `chlib/test_multi_if_acceptance` | **23** | ≈ 5 min | 低 | 纯组合逻辑；⚠️ **剔除 / 降级清单（oracle · 2026-11-01）**：① `test_jit_concat` / `test_interpreter_jit_compat` ⇒ ⇒ ⚠️ **JIT 混淆的担忧在机制上【不成立】**（macro 的解释器半边 `set_jit_enabled(false)` ⇒ ⇒ **两半边都不走 JIT** ⇒ ⇒ 归因不会混）⇒ ⇒ **真正问题是价值低**（身份是 JIT-vs-interpreter parity）⇒ ⇒ **降到批次末尾** + 先做 13.5 s 探针；② `chlib/test_multi_if_acceptance` 的 **3 个 `REQUIRE_THROWS_AS` 用例结构性不可转**（**构图期**拒绝 ⇒ 宏在函数体内自己构造 DUT ⇒ BODY 拿不到「构造失败」事件 ⇒ 且构图期异常对两后端是**同一段 C++ 代码** ⇒ ⇒ **dual 化零增量价值**）⇒ ⇒ **只转 3 个运行期用例**；③ `chlib/test_assert` ⇒ ⇒ 先探针「report」是可观测端口还是解释器侧打印；④ `test_axi_pwm_simple` 的 `VerilogGeneration` 用例是 **codegen 测试**（dual 无增量）⇒ ⇒ 只转 `Simulation` 用例；⑤ `test_wires_connection:274` 的「Empty constructed ch_uint」是**空节点语义**（解释器专属）⇒ ⇒ 剔除 |
| **C** | `test_axi_gpio` · `test_axi_timer` · `test_axi_uart` · `test_axi_spi` | **5** | ≈ 3 min | 中 | AXI peripheral ⇒ ⇒ DUT 较大；⚠️ **`rst_n` 是必需端口**（未接线 = 永久复位）⇒ ⇒ **每个 DUT MUST 驱动 `rst_n`**（`axi4-peripheral-deferred` 的教训） |
| **D** | `test_mem_comb_read` · `test_mem_connection` · `test_mem_timing` | **11** | ≈ 13 min | 中 | **mem 类**（冷编译 72 s/个）⇒ ⇒ ⚠️ `aread` 已在 R6 排除清单**移除**（drift 已消除）⇒ ⇒ 合法候选 |
| **E** | `test_bundle_connection` · `test_bundle_master_slave` · `chlib/test_cache_complete` | **12** | ≈ 8 min | 中 | bundle / cache；⚠️ cache 类 DUT 可能触发 **ADR-041 Async 走 golden-value 而非 parity** 的分类 |
| **F** | `chlib/test_onehot`(21) · `test_operation_results`(26) · `test_rv32i_pipeline`(7) · `test_module_advanced`(5) · `chlib/test_forwarding`(2) · `chlib/test_hazard_complete`(1) | **62** | ≈ 25 min | **高** | ⚠️ **本批单独一个 change** ⇒ ⇒ 且 ⚠️ **冷编译预估最高** ⇒ ⇒ ⇒ **CI 墙钟风险最大** ⇒ ⇒ ⇒ **必须先实测再决定是否拆分** |
| **G**（⚠️ **移出转换序列**） | `test_lnode_entry_set` · `test_axi_i2c` · `chlib/test_phase4_integration` · `test_multithread` | 4 | — | — | ⚠️ **oracle 裁决（2026-11-01）**：这 4 个**无 runtime 语义** ⇒ ⇒ 「先补 runtime 断言」**是路线 B 的新测试工作** ⇒ ⇒ ⇒ **伪装成转换会混淆两类产物的验收标准**（转换要求与原文**逐位等价**；新测试要求**手推期望**）⇒ ⇒ ⇒ **⇒ 移出转换序列** ⇒ ⇒ ⇒ ⇒ ⇒ 归入 **H 批**（路线 B）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 另**：`test_multithread` 的**并发**语义在「双跑」下需专门确认 |
| **H** | **路线 B：新增有意义的 dual 文件** | ≥ 2 | ≈ 1 min | 中 | ⚠️ **立项理由 = 这些语义【值得】双后端覆盖**（`chlib` 的 `state_machine` / `guard_contribution` / `conditional` / **builder 反压** / **pipe 透传** ⇒ ⇒ **目前零 dual 覆盖** ⇒ ⇒ 价值高）⇒ ⇒ ⇒ **计数达标是其【副作用】，不是立项理由** ⇒ ⇒ ⇒ ⇒ ⇒ **（critic SHOULD_FIX #2 · 2026-11-01：「补足到 60」的动机表述本身有【诱导凑数】的嫌疑 ⇒ ⇒ MUST 改写）** ⇒ ⇒ ⇒ ⇒ 每个文件 MUST 是**有独立价值的新测试**（⇒ ⇒ **不是**转换的复刻） |

⇒ ⇒⇒ **Σ 冷编译预估 ≈ 13.5 s + 5 + 3 + 13 + 8 + 25 + 1 ≈ 56 min**
⇒ ⇒⇒ **⚠️⚠️ 已超 CI 的 45 min** ⇒ ⇒⇒ ⇒ ⇒ **⇒ 必须实测 + 报告**
⇒ ⇒⇒ ⇒ ⇒ **⇒ 这是 objective 的 No-Go 判据所覆盖的情形**
⇒ ⇒⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 由 planner / 用户裁决（降级阈值 / 接受 CI 超时 / 缩减批次 F）**。

---

## 5. 墙钟账本（**每批实测后回填** ⇒ ⇒ 不许拍脑袋**）

| 批 | 冷缓存墙钟（实测） | 累计 | CI 预算 45 min 余量 |
|---|---|---|---|
| A | 13.5 s | 13.5 s | 44.8 min |
| B | 待测 | | |
| C | 待测 | | |
| D | 待测 | | |
| E | 待测 | | |
| F | 待测 | | |
| G | 待测 | | |
| H | 待测 | | |

---

## 6. 风险与对策

| # | 风险 | 对策 |
|---|---|---|
| 1 | 🔴 **CI 墙钟超 45 min**（预估 56 min） | ① 每批**实测**并回填 §5；② **批次 F 视实测决定拆分**；③ ⇒ ⇒ **超出即停 + 报告**（不硬凑） |
| 2 | 🔴 **verilator 半边被 skip ⇒ 假绿** | **每批 MUST 核对断言数 = 2 × pair 数**（缺席 ⇒ 只有 1 倍）⇒ ⇒ 列入验收模板 |
| 3 | 🔴 **dual 暴露真实 parity 分歧** | ⇒ ⇒ 这是**价值**不是故障 ⇒ ⇒ ⇒ **停下来开独立 change**（T7 步骤 4）⇒ ⇒ **不在覆盖 change 里顺手修 codegen** |
| 4 | ⚠️ **假绿的新形态**（A4 刚发现） | ⇒ ⇒ 接线方式本身是判据的一部分 ⇒ ⇒ **MUST 用 `as_master()` + `<<=`**（**禁** `operator=` 喂常量）⇒ ⇒ 否则会把「常量接线下能过」固化成「真实用法能过」 |
| 5 | ⚠️ **重复代码**（`_behavior` 文件复制 DUT） | ⇒ ⇒ 沿用仓内既有 pattern（`test_axi_spi_behavior` / `test_rv32i_pipeline_behavior`）⇒ ⇒ ⇒ 但每批 MUST 在文件头注释写明「原文件 MUST NOT 修改」 |
| 6 | ⚠️ **SECTION 排除项的措辞可能已陈旧** | ⇒ ⇒ **不在本 change 改契约** ⇒ ⇒ 登记为**待 planner 复核**（§2.2） |

---

## 6.1 🔴🔴 升级给 planner 的**决策材料**（2026-11-01 · oracle + critic **共同发现**）

> ⚠️ **两处都独立复算出同一个比「差 2 个文件」更严重的事实** ⇒ ⇒ ⇒
> **⇒ 阈值 60 已违反 objective 自己写下的不变量** ⇒ ⇒ ⇒ **⇒ 判据本身
> 需要 planner 重新背书** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这才是决策门该 escalate 的内容**。

### 6.1.1 阈值 60 **违反** objective §2 的自订不变量

objective §2 明文：「⚠️ **阈值 MUST ≤ 诚实保守上限**（62）—— 否则就是『硬凑』的判据」
⇒ ⇒ 而 62 的构成 = 「29 + 33 个可转换条目」⇒ ⇒ **那个 33 依赖 2026-10-28
的盘点，而它把 4 个「无 runtime 语义」的文件错算在内** ⇒ ⇒ ⇒
**⇒ 诚实保守上限的真值 = 29 + 29 = 58 < 60**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 即「阈值 ≤ 诚实上限」这条 objective 自订规则【已被违反】**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **这不是「还差 2 个文件」的工程量问题**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 是判据本身与自己的不变量矛盾 ⇒ ⇒ ⇒ 需 planner 重新背书**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 两条出路（planner 裁决）**：
① **明文背书「路线 B（新增有意义的 dual 文件）补足」** ⇒ ⇒ 判据不变
② **修阈值**（⇒ ⇒ 改成可达值 + 说明）⇒ ⇒ **⚠️ 但「阈值 ≤ 诚实上限」这条
不变量要求路线 B 被计入「诚实上限」** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 换句话说：无论选
哪条，planner 都需要对「什么算『诚实上限』」重新表态** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ **⇒ build 不得代写（owner = rdd-planner）** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ 本 change 只交数字 + 交证据**。

### 6.1.2 CI 墙钟缺口是**结构性**的（比 56 − 45 = 11 min 更大）

`.github/workflows/ci.yml` 的 `pr-verilator-fast` job（`timeout-minutes: 45`）
覆盖的是**全部**：装依赖 + 编 verilator + **编整个 CppHDL** +
`ctest -L verilator_e2e -LE perf`（既有 **39** 个条目）
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 计划里的「Σ ≈ 56 min」只是【新增】verilator 编译的增量**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 真实缺口比 11 min 大得多**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ 且 CI **没有缓存** `~/.cache/cpphdl/verilator`
（workflow 只缓存了 ccache）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「冷编译是主成本」在 CI 上
每一轮都成立**（缓存 key 本身是内容寻址的：verilog 源 SHA-1 + 模板 + flags +
verilator 版本）。

### 6.1.3 CI 杠杆（**交 planner · build 不擅自改 workflow**）

| # | 杠杆 | 安全性论证 | 效果 |
|---|---|---|---|
| ① | **缓存 `~/.cache/cpphdl/verilator`** | key **本身内容寻址** ⇒ ⇒ 改测试的 PR 全部命中 ⇒ ⇒ 缓存**不会**产生错配 | 不改测试的 PR ⇒ **全部变热缓存**（⇒ ⇒ 墙钟从「编译成本」降到「运行成本」） |
| ② | **`ctest -I 0,2` / `-I 1,2` 把 verilator_e2e 套件跨 job 分片** | 两个 runner **各持各的** `RESOURCE_LOCK` ⇒ ⇒ **互不冲突** | 45 min 预算 ⇒ **变 2 × 45 min** |

⚠️ **两者都不是任何测试批次 change 的一部分** ⇒ ⇒ ⇒ **是 CI 架构决策**
⇒ ⇒ ⇒ **⇒ build 提请、planner / 用户裁决**。

### 6.1.4 ⭐ 优先级建议（oracle · 2026-11-01）：**未收口项优先**，且**顺序反过来互相成就**

| 理由 | 证据 |
|---|---|
| ① B1（builder 死路）/ B6（pipe 零通过）是**协议合规用法下的功能失效** ⇒ ⇒ 不是「覆盖率搬运」 | `source.ready` **恒 0**；pipe 在**正确接线**下**零数据通过** |
| ② A5 的计划已**撞上 objective §9.2 自己的 No-Go 判据** ⇒ ⇒ 继续机械推进 = **朝着已知的墙走** | Σ ≈ 56 min > 45 min |
| ③ 收口清单的**收官判据 = 无 ❌ 行** ⇒ ⇒ 而 **A5 全做完一行 ❌ 都不减少** | `.rddf/checks/stream-domain-closure.md`（17 行 · ✅ 10 / ❌ 7） |
| ④ ⭐ **B1 / B6 的修复【必然附带新的行为测试】** ⇒ ⇒ 而这些测试**正是 H 批要找的「路线 B 文件」** | ⇒ ⇒ ⇒ **⇒ 两件事互相成就** ⇒ ⇒ ⇒ **⇒ 先裁 B1/B6 → 落地时顺带产出路线 B** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是「顺序反过来互相成就」的机制** |

⇒ ⇒⇒ **⇒ 建议的推进序列**（build 侧）：
1. **architect 裁 B1 / B6 / B4**（证据已齐 ⇒ ⇒ 一次会议量级）⇒ ⇒ B4（`fwft_fifo` 物理删除，零消费者）可**搭 B1 的便车**作独立小 change；
2. **planner 裁** 6.1.1（阈值）+ 6.1.3（CI 杠杆）；
3. **B1 / B6 落地时**，行为测试**直接写成 dual pair**（计入路线 B）⇒ ⇒ 顺带把 `tests/test_stream_builder.cpp:116-117` 的 `operator=` 接线修掉（台账已登记的判据之一）；
4. **A5 侧**只推**便宜批次**（B 按 §4 的剔除清单 + C）⇒ ⇒ 并**实测回填墙钟账本** ⇒ ⇒ 若 `pr-verilator-fast` 实测 > 40 min（留 5 min 余量）⇒ ⇒ **即停并报告**（把计划里的纪律**升级为验收模板的勾选项**）。


## 7. 变更记录

| 日期 | 变更 | 依据 |
|---|---|---|
| 2026-11-01 | **创建** ⇒ ⇒ 量化盘点（37 → 29）+ 成本模型（实测墙钟）+ 8 批清单 + 墙钟账本 | T7 步骤 1 · gap C · 2026-11-01 试点实测 |
