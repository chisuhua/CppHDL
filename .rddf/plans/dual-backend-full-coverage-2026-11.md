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

### 2.3 ⇒ ⇒⇒ **可达上限（路线 A = 转换既有文件）= 29 + 21 = 50 < 60**

⚠️⚠️ **口径二次订正（critic BLOCKER B4 · 2026-11-01 · 实测）**：
初版口径只减了「契约锁定」与「本身已含 dual pair」⇒ ⇒ **漏了第三类**：
**候选原文件的 `_behavior` 兄弟【已存在】** ⇒ ⇒ ⇒ **⇒ 转它 = 重复计数**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 实测：8 个**（`test_axi_gpio` / `test_axi_i2c` / `test_axi_spi` /
`test_axi_timer` / `test_axi_uart` / `test_blt_debug` /
`test_rv32i_pipeline` / `chlib/test_assert`）⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ **候选 37 − 契约锁定 7 − 本身已 dual 1 − **_behavior 兄弟已存在 8**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 真实可转 = 21** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 上限 = 29 + 21 = 50**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「阈值 60 ≤ 诚实上限」的违反幅度比初报（58）更大**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ planner 的裁决输入 MUST 用 50**

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

## 3. 成本模型（**实测**，2026-11-01 —— ⚠️ **二次订正：单位错了**）

### 3.0 🔴🔴 **订正：成本单位是【pair 数】，不是 DUT 数、也不是文件数**

⚠️ **逐 0.01 s 打点的实测**（假 HOME 全冷 · `test_assert_behavior` · **4 pair**）：

```
[  0.02s] 写出 top.v / sim_main.cpp
[  0.23s] Verilator: Walltime 0.012 s (elab=0.000, cvt=0.004, bld=0.000)
[ 10.89s] VerilatorBackend: cached artifact at .../.cache/cpphdl/verilator/<key>
[ 10.89s] VerilatorBackend: dlopen'd .../libVtop.so, created Vtop
```

⇒ ⇒ **Verilator 的 elaboration 只花 0.012 s** ⇒ ⇒ **那 10.66 s 是
【C++ 编译 + 链接 `libVtop.so`】** ⇒ ⇒ ⇒ **且实测第 2/3/4 个 pair 各自又花
~10.6 s** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 【根因】：`DUAL_BACKEND_TEST_PAIR` 为【每个 pair】
建独立的 `ch_device<DutType>`** ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 上下文的 verilog 源码不同
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 缓存 key 不同 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 【每个 pair 一次编译】**

⇒ ⇒⇒ **修正后的成本模型**：

| 成本项 | 单位 | 实测 | 随什么增长 |
|---|---|---|---|
| **verilator 自身的 JIT 编译** | **每 job 一次** | ~3 s | **不随** pair 数增长 |
| **每 pair 冷编译 + 链接 `.so`** | **每 pair** | **10.6 s**（组合逻辑 DUT） | **线性于 pair 数** |
| **每 pair 冷编译（mem 类 DUT）** | 每 pair | **72 s** | 同上，单价高 **6.8 倍** |
| **dlopen + Vtop 初始化** | 每 pair | **~0.01 s** | （已被「每 pair」项覆盖） |
| **全热**（缓存命中） | 每 pair | ~0.15 s | — |

⇒ ⇒⇒ ⚠️ ⚠️ **【重要推论】**：「每 pair 恰 1 条 `REQUIRE`」这条约定**代价很高**
⇒ ⇒ ⇒ 因为**拆 pair = 增加编译次数**（每拆一个 pair ⇒ ⇒ **+10.6 s**
（组合逻辑）/ **+72 s**（mem 类））⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 【必须】改用「XML
per-TEST_CASE 对比」的判据**（见 §4 验收模板）⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ **⇒ 【断言强度 MUST NOT 被判据绑架】**

### 3.1 修正后的**批次预算**（按 **pair 数**）

| 批 | pair 数（估） | 冷编译预估 | 备注 |
|---|---|---|---|
| A | 1 | ~14 s | 实测 **13.5 s**（含一次性 JIT）✔ |
| **B1**（✅ 已实施） | **5** | **~58 s** | 实测 **57.5 s** ✔ |
| B（其余） | ~30 | ~5.3 min | 组合逻辑 |
| C（AXI） | ~5 | ~3 min | AXI DUT 较大（单价待实测） |
| D（mem） | ~11 | **~13 min** | ⚠️ 单价 **72 s/pair** ⇒ ⇒ 放最后 |
| E（bundle/cache） | ~12 | ~8 min | cache DUT 单价待实测 |
| F（大文件多 DUT） | ~62 | ~11 min | ⚠️ 原估 25 min **偏高**（按 pair 重算） |
| H（路线 B） | ≥2 | ~1 min | — |

