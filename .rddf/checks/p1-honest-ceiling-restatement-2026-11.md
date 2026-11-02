# P1 决策材料：「诚实上限」的构成 —— 第二版（2026-11-02 实测）

> ⚠️ **本文档性质**：**客观台账 + 决策选项**。**owner = rdd-planner**（`objective` 的
> 判据属 planner 职责）⇒ ⇒ **build 不代写判据**，只交**实测数字**与**选项**。
> ⇒ ⇒ 与 plan §6.1.1 的定位一致（"本 change 只交数字 + 交证据"）。

---

## 1. 为什么需要第二版（第一版已被实测推翻）

`objective-test-coverage-100pct` §2 在 2026-11-02 采纳了「诚实上限的重述」：

> **「诚实保守上限」的构成 = 「既有可转条目数」+ 「新增有意义的 dual 文件数」**

⚠️ **该重述的方向是对的，但它把「诚实上限」变成了一个**没有上界的量** ⇒ ⇒
「数字可达性」这条护栏随之失效 ⇒ ⇒ 「防硬凑」的责任被转移到三条机制
（逐条价值论证 · 计数锚点机械核验 · 转换类/新增类验收标准不可混用）。

⚠️ **但重述之后没有人再去回答那个更根本的问题**：**「新增有意义的 dual 文件」在
本仓实际语料里到底还有多少？** ⇒ ⇒ 本文档用**实测**回答它。

---

## 2. 实测：剩余语料盘点（2026-11-02）

### 2.1 判据（先固定，避免盘点时自定标准）

一个组件 `C` 值得「独立价值的双后端覆盖」需同时成立：

- **E1 可观测**：存在**顶层端口**让两条后端给出同一读数；
- **E2 非平凡运行期语义**：读数随输入/状态变化（非常量、非纯结构断言）；
- **E3 尚无 dual 覆盖**。

### 2.2 路线 A（转换既有文件）—— **实测剩余 12 个，全部成立**

plan §2.1 的权威筛选（`ch_device|ch_module` − `STATIC_REQUIRE` − `>64-bit ch_out`）
重跑得候选 38 ⇒ 逐个裁决：

| 分类 | 数量 | 说明 |
|---|---|---|
| 🔒 契约锁定（9 个 `test_verilator_*` MUST 保留原状） | 7 | capability `dual-backend-test-coverage` 明文 |
| ⛔ 价值判据排除 | 3 | `test_axi_pwm_simple`（零判别力且已被覆盖）· `test_jit_concat` · `test_interpreter_jit_compat`（身份是 **JIT-vs-interpreter** parity ⇒ 正交） |
| → 归路线 B（G 批，无 runtime 语义） | 3 | `test_lnode_entry_set`（端口取值 = 0）· `test_multithread`（无顶层端口）· `chlib/test_phase4_integration`（无顶层端口） |
| ⚠️ **已有 `_behavior` 兄弟**（转它 = 重复计数） | 8 | `test_axi_gpio/i2c/spi/timer/uart` · `test_blt_debug` · `test_rv32i_pipeline` · `chlib/test_assert` |
| ✅ **路线 A 真可转** | **12** | 见下表 |

**12 个的实测复核**（全部：无 dual 兄弟 · 自身不含 dual pair · 有运行期取值）：

| 批 | 文件 | 运行期取值 | 冷编译单价 |
|---|---|---|---|
| **D** | `test_mem_comb_read` · `test_mem_connection` · `test_mem_timing` | 14 / 4 / 24 | ⚠️ **72 s/pair**（实测） |
| **E** | `test_bundle_connection` · `test_bundle_master_slave` · `chlib/test_cache_complete` | 8 / 2 / 54 | 中 |
| **F** | `chlib/test_onehot` · `test_operation_results` · `test_module_advanced` · `chlib/test_forwarding` · `chlib/test_hazard_complete` | 27 / 52 / 7 / 39 / 10 | ⚠️ 最高 |
| — | `test_stream_builder` | 2 | 低 |

⚠️ **重要限定**：这 12 个里**多数没有自带「顶层 `ch_out`」的 DUT** ⇒ ⇒ 必须**新建
带端口的 DUT 包一层** ⇒ ⇒ **它们在验收标准上属于「新增类」（须手推期望）**，
**不是**「转换类」（逐位等价）⇒ ⇒ plan §4 那条「两类验收标准不可混用」的界线，
在这 12 个上**不成立** ⇒ ⇒ 逐个标注。

