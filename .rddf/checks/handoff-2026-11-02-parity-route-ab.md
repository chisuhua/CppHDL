# 交接快照：objective-test-coverage-100pct（路线 A/B 推进）

- **时间**：2026-11-02（builder，第 3 次交接 · 同一会话的第 2 批）
- **objective**：`.rddf/roadmap/objectives/objective-test-coverage-100pct.md`
- **plan**：`.rddf/plans/dual-backend-full-coverage-2026-11.md`
- **用途**：下一次会话**直接从这里继续**，不需要重新盘点。

---

## 1. 当前计数（实测，全部已对齐 AGENTS.md 锚点）

| 量 | 值 | 命令 |
|---|---|---|
| **parity 源文件数** | **56**（目标 60，差 **4**） | `bash scripts/verify_dual_backend_parity.sh` |
| parity macro 调用数（PAIRS） | **258** | 同上 |
| `CTEST_TOTAL` | **233** | `cd build && ctest -N \| tail -1` |
| `VERILATOR_E2E_TOTAL` | **65** | `ctest -N -L verilator_e2e \| tail -1` |
| non-perf | **222** | `ctest -N -LE perf \| tail -1` |
| `tests/test_*.cpp` | **130** | `ls tests/test_*.cpp \| wc -l` |
| `tests/chlib/test_*.cpp` | **47** | `ls tests/chlib/test_*.cpp \| wc -l` |
| `tests/chlib/test_*.cpp` | **44** | `ls tests/chlib/test_*.cpp \| wc -l` |

⚠️ 工作区里有一批**并发会话**的文件（`hierarchical-rtl-output`：`docs/adr/ADR-058..062`、
`docs/usage_guide/05-component-design.md`、`openspec/changes/hierarchical-rtl-output-2026-11/` 等）
⇒ ⇒ **提交时 MUST 按路径显式 `git add`，MUST NOT 用 `git add -A`**。

## 2. 已完成批次

| 会话 | 内容 |
|---|---|
| 第 1 会话（11 commit） | 路线 B 第 6 批 · `sread` 禁读拍修复与归档 · 路线 A **D 批第 1 个**（`test_mem_connection_dual`，4 pair） |
| 第 2 会话（2 commit） | 路线 A **D 批第 2 个**（`test_mem_comb_read_dual`，2 pair / 14 断言）+ 归档 + 台账 47 → **49** |
| 本会话（10 commit） | ① 路线 B **新文件批**：`test_bitwise_dual`（2 pair / 100 断言）· `test_onehot_dual`（3 pair / 16）· `test_forwarding_dual`（1 pair / 10）⇒ 49 → **52**；② 路线 A **D 批第 3 个**：`test_mem_multiplex_dual`（2 pair / 14）⇒ 52 → **53**；③ 路线 B：`test_arithmetic_advance_dual`（3 pair / 104）⇒ 53 → **54**；④ 路线 A **E 批**：`test_cache_async_dual`（3 pair / 73）⇒ 54 → **55**（**含 spec 订正**：用事实裁决 Async 禁令）；⑤ 路线 A **F 批**：`test_hazard_complete_dual`（1 pair / 112，零 DUT 复制）⇒ 55 → **56** |

parity 轨迹：**37 → 49 → 52 → 53 → 54 → 55 → 56**。

## 3. 🔴 每个批次的**前置条件**：可转性 4 问（已固化进 plan §4）

**写代码之前**必须先答完 4 问；任一为「否」⇒ 不转（只转通过的部分）：

| # | 判据 | 反例（实测） |
|---|---|---|
| ① | 语义**是否已被别的 dual 文件覆盖**？（按语义 + 判别形态搜，**不是**按文件名） | `test_mem_comb_read` 的时序对已被 `test_mem_read_timing_dual` 以**同一形态 + 同一组期望值**覆盖 ⇒ 重复 ⇒ **初稿 5 pair 砍到 2** |
| ② | 断言形态**对目标维度有判别力**吗？（变异实测） | 「输入地址 + `tick()` 后读」对 0/1-cycle **零判别力**（T2） |
| ③ | **与后端无关**吗？（编译期 / codegen 文本断言 / 构图期拒绝） | `aread Verilog codegen emits assign` ⇒ 零增量 |
| ④ | **形态上不可转换** ⇒ 是否值得**新写**？ | 本批 3 个目标**全部**不可转换（裸 context / SECTION / 函数式接口）⇒ ⇒ **必须允许新写路线** ⇒ ⇒ 期望值**手推** ⇒ ⇒ 笔误风险 ↑（本批实测到 1 次，被双后端一致地证伪） |