⇒ ⇒⇒ **Σ ≈ 41 min**（原估 56 min；按 pair 重算：5.3+3+13+8+11+1
⇒ ⇒⇒ ⇒ ⇒ **⇒ 【修正后【逼近而非明显超标】45 min】** ⇒ ⇒⇒ ⇒ ⇒ ⇒
**⇒ 但 45 min 覆盖的是【全部】构建 + 既有 39 个条目 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 真实余量远小于 41 min ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 【仍需逐批实测】** ⇒ ⇒⇒ ⇒ ⇒ ⇒ ⚠️ 但**仍须逐批实测回填**（D 批的 mem 单价
72 s 是**实测值**，其余是**外推**）⇒ ⇒⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒


## 4. 批次清单（**每批一个 change**；批次内按 **pair** 聚合 ⇒ ⇒ **不是**按 DUT ⇒ ⇒ 理由见 §3.0）

⚠️ **每批的验收模板**（**逐批复制**）：
- [ ] 新增的 `_behavior` 文件**真跑 verilator**
      ⇒ ⇒ **判据 = 「断言数(有 verilator) == 2 × 断言数(interpreter-only)」**
      ⇒ ⇒ ⇒ **⇒ 【N2′ · 判据松绑】**（oracle · 2026-11-01）
      ⇒ ⇒ ⇒ ⇒ ⇒ **它对 pair 结构【零约束】** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 多断言 pair
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 无需拆** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 零额外编译成本**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **与旧判据「总断言数 == 2 × pair 数」判别力
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 等价**（两者都在证明「verilator 半边真跑了」）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 已在既有 gpio 文件实测成立**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ （**46 == 2 × 23**）✔
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **负向验证的成本 ≈ 0**：只需**把 verilator 移出
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ PATH** 跑**interpreter 半边**（秒级）⇒ ⇒ ⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 不是「额外跑一遍 dual 测试（~40 min）」**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「用例数」不能作判据**（缺席时用例数不变）
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「TEST_CASE 数」同样不能**
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
| **F** | `chlib/test_onehot`(21) · `test_operation_results`(26) · `test_rv32i_pipeline`(7) · `test_module_advanced`(5) · `chlib/test_forwarding`(2) · `chlib/test_hazard_complete`(1) | **62** | ≈ 11 min | **高** | ⚠️ **本批单独一个 change** ⇒ ⇒ 且 ⚠️ **冷编译预估最高** ⇒ ⇒ ⇒ **CI 墙钟风险最大** ⇒ ⇒ ⇒ **必须先实测再决定是否拆分** |
| **G**（⚠️ **移出转换序列**） | `test_lnode_entry_set` · `test_axi_i2c` · `chlib/test_phase4_integration` · `test_multithread` | 4 | — | — | ⚠️ **oracle 裁决（2026-11-01）**：这 4 个**无 runtime 语义** ⇒ ⇒ 「先补 runtime 断言」**是路线 B 的新测试工作** ⇒ ⇒ ⇒ **伪装成转换会混淆两类产物的验收标准**（转换要求与原文**逐位等价**；新测试要求**手推期望**）⇒ ⇒ ⇒ **⇒ 移出转换序列** ⇒ ⇒ ⇒ ⇒ ⇒ 归入 **H 批**（路线 B）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 另**：`test_multithread` 的**并发**语义在「双跑」下需专门确认 |
| **H** | **路线 B：新增有意义的 dual 文件** | ≥ 2 | ≈ 1 min | 中 | ⚠️ **立项理由 = 这些语义【值得】双后端覆盖**（`chlib` 的 `state_machine` / `guard_contribution` / `conditional` / **builder 反压** / **pipe 透传** ⇒ ⇒ **目前零 dual 覆盖** ⇒ ⇒ 价值高）⇒ ⇒ ⇒ **计数达标是其【副作用】，不是立项理由** ⇒ ⇒ ⇒ ⇒ ⇒ **（critic SHOULD_FIX #2 · 2026-11-01：「补足到 60」的动机表述本身有【诱导凑数】的嫌疑 ⇒ ⇒ MUST 改写）** ⇒ ⇒ ⇒ ⇒ 每个文件 MUST 是**有独立价值的新测试**（⇒ ⇒ **不是**转换的复刻） |

⇒ ⇒⇒ **Σ 冷编译预估 ≈ 14 s + 5.3 + 3 + 13 + 8 + 11 + 1 ≈ 41 min**
⇒ ⇒⇒ **⚠️⚠️ 已超 CI 的 45 min** ⇒ ⇒⇒ ⇒ ⇒ **⇒ 必须实测 + 报告**
⇒ ⇒⇒ ⇒ ⇒ **⇒ 这是 objective 的 No-Go 判据所覆盖的情形**
⇒ ⇒⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 由 planner / 用户裁决（降级阈值 / 接受 CI 超时 / 缩减批次 F）**。