### 2.3 路线 B（新增有意义的 dual 文件）—— **实测剩余 ≈ 2–4 个**

**既有文件**的池已见底：

| 候选 | 实测 | 判定 |
|---|---|---|
| `tests/test_stream_operators.cpp` | **15 条断言全是结构性**（`impl()` / `num_srcs()` / `src(0)`）+ 裸 context | ❌ 硬转只会制造零判别力的假覆盖 |
| `tests/test_lnode_entry_set.cpp` | 端口取值 = 0 | ❌ |
| `tests/test_multithread.cpp` | 无顶层端口 | ❌ |
| `tests/chlib/test_phase4_integration.cpp` | 无顶层端口 | ❌ |
| `tests/test_axi_i2c.cpp` | 已有 `_behavior` 兄弟 | ❌ 重复计数 |

**真正的新写目标**（`include/chlib/**` 中「有运行期语义 + dual 覆盖率 = 0」者）：

| 组件 | 既有测试 | dual 覆盖 | 本会话已做 |
|---|---|---|---|
| `combinational`（priority/binary encoder + decoder） | `chlib/test_combinational.cpp`（13 TC / 56 取值） | 0 | ✅ 路线 B 第 4 批 |
| `converter`（onehot / BCD / gray） | `chlib/test_converter.cpp`（6 TC / 25 取值） | 0 | ✅ 路线 B 第 4 批 |
| `arithmetic`（carry / borrow） | `chlib/test_arithmetic.cpp`（16 TC / 68 取值） | 0 | ✅ 路线 B 第 5 批 |
| `logic` + `sequential`（门 / 寄存器 / 计数器 / 边沿） | `chlib/test_logic.cpp`(17) · `test_sequential.cpp`(10) | 0 | ✅ 路线 B 第 5 批 |
| `onehot`（编解码） | `chlib/test_onehot.cpp`（7 TC / 27 取值） | 0 | ⚠️ 与 F 批的 `test_onehot` **同一组件** ⇒ 归 F 批 |
| `memory`（RAM / DualPortRAM / FIFO） | 2 个引用 | 0 | ⚠️ **待复核**（D 批 mem 类） |
| `pipeline`（`PipelineStage` / `PipelineStageDual` / `pipeline_reg`） | 1 个引用 | 0 | ⚠️ **待复核** |

⇒ ⇒ **已完成 2 个（第 4/5 批）+ 剩余待复核 2 个** ⇒ ⇒ **路线 B 剩余 ≈ 2–4 个**。

### 2.4 hand-crafted 白名单 —— **已穷尽**

`examples/riscv-mini/tests/` 的 **17** 个文件逐个查 `InterpreterBackend` /
`VerilatorBackend`：

- `test_rv32i_soc_dual_parity.cpp` — Interp=5 / Veril=9 / macro=0 ⇒ **已在白名单**
  （**唯一**一个 hand-crafted 真 dual）
- `test_cache_smoke.cpp` — macro=4 ⇒ **已按 macro 计入**
- 其余 **15** 个 — Interp=0 / Veril=0 / macro=0 ⇒ **全是 interpreter-only**

⇒ ⇒ **「扩白名单」这条出路机械上不可行**（白名单里只有 1 个，无遗漏）。

---

## 3. 结论：诚实上限的**实测值**

| 量 | 值 | 来源 |
|---|---|---|
| 现状（2026-11-02 实测） | **44** | `scripts/verify_dual_backend_parity.sh` |
| 路线 A 剩余 | **+12** | §2.2（实测复核） |
| 路线 B 剩余 | **+2 ~ 4** | §2.3（待复核 2 个 + 可能的同组件拆分） |
| **实测可达上限** | **58 ~ 60** | 44 + 12 + (2~4) |

⇒ ⇒ ⚠️ **实测上限 ≈ 58–60**，而阈值 = **60** ⇒ ⇒ **落在阈值边缘**，
**不是「差很多」，也不是「稳过」**。

⚠️ **与第一版的对比（实测订正）**：2026-11-02 的第一版估算说「路线 B 还有 8 个」
⇒ ⇒ **实测证明只有 2–4 个** ⇒ ⇒ 原因是：**既有测试文件的池已见底**，
而**真正的新写目标**（`combinational` / `converter` / `arithmetic` / `logic` /
`sequential`）**恰好只有 5 个**，本会话已清掉 4 个 ⇒ ⇒ **剩下的空间比乐观估算小得多**。

---

## 4. 交给 planner 的三个选项