### 🔴 4 问之外的**第 5 条纪律**：**「预测无效的变异」也必须真跑**

D 批第 3 个文件里，我按 T2 边界**预测** N4（`aread` → `always_ff`）是**无效变异（恒绿）**
⇒ 若只跑「预期会红」的变异，就**漏掉**了一条**框架层不变量**（见 §6 T5）。
⇒ ⇒ **纪律**：变异清单里**允许且应当**包含「我认为不会红」的条目，跑完**如实记录**
（红 ⇒ 追机制；绿 ⇒ 才算证明边界）。**预测 ≠ 事实**。

## 4. 📊 实测覆盖矩阵（路线 B 的"新文件"池 = **7** 个，原估 1–3 **被低估**）

「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集 × `include/chlib/*.h` 求交 ⇒
**零 dual 覆盖**的 chlib 组件：

| 组件 | 状态 | 备注 |
|---|---|---|
| `chlib/bitwise.h` | ✅ 本批 | 9 条 select 链 / 移位 / 掩码 |
| `chlib/onehot.h` | ✅ 本批 | 译码/编码 select 链 |
| `chlib/forwarding.h`（`ForwardingMux`） | ✅ 本批 | ADR-047 D4 的 `is_jump` 返回地址 |
| `chlib/arithmetic_advance.h` | ✅ 本会话 | CLA / CSA / Wallace / Booth / 非恢复除法 / 定点；**挖出 3 个既有缺陷**（见 §5.A） |
| `chlib/selector_arbiter.h` | ⬜ | `tests/chlib/test_selector_arbiter.cpp` |
| `chlib/stream_arbiter.h` | ⬜ | 3 个 arbiter（priority / round_robin） |
| `chlib/stream_pipeline.h` | ⬜ | 3 个 pipe（ADR-055 刚修过 ⇒ **回归价值高**） |
| `chlib/stream_width_adapter.h` | ⬜ | 位宽适配 |

⚠️ **计划漏项**：`tests/chlib/test_bitwise.cpp` 与 `tests/chlib/test_arithmetic_advance.cpp`
**不在**任何既有批次清单里 ⇒ ⇒ 「新写」路线**不受批次表约束**（组件零覆盖即可立项）。

## 4.5 🔴🔴 **实测订正：「路线 A 剩余 9」里真正可转的只有 3 个**

objective §2 的台账写「路线 A 剩余 12 − 3 已落地 = **9**」。逐个复核实测后
（2026-11-02 · build 交付，判据仍归 planner）：

| 文件 | 台账批 | 实测 | 判定 |
|---|---|---|---|
| `tests/test_bundle_connection.cpp` | E | 17 TC / **0 个 `ch_out`** / 16 个纯结构（0 tick） | ❌ **不可转**（无顶层输出端口 ⇒ Verilator 无法观测；断言与后端无关） |
| `tests/test_bundle_master_slave.cpp` | E | 9 TC / **0 个 `ch_out`**（bundle 字段是 `proxyimpl`，不是端口） | ❌ **不可转**（TB 走 `set_bundle_value` 直写 `data_map_`） |
| `tests/chlib/test_cache_complete.cpp` | E | 13 TC / 45 tick；Sync 6 个 vs Async 4 个 + 3 个对比 | ✅ **部分可转**（Async 4 个是**新覆盖**；Sync 大半与 `test_cache_sync.cpp`（10 pair）重复） |
| `tests/test_operation_results.cpp` | F | 9 TC / 9 个 `ch_out`；其中 2 个裸 context + 1 个纯 C++ | ⚠️ **部分可转**（需逐案查重：算术/位运算/比较/移位多已被 `test_arithmetic_carry_dual` / `test_arithmetic_encoding_dual` / `test_sequential_counters_dual` 覆盖） |
| `tests/chlib/test_hazard_complete.cpp` | F | 8 TC；DUT `riscv::HazardUnitComplete` 头里有 **7 个 `ch_out`**（≤32b）且**纯组合** | ✅ **可转**（**零 DUT 复制**，与 `test_forwarding_dual` 同型）⚠️ 生产消费者 = 0 |
| `tests/test_stream_builder.cpp` | — | 7 TC；**已存在 `test_stream_builder_backpressure_dual.cpp`** | ❌ **重复计数**（4 问 ①） |
| `tests/chlib/test_onehot.cpp` | F | 21 SECTION；**已被本会话 `test_onehot_dual.cpp` 覆盖** | ❌ **重复**（4 问 ①） |
| `tests/chlib/test_forwarding.cpp` | F | **已被本会话 `test_forwarding_dual.cpp` 覆盖** | ❌ **重复**（4 问 ①） |
| `tests/test_module_advanced.cpp` | F | one-hot 译码 ⇒ **已被 `test_onehot_dual.cpp` 覆盖** | ❌ **重复**（4 问 ①） |