---

## 4.1 🔴🔴 判据的**规模化障碍**（A5 可持续性的真正瓶颈）

### 现象（实测数据）

⚠️ 「每 pair 恰 1 条 `REQUIRE`」这条约定（§4 验收模板的**前提**）⇒ ⇒ 在
**多断言用例**上代价极高 ⇒ ⇒ **且 AXI 类用例恰恰是多断言的**：

| `tests/test_axi_gpio.cpp` 的用例 | REQUIRE 数 | 可转？ |
|---|---|---|
| `端口接口` | 0（width 结构断言） | ❌ 无增量价值 |
| `VerilogGeneration` | 2 | ❌ codegen 测试 |
| `ComponentCreation` | 1（name） | ❌ 价值低 |
| 写 DATA_OUT 反映到 gpio_out | 1 | ✅ |
| 写 DIRECTION 反映到 gpio_dir | 1 | ✅ |
| 读 DATA_OUT 返回写入值 | 1 | ✅ |
| 读 DATA_IN 反映 gpio_in | 1 | ✅ |
| AW/W 相位分离后写仍落地 | 1 | ✅ |
| AW 握手后改写 awaddr 不改变写目标 | 3 | ✅（须拆 3 pair） |
| BVALID 保持到 BREADY | 3 | ✅（须拆 3 pair） |
| RVALID 保持到 RREADY | 4 | ✅（须拆 4 pair） |
| 事务结束后可接受下一笔 | 2 | ✅（须拆 2 pair） |
| 写未映射地址返回 DECERR | 2 | ✅（须拆 2 pair） |
| rst_n 复位清空寄存器并释放 busy | 5 | ✅（须拆 5 pair） |

⇒ ⇒ **11 个可转用例 = 24 条 REQUIRE = 24 个 pair** ⇒ ⇒
**24 × 10.66 s ≈ 4.3 min（仅 gpio 一个文件！）** ⇒ ⇒ ⇒ **⇒ 不可持续**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「每 pair 一条 REQUIRE」这条约定【在规模化下失效】**。

### ⭐ 候选解法（**假设已实测可行**）

⇒ ⇒ **把每个 pair 的 body 包进 `SECTION`** ⇒ ⇒ Catch2 的 XML reporter 会给出
**per-Section** 的 `successes` ⇒ ⇒ ⇒ **两半边的同名 Section 可按名字配对**
⇒ ⇒ ⇒ **⇒ 判据可升级为「verilator 半边每个 Section 的 successes == interpreter
半边对应 Section 的 successes」** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 多断言 pair【无需拆】**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 零额外编译成本** ✔✔

⚠️ **实测证据**（2026-11-01，批次 B1 临时验证 ⇒ ⇒ 事后已撤销）：

```xml
<TestCase name="... [interpreter]">
  <Section name="enable=1 & condition=0 => failed=1">
    <OverallResults successes="1" .../>
<TestCase name="... [verilator]">
  <Section name="enable=1 & condition=0 => failed=1">
    <OverallResults successes="1" .../>
```

⇒ ⇒ **⇒ per-Section 的 successes 确实可按名字配对** ✔

⚠️ **但它落地是【基础设施级】** ⇒ ⇒ ⇒ **不在批次 change 里顺手做**：
① 需要**机械核验脚本**（解析 XML + 按 Section 名配对比对）⇒ ⇒ ⇒ **要么新增
护栏、要么扩展既有护栏**；
② 需要**重新跑**已归档的批次 A / B1（它们的 pair **无 SECTION** ⇒ ⇒ ⇒ **不满足
新判据的前置**）⇒ ⇒ ⇒ **返工**；
③ **CI 墙钟**：XML 需要**额外跑一遍** dual 测试（`ctest` 不产 XML）⇒ ⇒ ⇒
⇒ ⇒ **要么在 CI 里加一次（重复成本 ~40 min）**、⇒ ⇒ ⇒ ⇒ ⇒ **要么用
Catch2 listener + sidecar 把比对放进测试内部**（⇒ ⇒ ⇒ ⇒ **零额外墙钟**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 但实现复杂** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 需 architect/planner
定夺**）。

⇒ ⇒⇒ **⇒ 决策门（独立立项）**：是否值得为「解除规模化障碍」升级判据？
⇒ ⇒⇒ **收益**：批次 B/C/D/E/F 的墙钟从「拆分后」回到「不拆」
⇒ ⇒⇒ ⇒ ⇒ ⇒ **（估算省 ~3–5 倍）** ⇒ ⇒⇒ ⇒ ⇒ ⇒ **⇒ 成本**：新护栏 +
2 个批次返工 + CI 策略决策 ⇒ ⇒⇒ ⇒ ⇒ ⇒ **⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **建议：做**，
但**作为独立 change**（⇒ ⇒ 不与任何批次耦合）⇒ ⇒⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒**