| # | 选项 | 内容 | 代价 / 风险 |
|---|---|---|---|
| **①** | **阈值保持 60** | 路线 A 的 12 个**全做** + 路线 B 剩余的 2–4 个**全做** ⇒ ⇒ 落在 **58–60** ⇒ ⇒ **可能刚好够，也可能差 1–2** | 若差 1–2 ⇒ 需再找目标 ⇒ 而「再找」有**滑向凑数**的风险（须逐条论证） |
| **②** | **阈值下调到实测上限** | 改为 **≥ 58**（或按最终实测值定）⇒ ⇒ 与「阈值 ≤ 诚实上限」的不变量**再次自洽** ⇒ ⇒ 且**诚实**（不再有「差几个」的悬空） | 目标数字变小 ⇒ 「全量覆盖」的叙事弱化 |
| **③** | **改口径**（不只是改数） | 例如：把「新增有意义的 dual 文件」的**判定**机械化（如「零 dual 覆盖 + 有顶层端口 + 运行期取值 > 0」）⇒ ⇒ 「诚实上限」变成**可枚举、可复算**的数 | 需要新的机械护栏（可复用 `verify_dual_backend_parity.sh` 的口径定义） |

### 4.1 build 的建议：**②（若确定只做现有语料）或 ① + 严格的价值闸门**

理由：

- ① 的**风险不是技术风险，是治理风险** —— 「还差 2 个」会制造凑数压力 ⇒ ⇒
  而 `critic SHOULD_FIX #2` 已明确指出这种压力 ⇒ ⇒ **建议把闸门写死**
  （每个新增文件 MUST 有独立价值论证 + 变异证明判别力 + 期望值手推）
  ⇒ ⇒ 这三条**本会话每批都在执行**，可作为硬标准；
- ② 的**优点是诚实** —— 与本仓一贯的「如实登记」文化一致
  （plan §6.1.1 的第三条出路本质就是这个）；
- ③ 值得**单独考虑**，因为它能把「诚实上限」从**一次性盘点**变成**持续可复算的量**
  ⇒ ⇒ 但它是**判据口径变更** ⇒ ⇒ owner = planner。

---

## 5. 给 planner 的三个「必须知道」

1. ⚠️ **本会话的 5 批（路线 B 第 1–5 批）共挖出 4 个真缺陷**（`priority_encoder`
   注释与实现相反 · `add_with_carry`/`sub_with_borrow` 的 `cin` 被忽略 ·
   `edge_detector` 的 `pos`/`neg` 恒 0 · `johnson_counter` 恒 0）⇒ ⇒
   ⇒ **它们全部是「既有测试恰好用建图期常量 ⇒ 从未暴露」型** ⇒ ⇒
   ⇒ **dual 覆盖的边际价值已被实证**：它不只是「一致性检查」，而是**缺陷发现机制**。
   ⇒ ⇒ 这个事实**支持**继续推进（哪怕阈值下调），因为**判据的价值不取决于计数**。
2. ⚠️ **CI 墙钟的真实约束**：P2 已落三个杠杆（verilator 构建缓存 = **10–30 min → 2 s**
   · fixture 缓存 · 分片），但**分片实测是负结果**（按序号分片不均衡且不稳定）⇒ ⇒
   ⇒ 真实的墙钟约束是「**D 批 mem 类 72 s/pair**」⇒ ⇒ 路线 A 的 D 批（3 个）
   ⇒ ⇒ **是 CI 预算的主要消耗**，需实测后回填。
3. ⚠️ **「诚实上限」若含「新增文件」则无上界** ⇒ ⇒ **任何以「还差 N 个」为动机的
   推进都有滑向凑数的风险** ⇒ ⇒ 建议无论选哪条，都把 §4.1 的三条硬闸门
   （价值论证 · 变异证明 · 手推期望）写成**判据的一部分**，而不只是**工作纪律**。

---

## 6. 变更记录

| 日期 | 变更 | 依据 |
|---|---|---|
| 2026-11-02 | **创建**：P1 的第二版决策材料（用**实测**回答「新增有意义的 dual 文件在本仓还有多少」）⇒ ⇒ 路线 A 剩余 **12**（复核全部成立）· 路线 B 剩余 **2–4**（第一版估 8，**实测偏低 4–6**）⇒ ⇒ 实测可达上限 **58–60** ⇒ ⇒ 三个选项（阈值 60 / 下调到实测上限 / 改口径使其可枚举） | 2026-11-02 的量化盘点（§2）+ 路线 B 第 1–5 批的实测 |