⇒ ⇒ **路线 A 真正剩 3 个**（cache_complete 的 Async · operation_results 的部分 · hazard_complete），
不是 9 ⇒ ⇒ ⇒ **阈值 60 的差额（54 → 60 = 6）必须由「路线 B 剩 4」+「路线 A 剩 3」共同补足**
= 最多 7 个候选 ⇒ ⇒ **margin 仅 1** ⇒ ⇒ **任何一个被 4 问否决，就补不满** ⇒ ⇒
**这条必须上报 planner**（build 不擅自改判据）。

⚠️ **另登记一个口径盲区（给 planner）**：现行口径数**源文件数** ⇒ ⇒
**在既有 dual 文件里加 pair 对闸门零贡献**（但边际价值可能很高，例如
`test_mem_read_timing_dual` 只有 1 pair）⇒ ⇒ 口径**奖励「开新文件」而非「加深既有文件」**
⇒ ⇒ 与 objective §2 的三条防凑数机制**方向相反**（那三条要求「有独立价值」）。

## 5. 下一步（按优先级）

### A. ~~`chlib/arithmetic_advance.h`~~ ✅ **已完成**（`test_arithmetic_advance_dual`，3 pair / 104）
**变异 12/12 全抓**（每个断言组都有专属变异）。🔴 **挖出三个既有缺陷 / 名不副实（未修，需独立立项）**：
1. **`wallace_tree_multiplier` 与 `booth_multiplier` 函数体逐字相同**（都是 `a*b`）
   ⇒ 既不是 Wallace 树、也不是 Booth 编码 ⇒ 与 `fwft_fifo` / `stream_arbiter_sequence`
   同型。⚠️ 处置提醒：**它们不是「同一个函数」而是两个模板**（各自的硬件实例都能被观测）
   ⇒ 故本批用「两者逐位相同」把它**钉住**；改名/重写会改 RTL 结构 ⇒ 需 **ADR**。
2. **`carry_save_adder` 的 `carry` 静默截断**：签名 `ch_uint<N>`，内部 `ch_uint<N+1> temp`
   ⇒ `temp << 1` 的**最高位（真 carry-out）被丢**。本批**按签名行为**断言并在向量表里
   刻意放 `(0x80,0x80,0x00)`（只有 carry-out 置位）⇒ 截断被显式钉住。
3. **`fixed_point_divider` 的除零保护是死代码**：`if (b == 0_d)` 是 **C++ 控制流作用于
   硬件值** ⇒ `ch_bool::operator uint64_t()` 对非 const 恒返 0（`src/lnode/bool.cpp:44-49`）
   ⇒ 建图期恒假 ⇒ 硬件是裸 `(a<<Q)/b`。⚠️ **本批故意不喂 b==0**（interpreter 是原生
   整数除法 ⇒ **SIGFPE**）⇒ 谁要修它，MUST 同时把该守卫改成**真 `select`**
   （参照 `non_restoring_divider` 的写法）。

### A'. 路线 B 剩余候选（**剩 3 个**，各自先过 4 问）
`chlib/selector_arbiter.h` · `chlib/stream_arbiter.h` · `chlib/stream_pipeline.h` ·
`chlib/stream_width_adapter.h`（⚠️ 后两个与 ADR-054/055 刚修过的 ready 语义重叠 ⇒
**回归价值高**，但也最可能与既有 dual 文件（`test_stream_demux_fork_dual` /
`test_stream_builder_backpressure_dual`）语义重复 ⇒ 必须逐条过 4 问 ①）。

### B. `tests/chlib/test_cache_complete.cpp`（E 批）—— **能解一个 spec 矛盾**
⚠️ `openspec/specs/dual-backend-test-coverage/spec.md` **自相矛盾**：
- Requirement「cache 双后端 MUST 按 Sync/Async 分类」明写：Async dual-run **可以**走
  parity，「『Async 强制 parity 会永远红』这一前提**已不成立**，禁令解除」；
- 但另一个 Requirement 的 Scenario「spec R6 排除清单遵守」仍写：runtime test 涉及
  `ICacheAsync`/`DCacheAsync` ⇒ **MUST NOT** 用 `DUAL_BACKEND_TEST_PAIR`（理由还是
  那条**已消除**的 skew）。