## 5. 墙钟账本（**每批实测后回填** ⇒ ⇒ 不许拍脑袋**）

| 批 | 冷缓存墙钟（实测） | 累计 | CI 预算 45 min 余量 |
|---|---|---|---|
| A | **13.5 s**（含 verilator 自身 JIT 的一次性 ~3 s） | 13.5 s | 44.8 min |
| **B1**（✅ 已实施） | **57.5 s**（**5 pair** ⇒ ⇒ 10.66×5 + JIT ≈ 57.3 s ⇒ ⇒ 与 §3.0 模型一致 ✔） | **71 s**（A 13.5 s + B1 57.5 s） | 43.8 min |
| **B2**（✅ 已实施 · 连接语义组） | **33 s**（**16** pair ⇒ ⇒ **≈2.1 s/pair** ⇒ ⇒ ⚠️ **比 §3 模型（10.66 s/pair）低 5 倍** ⇒ ⇒ **成本模型 MUST 按 DUT 复杂度分档**：本次全是**纯组合 / 轻连线** DUT ⇒ ⇒ AXI peripheral（批次 C）才是**重** DUT） | **104 s**（A 13.5 s + B1 57.5 s + B2 33 s） | 44.2 min |
| B（其余：pwm / jit_concat / interpreter_jit_compat） | ⛔ **不转**（理由见 change 的 proposal「被刻意排除的」表：pwm 唯一可转用例**零判别力**且**已被** `test_axi_pwm_behavior` 覆盖；两个 JIT 文件的身份是 **JIT-vs-interpreter** parity ⇒ 与本主题**正交** ⇒ ⇒ **不为了计数而转换**） | — | — |
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
| 1 | ⚠️ **CI 墙钟逼近 45 min**（修正后预估 ≈ 41 min，但 **45 min 覆盖的是【全部】构建 + 既有 39 个条目** ⇒ ⇒ ⇒ **真实余量远小于 41 min**） | ① 每批**实测**并回填 §5；② **批次 F 视实测决定拆分**；③ ⇒ ⇒ **超出即停 + 报告**（不硬凑） |
| 2 | 🔴 **verilator 半边被 skip ⇒ 假绿** | **每批 MUST 核对「总断言数 == 2 × pair 数」**（缺席 ⇒ 只有 1 倍 ⇒ ⇒ **用例数不变 ⇒ ⇒ 不能用用例数判**）⇒ ⇒ 判据细节见 **§4 验收模板**（含**双向**实测证据） |
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
**⇒ 诚实保守上限的真值 = 29 + 21 = **50** < 60**
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
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 计划里的「Σ ≈ 41 min」只是【新增】verilator 编译的增量**
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
| ② A5 的计划**逼近** objective §9.2 自己的 No-Go 判据（⇒ ⇒ 修正后 Σ ≈ 41 min，**但 45 min 覆盖的是全部构建 + 既有 39 个条目** ⇒ ⇒ ⇒ **⇒ ⇒ 真实余量远小于 41 min**）⇒ ⇒ 继续机械推进 = **朝着已知的墙走** | Σ ≈ 41 min（增量）vs 45 min（含全部构建） |
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
| 2026-11-01 | 🔴 **可达上限口径二次订正**：**58 → 50**（漏排除「`_behavior` 兄弟已存在」的 8 个 ⇒ ⇒ 真实可转 **21**）⇒ ⇒ **「阈值 60 ≤ 诚实上限」的违反幅度更大** ⇒ ⇒ ⇒ planner 的裁决输入 MUST 用 **50**；⇒ ⇒ ⇒ ⇒ **⇒ 批次 C 的前提也错了**：`test_axi_gpio_behavior.cpp` 等 **8 个 `_behavior` 已存在**（2026-10-20 起）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 批次 C 不是「新建文件」而是「补齐既有 `_behavior` 未覆盖的用例」** ⇒ ⇒ ⇒ ⇒ ⇒ | critic BLOCKER B4 + B3（实测：候选里 8 个原文件的 `_behavior` 兄弟已存在） |
| 2026-11-01 | 🔴 **成本模型二次订正**：单位 = **【pair 数】**（逐 0.01 s 打点：elaboration **0.012 s**，**C++ 编译+链接 10.66 s/pair**）⇒ ⇒ 「每 pair 一条 REQUIRE」代价很高 ⇒ ⇒ 批次预算由 Σ 56 min 修正为 **~41 min** | 批次 B1 实施期实测（假 HOME 全冷） |
| 2026-11-01 | **创建** ⇒ ⇒ 量化盘点（37 → 29）+ 成本模型（实测墙钟）+ 8 批清单 + 墙钟账本 | T7 步骤 1 · gap C · 2026-11-01 试点实测 |