⇒ ⇒ **用事实裁决**：把 `test_cache_complete.cpp` 的 **4 个 Async 用例**转成 parity
⇒ 若**真的绿** ⇒ 矛盾按事实消解（并把该 Scenario **原地订正**，标题 MUST NOT 改 ——
CLI 按标题匹配 Scenario）；若红 ⇒ 说明前提并未消除 ⇒ 保留禁令。
⚠️ `test_cache_sync.cpp` 已是 dual（10 pair）⇒ Sync 用例大概率重复 ⇒ 只转 Async 那批。

### C. ~~D 批剩 1：`tests/test_mem_timing.cpp`~~ ✅ **已完成**（`test_mem_multiplex_dual`）
7 个用例逐条过 4 问后只有 2 个形态有增量（mux 嵌读端口 / 同一 mem 两个读端口），
其余 5 个**已被** `test_mem_sread_disable_dual` / `test_mem_comb_read_dual` /
`test_mem_connection_dual` 覆盖 ⇒ ⇒ **D 批已清空**。

### D. E/F 批剩余（**逐个先过 4 问**）
`test_bundle_connection` · `test_bundle_master_slave` · `test_operation_results`
· `test_module_advanced`（one-hot 译码 ⇒ **已被**本批 `test_onehot_dual` 覆盖 ⇒ 重复）·
`test_rv32i_pipeline`（**已被** `test_rv32i_pipeline_behavior` 的 16 pair 逐位覆盖 ⇒ 重复）·
`chlib/test_forwarding`（**已被**本批覆盖）· `chlib/test_hazard_complete`
（`HazardUnitComplete` 有 7 个顶层端口 ⇒ 可转；但**生产消费者 = 0** ⇒ 价值中）·
`chlib/test_onehot`（**已被**本批覆盖）⇒ ⇒ **F 批 62 个用例里，实际可转的可能只有个位数**。

### E. planner 裁决（**build 只交数字，不改判据**）
- 现状 **53** · 路线 A 剩余 **9**（12 − 3：2 个已证重复 + 1 个已落地）· 路线 B 候选**剩 4**（7 − 3 已落地）
- ⇒ 上限算术：**乐观 66** / 按实测命中率 ~50% 折算 ≈ **60~62** ⇒ ⇒ 阈值 60
  **从「不可达」变成「贴边可达」** ⇒ **需 planner 重算**（台账已更新 + 标注）。
- 🔴 **另请 planner/oracle 看待办**：T5 那条**框架层不变量**（codegen `always_ff`
  ⟺ Simulator 时序表非空）需要**独立立项**做全仓交叉审计 —— 它与覆盖无关，
  但会**静默**吞掉生成的时序逻辑。

## 6. 陷阱清单（**T1–T5**；T2 已收窄表述，T5 是本会话挖出的框架层缺口）

### 🔴 T1：`ctest` **不会**自动重新构建
改了 `src/**` / `include/**` 后只重建被点名的 target 就跑 ctest ⇒ **假绿**。
⇒ 跑 ctest 前 MUST `cmake --build build -j$(nproc)`（已写入 AGENTS.md COMMANDS 段）。
⚠️ 变异测试只需重建**被观测的** target（`libcpphdl.a` 是**静态库** ⇒ 替换归档不影响
已在跑的其他进程）⇒ 但**最终验收**仍须全量重建。

### 🔴 T2：靠「`tick()` 后读」**无法**区分 0-cycle 与 1-cycle
机制（已查清）：地址是输入端口时，TB 在 `tick()` **之前**就写入 `data_map_` ⇒ 沿上
采样到的已是新地址 ⇒ 两种时序**观测值相同**；「tick 之间观测」也无效
（`get_port_value` 只读 `data_map_`，`set_input_value` 不触发组合求值）。
⇒ ⇒ **唯一可判别的形态 = 地址由「同一条沿提交的 `ch_reg`」驱动**。

### 🔴 T3：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里**花括号内的逗号**会被当参数分隔
用例表 MUST 放**文件作用域**（只有圆括号保护）。

### 🔴 T4（本会话新增）：**变异注入器必须 fail-closed**
同一段源码文本在头文件里常出现**多次**（例：`onehot.h` 的 `one_hot_val` 计算在函数式
与模块式两处逐字相同）⇒ ⇒ 注入器的模式若**不唯一命中**必须**拒绝执行**
（本会话实测：O2 首次被 `MUTATE_FAIL` 拒绝 ⇒ 补足上下文后重跑才命中）⇒ ⇒
否则「变异没注入 ⇒ 跑旧二进制 ⇒ 假绿」。

### 🔴🔴 T5（本会话新增，**框架层**）：无时序节点的设计 ⇒ 时钟从不翻转

`src/simulator_init.cpp:153` 有 `if (sequential_instr_list_.empty()) default_clock_instr_ = nullptr;`
⇒ ⇒ `Simulator::tick()` 里 `eval()`（进而 `eval_sequential()` ⇒ Vtop 时钟 0→1→0）
**只在 `default_clock_instr_` 存在时**才调用 ⇒ ⇒ **整个设计没有任何时序节点**
（无 `ch_reg` / 无 `sread` / 无 mem 写口）⇒ **时钟从不翻转** ⇒ 生成的 Verilog 里
**任何 `always_ff` 永不触发** ⇒ 读到 **0**，**无任何报错**。

⇒ **不变量**：「codegen 发射了任一 `always_ff`」 ⟺ 「Simulator 时序表非空」。
⇒ 实测证据：`tests/test_mem_multiplex_dual.cpp` 的 N4 变异（`aread` → `always_ff`）
让 pair 2（只有 `aread` + `ch_out`）**全部读 0**，而同文件的 pair 1（用 `sread`
⇒ 时序表非空）**完好**；把变异生成的 `top.v` 拿**独立 harness** 驱动 ⇒ **正确**
⇒ ⇒ 错在**驱动路径**，不在 Verilog 语义。
⇒ ⚠️ 是否还有**其它**构造在漂移**未审计** ⇒ **独立立项**（全仓 codegen × instr 分类交叉审计）。

### 既有陷阱（仍有效）
- **子组件 MUST 用 `ch::ch_module<T>`**（裸构造的 `describe()` 不会被调用 ⇒ SIGSEGV）。
- **端口连线 MUST 用 `<<=`**；⚠️ 但 **`ch_out` 的 `operator=` 是真数据流边**
  （`ch_logic_out::operator=` → `set_src`，`include/core/io.h:113-123`）⇒ ⇒
  库组件里的 `io().out = expr` **是正确接线** ⇒ ⇒ **库组件可直接当 dual 的 DUT**
  （零 DUT 复制，先例：`tests/test_blt_debug_behavior.cpp`）。
- **`ch_in<ch_bool>` 不能绑 `const ch_bool&`**。
- **变异 MUST 先确认仍可编译**；**还原用 `cp`**（刷新 mtime）。
- **无效变异 / 判别力边界 MUST 如实登记**（本会话已登记 2 条边界 + 1 条期望值笔误）。
- **N2′**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
  （`./build/tests/<t> "*\[interpreter\]"` 可直接取单后端数）。
- **AGENTS.md 的 hunk 级隔离**：并发会话也在改它 ⇒ 提交前**逐 hunk 复核**
  `git diff --cached AGENTS.md`。

## 7. 关键文件索引

| 路径 | 说明 |
|---|---|
| `tests/chlib/test_bitwise_dual.cpp` | 本会话新增（bitwise 6 检测器 + 3 位操作） |
| `tests/chlib/test_onehot_dual.cpp` | 本会话新增（one-hot 译码/编码，零 DUT 复制） |
| `tests/chlib/test_forwarding_dual.cpp` | 本会话新增（ForwardingMux 三档位 + `is_jump`） |
| `tests/test_mem_comb_read_dual.cpp` | 第 2 会话新增（mem 写口 × aread） |
| `tests/test_mem_multiplex_dual.cpp` | 本会话新增（mux 嵌读端口 + 同一 mem 两个读端口；**N4 钉住 T5 不变量**） |
| `tests/test_mem_read_timing_dual.cpp` | ⚠️ **已覆盖 aread/sread 时序**（别重复转换） |
| `tests/chlib/test_cache_sync.cpp` | ⚠️ **已是 dual**（10 pair）· Async 的 spec 矛盾见 §5.B |
| `openspec/specs/dual-backend-test-coverage/spec.md` | ⚠️ **Async 那条自相矛盾**（见 §5.B） |
| `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` | §2 判据 + 台账（现状 **52**） |
| `.rddf/plans/dual-backend-full-coverage-2026-11.md` | §4 批次 + **4 问前置条件** + **覆盖矩阵** · §5 墙钟账本 |
| `/tmp/opencode/mutate_*.py` | fail-closed 变异注入器模板（**会话级，不在仓内**） |
