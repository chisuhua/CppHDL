---
name: memory-timing-contract-2026-11
priority: P1
phase: phase-6
category: cpu
type: feature
状态: draft
进度: B3 未开工 · ADR-068 **Accepted**（T2′ = (F) 字节道拆分存储）· ✅ **T2 已裁决**（**ADR-069** · **Accepted · 2026-11-12** · 决策 = **候选 ②（A0）** = 地址照旧 + MEM/WB 延后 1 拍 ⇒ ⇒ `read-new` 条件自动成立）· ✅ 公开面前置门已登记（R6 · 2026-11-12）⇒ ⇒ **只差 change 立项**（⚠️ 原载体 = 新立 ADR-069 · Parent = ADR-068 ⇒ ⇒ **禁**改写刚 Accepted 的「本 ADR 不裁决 T2」承重句 ⇒ ⇒ 裁决 MUST 与 ADR-068 的 D4 一并读：若 T2 选 A1 ⇒ MUST 追加同址处置（读数据旁路 或 一拍停顿）) ⇒ ⇒ T2 相关项不可开工
主题: 已知功能缺口清零
依赖: rv32i-commit-path（时序金标基线）· firmware-load-chain-2026-11（装载路径先做）
来源: 用户 2026-11-10 拍板「#12 改成寄存器读」+ planner 实测（组合读 ⇒ 推断不出 BRAM）
生成时间: 2026-11-10
evidence: probe:include/cpu/pipeline/rv32i_tcm.h#L100
---

# Improvement: 存储器**时序契约**（D-TCM 组合读 → 寄存读 + MEM 级 stall/反压通路）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（未开工 ⇒ 归 **B3**）⇒ ⇒ 🚪 **入口门**：ADR-068 已 Accepted ✅ · **T2（MEM 数据对齐 A/B）待 architect 裁决**（R5）
- **审计（2026-11-13 · planner 独立复核）**: ✅ **确认未开工**（`include/cpu/pipeline/rv32i_tcm.h:113` 仍 `memory.aread(...)` ⇒ ⇒ D-TCM **仍是 0 拍组合读** ⇒ ⇒ 产物仍是 4096:1 组合 mux ⇒ ⇒ 「可推断 BRAM」**未达成** · `rv32i_pipeline.h:61` 的 `data_ready` **仍只有声明**（流水线内部仍无消费者）⇒ ⇒ 结论与 frontmatter「`draft` / B3 未开工 / ADR-068·069 已 Accepted」**一致** ⇒ ⇒ 本轮**无需订正**
- **用户裁决（2026-11-10）**: 「#12 改成寄存器读」⇒ ⇒ D-TCM 从 `aread`（0 拍组合）改为 `sread`（1 拍寄存读）

---

**主题**: 已知功能缺口清零

## 架构依据

### A.1 实测事实（2026-11-10）

| # | 事实 | 证据 |
|---|---|---|
| ① | D-TCM 用 `aread`（**0 拍组合读**）⇒ ⇒ MEM 级在**当前拍**采样 `rdata` | `include/cpu/pipeline/rv32i_tcm.h:113`（`old_word <<= memory.aread(io().addr)`）· `:128`（`io().rdata <<= old_word`） |
| ② | ⚠️ 产物是**组合 mux 读**：`assign top_dtcm_mem_aread_data_proxy = top_dtcm_dtcm_memory[top_dtcm_unnamed_input];` ⇒ ⇒ **4096:1 组合 mux** ⇒ ⇒ **FPGA 上推断不出 BRAM**（BRAM 需寄存读）⇒ ⇒ ASIC 上是巨大 mux 树<br>🔴 **metis 订正（决定性）**：**真正卡住 BRAM 的不是读口，而是**写口**的 RMW 组合读**（`rv32i_tcm.h:112-117` 的 `old_word <<= memory.aread(io().addr)`）⇒ ⇒ 即使读口改成 `sread`，写口前仍挂着一个 4096:1 组合 mux ⇒ ⇒ **改读口本身不足以达成「可推断 BRAM」** ⇒ ⇒ 必须另有写口处置（见 T2′） | 本轮实测产物 `rv32i_soc.v:1995` · 数组声明 `:1428-1429` · `rv32i_tcm.h:112-117` |
| ③ | 对比：I-TCM 用 `sread`（**1 拍寄存读**）⇒ ⇒ 可推断 BRAM ⇒ ⇒ **两个 TCM 的时序语义不一致** | `include/cpu/pipeline/rv32i_tcm.h:56`（`memory.sread(io().addr, ch_bool(true))`） |
| ④ | ⚠️ **metis 订正（原稿「仅声明处」是错的）**：`data_ready` **有 6+ 处实例化驱动**（`rv32i_pipeline_top.h:133` · `test_pipeline_debug.cpp:241` · `test_pipeline_minimal.cpp:33` · `test_pipeline_wrapper.cpp:56` · `test_riscv_tests_pipeline.cpp:134` · `test_wrapper_pipeline.cpp:43`）⇒ ⇒ 正确表述 =「**流水线内部无消费者**，且被 `dtcm.io().ready ≡ ch_bool(true)` **钉成常量 1**」⇒ ⇒ 「死端口」与「常量 1 的驱动源」是**两种不同的修法** | `include/cpu/pipeline/rv32i_pipeline.h:61`（声明）· `include/cpu/pipeline/rv32i_tcm.h:129`（`ready` 恒 true） |
| ⑤ | MEM 级假定存储器**零延迟** ⇒ ⇒ 架构上**接不了** 1-cycle `sread` cache | `include/cpu/pipeline/stages/mem_stage.h` 组合读 `mem_read_data` · `include/cpu/cache/d_cache_sync.h`（1 拍 `sread` + miss 多拍） |
| ⑥ | `ICacheSync` / `DCacheSync` 是**孤立组件**（流水线零实例化）⇒ ⇒ 接 cache 的前置不止时序 | grep：`include/cpu/pipeline/**` 无 cache 实例化 |
| ⑦ | ⚠️ 改 `aread`→`sread` ⇒ ⇒ **load 的数据晚一拍到** ⇒ ⇒ MEM/WB 装载点 MUST 延后 ⇒ ⇒ **load-use 停顿契约可能变化** ⇒ ⇒ 全部时序金标 MUST **手推**重算 | 本轮实测基线：`perf_instructions = 178` · `load_use_stalls = 6` · `control_stalls = 4` · `store 拍数 = 5` · 20/20 golden |
| ⑧ | ⚠️ D-TCM 的**字节写是「读-改-写」** ⇒ ⇒ 组合读改成寄存读 ⇒ ⇒ **写路径的时序也变**（改写用的 `old_word` 从"当拍组合值"变成"上一拍值"）⇒ ⇒ ⚠️ **可能破坏字节写语义** ⇒ ⇒ MUST 单独判据 | `include/cpu/pipeline/rv32i_tcm.h:109-117`（注释明说「组合读返回的是**边沿前**的值 ⇒ 合并结果正是「旧字 + 新字节」） |
| ⑨ | I-TCM 的取指侧**已经**是 1 拍 ⇒ ⇒ 取指侧有 `pc_lag` 对齐机制（`fetch_pc_reg`）⇒ ⇒ **数据侧无对应机制** ⇒ ⇒ 需新建 | `include/cpu/pipeline/rv32i_pipeline.h`（`fetch_pc_reg`）· AGENTS.md「RV32I imem 的 1 拍 `pc_lag`」 |
| ⑩ | 仓内**已有** 1-cycle cache 组件（`DCacheSync`）⇒ ⇒ 本项完成后接 cache 的**时序**前置满足（仍需 MEM stall 通路 + 地址译码） | `include/cpu/cache/d_cache_sync.h` |

### A.2 为什么组合读不可接受

② 是**上板可行性**问题，不是"风格"问题：
- FPGA：4096×32 的组合读 ⇒ 推断成 4096:1 mux ⇒ **面积爆炸 + 时序崩溃**（BRAM 不可用）。
- ASIC：同样是一大片 mux。
⇒ ⇒ 当前 D-TCM 是**"仿真正确、上板不可行"**的状态 ⇒ ⇒ **必须**在「能上板」这一目标下改。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **T1** | **D-TCM 读改寄存读**：`aread` → `sread`（1 拍）⇒ ⇒ 产物变成寄存读 ⇒ ⇒ **可推断 BRAM** |
| **T2** | **MEM 级数据对齐**（对应取指侧的 `pc_lag`）：⚠️ **metis 订正**：原稿选项 A 的表述**自相矛盾**（「地址提前」却写「延后一拍出地址」）且「地址提前」有**两个不同形态**（A1 = EX 级组合出地址 · A2 = EX/MEM 之间再加一级寄存器）⇒ ⇒ MUST 展开成**三选一 + 每项的预测计数器表**（见下）⇒ ⇒ 需 architect 裁决 |
| **T3** | ⚠️ **critic 第 1 轮追加**：`data_ready` 的语义重定义 MUST **随 T1 一起落** —— 「1 拍后数据有效 ⇒ `ready ≡ 1` 语义为真」**只在 `sread` 之后成立**；若先在 B0 改语义、后面才改实现 ⇒ ⇒ 中间态出现**「声称成立但不成立」**（比「既不满足旧也不满足新」更糟）⇒ ⇒ **T1 与 T3 的语义重定义同批**。<br>（**原内容见 §What Changes 段**——本单元只留摘要，避免与正文漂移）|
| **T4** | **字节写语义的时序修正**：D-TCM 的读-改-写在寄存读下 MUST 仍实现「旧字 + 新字节」⇒ ⇒ 写使能与读地址的**相位**关系 MUST 重新定义并写进契约<br>⚠️ **metis 追加判别力要求**：真正会静默出错的是 RMW 的 **`old_word` 相位**（取的是**哪个地址**的旧字）⇒ ⇒ MUST 有**跨地址**探针（连续两次 `sb` 到**不同字** ⇒ 不得串字；现有 `tests/test_rv32i_timing_dual.cpp:157-168` 的 addr16→addr17 拍表**恰好覆盖**这一形态 ⇒ **必须点名它会被打红**）；`sw`（strbe=0xF，覆盖全车道）**不能**当判据（会掩盖 `old_word` 错误） |
| **T6** | 🔴 **B4 修复（critic 第 1 轮 BLOCKER）：store→load 同址顺序契约** —— T2′(iii) 把 RMW 读改成 `sread` + **下一拍**写回 ⇒ ⇒ **紧邻同址 `sw`→`lw` 会读到旧值**（写落在 T+1 沿；load 的 `sread` 也在 T+1 沿采样 ⇒ 非阻塞赋值取旧值），而 **T3 已被拆出**（无 stall 通路兜住）、T4 只覆盖**跨地址**相位、T2 只覆盖 load 数据对齐 ⇒ ⇒ **无人覆盖**。<br>⇒ MUST：① **显式定义 read-during-write 语义**；② **判别性用例**：紧邻 `SW x1,0(x2)` / `LW x3,0(x2)`（**同址 · 零间隔**）⇒ 断言 `x3 == 写入值`；③ ⚠️ RISC-V 语义要求「store 后同址 load MUST 读到新值」⇒ ⇒ **本项实际只能选 `read-new`**（写回 MUST 与 load 采样同沿可见）⇒ `read-old` 只能作为**登记为已知限制**的降级，且会**违反 ISA** ⇒ 不推荐。<br>⚠️ **现固件零判别力**：`SW(4,0,0x100)`@word16 / `LW(5,0,0x100)`@word20（**隔 4 字**）；`SB`@48 / `LW`@52 ⇒ ⇒ 必须补紧邻形态 |
| **T5** |

### ❌ 不在本项

- **接 cache**（`ICacheSync`/`DCacheSync` 实例化 + AXI 主端）⇒ ⇒ 属**后续立项**（本项只把**时序前置**满足）
- **地址映射 / 装载链路** ⇒ 归 `firmware-load-chain-2026-11`（**先做**）
- **特权级 / ISA 缺口** ⇒ 归 `rv32i-isa-gap-closure-2026-11`
- **任何「为了让测试变绿」而弱化判据** ⇒ ⇒ 判据只可**更强**

---

## Why

### W.1 「仿真正确」与「能上板」是两件事，当前只满足前者

② ⇒ ⇒ 这是一颗**跑得对但综合不出来**的核 ⇒ ⇒ 若只按"仿真通过"验收 ⇒ ⇒ 会给使用者一个**错误的安全感** ⇒ ⇒ 仓内已有同类纪律（KNOWN LIMITATIONS 里大量"仅仿真模型"的登记）⇒ ⇒ 本项把 D-TCM 从"仅仿真模型"提升为"可综合"。

### W.2 两个 TCM 的时序语义不一致是**隐患**

③ ⇒ ⇒ I-TCM 1 拍、D-TCM 0 拍 ⇒ ⇒ 同一个 SoC 里两种存储器契约 ⇒ ⇒ 取指侧有 `pc_lag` 对齐机制、数据侧没有 ⇒ ⇒ **移植 cache 时必然踩坑**（这是 #4「接 AXI cache」被架构性阻塞的根因之一）。

### W.3 无反压 ⇒ 接不了任何真存储器

④⑤ ⇒ ⇒ MEM 级假定零延迟 ⇒ ⇒ 1 拍 cache 接不上、多拍 miss 接不上 ⇒ ⇒ **「RV32I L1 cache」这条既有 roadmap（`cpu-cache-subsystem` capability）在架构上无法落地**⇒ ⇒ 这是**架构阻塞**，不是接线活。

### W.4 字节写是最容易被时序改动悄悄破坏的路径

⑧ ⇒ ⇒ 「读-改-写」依赖**组合读**拿到"边沿前的旧字" ⇒ ⇒ 改寄存读 ⇒ ⇒ 旧字变成"上一拍的值" ⇒ ⇒ 若相位没对齐 ⇒ ⇒ `sb`/`sh` 会写到**错误的车道**（或整字覆盖）⇒ ⇒ **静默**（既有 golden 只查 2 个字节写用例 ⇒ ⇒ 判别力有限 ⇒ ⇒ 本项 MUST 补**相位错位探针**）。

---

## What Changes

### T1 — D-TCM 读改寄存读

```cpp
// 现状（0 拍组合）
ch_uint<DATA_WIDTH> old_word;
old_word <<= memory.aread(io().addr);

// 改后（1 拍寄存读）
auto read_data = memory.sread(io().addr, ch_bool(true));
```
- ⚠️ **产物形态变化**：组合 mux 读 ⇒ 寄存读 ⇒ ⇒ **BRAM 可推断** ⇒ ⇒ 产物行数与结构变化 ⇒ ⇒ 既有逐字节 golden（若比对 .v）MUST 更新。
- ⚠️ **`ready` 语义**：现在恒 `true`（单周期访问）⇒ ⇒ 改后 `ready` 仍恒 true（1 拍固定延迟 ⇒ 不是握手）⇒ ⇒ 真正的手势在 T3。

### T2 — MEM 级数据对齐（**MUST 由 architect 裁决**）

两个选项：

| 选项 | 描述 | 代价 |
|---|---|---|
| **A（建议）** | **地址提前一拍**：MEM 级在 N 拍发出地址 ⇒ N+1 拍采样数据 ⇒ ⇒ **与取指侧 `pc_lag` 同构** ⇒ ⇒ 复用既有机制 | MEM 级要延后一拍出地址 ⇒ ⇒ **流水线深度可视化地变**（有效链时序变）⇒ ⇒ 全部 golden 重算 |
| B | **数据晚一拍采样**：地址照旧，MEM/WB 装载点延后一拍 | MEM/WB 寄存器后移 ⇒ ⇒ **写回延后一拍** ⇒ ⇒ 同样全量重算，但**访存延迟对消费者不可见**（写回晚一拍 ⇒ 前端多等）⇒ ⇒ **load-use 停顿可能变长** |

⇒ ⇒ ⚠️ **两者都改全局时序** ⇒ ⇒ MUST 走 architect（ADR 级）⇒ ⇒ 本项只提出选项与代价。

### T3 — MEM 级 stall / 反压

- `data_ready`（当前死端口）⇒ 变成 MEM 级**真实输入**。
- MEM 级等 `data_ready` 时：冻结 MEM/WB 寄存器 + 向前级发 stall ⇒ ⇒ 有效链保持（不产生气泡误写）⇒ ⇒ **契约**：stall 期间 `data_write_en` MUST 保持（否则 store 被重复发出或丢失）。
- ⚠️ **实现位置**：stall 汇入 `HazardUnit`（已有 stall 移位链）⇒ ⇒ 需新增一路 stall 源（MEM 反压）⇒ ⇒ **不与既有 load-use 停顿混淆**（口径分离）。
- ⚠️ **判别力**：必须有一个**能观测到 MEM 反压**的用例 ⇒ ⇒ 需要一个会拉低 `data_ready` 的 DUT（测试专用）⇒ ⇒ 否则该通路**零覆盖**（⇒ ⇒ 与仓内「无判别力 ⇒ 不算覆盖」的纪律一致）。

### T4 — 字节写相位契约

- 在寄存读下，写使能 / 写地址 / 写数据的相位 MUST 显式定义：写使能 MUST 与**被覆盖的那一拍地址**对齐。
- 判据 MUST 覆盖：`sb` / `sh` 的**四个车道 × 两个半字车道**组合 ⇒ ⇒ 不只是现有的 2 个用例。

### T5 — 时序金标重算

- 受影响的期望值（**全部 MUST 手推**）：`perf_instructions`（当前 178）· `load_use_stalls`（当前 6）· `control_stalls`（当前 4）· store 拍数（当前 5）· `dbg_id_pc` / `dbg_id_instr` 逐拍序列 · 写回序列（`kExpectedWritebacks` 24 条）。
- ⚠️ **禁抄仿真输出**（仓内纪律：两后端同源 ⇒ parity 对该族缺陷结构性失明 ⇒ 期望值 MUST 手推）。
- ⚠️ `openspec/specs/rv32i-commit-path/spec.md` 的既有基线（188 → 179 已被订正过）MUST 再订正 ⇒ ⇒ **MODIFIED**（标题逐字保留）。

---

## Acceptance

### 产物形态

- [ ] D-TCM 读为**寄存读**：产物中 D-TCM 的读端口**不是** `assign data = mem[addr];` 形态 ⇒ ⇒ **可推断 BRAM**（判据：产物中无「4096:1 组合读」） ⇒ 归属: **B3**
- [ ] I-TCM 与 D-TCM 的读时序语义**一致**（都是 1 拍寄存读） ⇒ 归属: **B3**
- [ ] `verilator --lint-only` 干净；`scripts/verify_flat_rtl_emit.sh`（若已落地）全绿 ⇒ 归属: **B3**

### 行为

- [ ] 🔴 **T6 判别性判据（critic 第 2 轮：原只有「在内」没有 AC）**：**紧邻** `SW x1, 0(x2)` / `LW x3, 0(x2)`（**同址 · 零间隔**）⇒ 断言 `x3 == 写入值`（`read-new` 语义） ⇒ 归属: **B3**
- [ ] 🔴 `read-during-write` 语义 MUST 显式写进 capability（若选 `read-old` ⇒ 必须登记为**违反 ISA** 的已知限制 + 在 `arch-residual-registry.md` 留条目） ⇒ 归属: **B3**
- [ ] 🔴 现固件的「隔 4 字」形态 MUST **不能**冒充该判据（`SW`@word16 / `LW`@word20）⇒ 新用例 MUST 紧邻 ⇒ 归属: **B3**

- [ ] ⚠️ **全部时序金标手推重算并登记**（退休数 / 停顿拍数 / 控制停顿 / store 拍数 / 逐拍 `dbg_id_*` / 24 条写回序列）⇒ ⇒ **禁抄仿真输出** ⇒ 归属: **B3**
- [ ] 20 项 golden（取指填充不变量 + 3 个存储 golden + sb/sh lane + 3 个「被冲刷指令未写回」+ 4 个派生计数器）在新时序下**全绿** ⇒ 归属: **B3**
- [ ] ⚠️ `sb` / `sh` 的**相位正确**：`sb` 写 4 个车道 + `sh` 写 2 个半字车道**全覆盖**（现有仅 2 个用例 ⇒ 判别力不足 ⇒ 本项 MUST 补） ⇒ 归属: **B3**
- [ ] **相位错位探针**：故意把写使能错开一拍 ⇒ ⇒ **必红**（证明该判据有判别力） ⇒ 归属: **B3**

### 反压

- [ ] ⚠️ **2026-11-12 订正（metis 第 1 轮 · P1-8）**：原三条「**真实反压**」AC 是 **ADR-068 之前的旧形态** ⇒ ⇒ **ADR-068（Accepted · 2026-11-11）的 V7 胜出**（`data_ready`：**driver 仍恒 1** · 语义 MUST 注明「**固定 1 拍延迟（不是握手）**」）⇒ ⇒ **真反压移出本批**（归「接 cache」独立立项）⇒ ⇒ 原三条 AC **逐字保留在下方历史记录** ⇒ ⇒ 本批只保留**登记型**判据 ⇒ 归属: **B3**
- [ ] `data_ready` 的**语义登记**：契约（capability `rv32i-commit-path` + `include/cpu/AGENTS.md`）MUST 注明「**固定 1 拍延迟**（**不是握手**）」⇒ ⇒ ⚠️ **禁**在本批实现「驱动 0 ⇒ MEM 冻结」（那是**反压实现** ⇒ ⇒ 属独立立项）⇒ ⇒ 且 MUST 显式写明「本批**不**测反压」⇒ 归属: **B3**
- [ ] ⚠️ **历史记录（原三条 · 已被上一条取代 · 逐字保留）**：
  - 「`data_ready` 变成**真实反压**输入（不再是死端口）；MEM 级等 `data_ready` 时有效链保持、不产生误写 ⇒ 归属: **B3**」
  - 「stall 期间 `data_write_en` 的行为有明确契约并被断言（**不重复发 / 不丢**） ⇒ 归属: **B3**」
  - 「存在**能拉低** `data_ready`** 的 DUT ⇒ ⇒ 该通路**非零覆盖**（否则判"未覆盖"） ⇒ 归属: **B3**」

### 不回归

- [ ] 既有 ctest（`CTEST_TOTAL` == `ctest -N`）中**不受影响**的部分全绿；受影响部分 MUST 有**手推**的新期望值 + 变更说明 ⇒ 归属: **B3**
- [ ] `openspec/specs/rv32i-commit-path/spec.md` 的基线已 MODIFIED 订正（标题逐字保留） ⇒ 归属: **B3**
- [ ] 既有 `test_rv32i_soc_dual_parity` 的**双后端 parity** 仍成立（两后端同时改 ⇒ ⇒ parity 恒绿 ⇒ ⇒ **判别力来自手推 golden**，不来自 parity） ⇒ 归属: **B3**
- [ ] 计数锚点同步（`CTEST_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `VERILATOR_E2E_TOTAL` 如有变动） ⇒ 归属: **B3**

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-memory-timing`**（**新**） | ADDED | 存储器时序契约：I/D TCM 统一 1 拍寄存读 · MEM 级数据对齐（地址提前 vs 数据延后 · **二选一**）· MEM 反压（`data_ready` 语义 + stall 期间写使能契约）· 字节写相位契约 · 「可推断 BRAM」作为**可判定**产物属性 |
| `rv32i-commit-path` | MODIFIED | 时序金标基线订正（退休数 / 停顿拍数 / 逐拍序列）⇒ ⇒ 标题逐字保留 |
| `rv32i-isa-semantics` | MODIFIED | 追加「字节/半字写的**相位**契约」与「MEM 反压期间写使能保持」⇒ ⇒ 既有字节道语义条款**不删** |
| `mem-read-timing` | **不动** | 该 capability 讲的是 `ch_mem` 端口自身的时序 ⇒ ⇒ 本项是其**使用侧**的契约 ⇒ ⇒ MUST NOT 复述 |
| `cpu-cache-subsystem` | **不动**（引用） | 本项只满足接 cache 的**时序前置** ⇒ ⇒ cache 接入仍属后续立项 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `include/cpu/pipeline/rv32i_tcm.h` | `aread` → `sread` ⇒ ⇒ **字节写的读-改-写时序变**（T4）⇒ ⇒ 公共组件 ⇒ ⇒ 全部实例化点审计 |
| `include/cpu/pipeline/stages/mem_stage.h` | 数据采样点后移 或 地址前移（T2）⇒ ⇒ **有效链时序变** |
| `include/cpu/pipeline/rv32i_pipeline.h` | `data_ready` 从死端口变真反压；stall 源增加一路 ⇒ ⇒ **stall 口径**须分离（load-use vs MEM 反压）⇒ ⇒ 现有 `perf_load_use_stalls` 计数器**MUST NOT** 混入反压拍 |
| `include/cpu/pipeline/hazard_unit.h` | 若 stall 汇入此处 ⇒ ⇒ stall 移位链要区分两类来源 ⇒ ⇒ ⚠️ 该组件已有"3 拍固定延迟"的契约 ⇒ ⇒ 引入第二类 stall 会**污染**它 ⇒ ⇒ **建议**：MEM 反压**不进** `HazardUnit` 的 load-use 链，而是独立的 MEM 级冻结信号 ⇒ ⇒ **须 architect 裁决** |
| 全部时序金标 | **全量重算**（手推）⇒ ⇒ 大 diff ⇒ ⇒ MUST 分批 + 逐批 review |
| 产物 | 组合读 ⇒ 寄存读 ⇒ ⇒ 产物结构变化 ⇒ ⇒ 逐字节 golden 更新 |
| CI 墙钟 | 基本不变（仍是同数量级的测试） |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **手推 24 条写回序列 + 逐拍序列出错** ⇒ ⇒ 假红或假绿 | 逐条推导留痕（仓内既有纪律）⇒ ⇒ 关键期望值配**独立手算**（不同人/不同轮次）⇒ ⇒ 变异自证（挑 3 个变异 MUST 转红） |
| **字节写被时序改动悄悄破坏** | T4 的相位契约 + **全覆盖**（4 车道 × 2 半字）+ **错位探针** |
| **MEM 反压通路零覆盖**（没人造 `data_ready=0` 的 DUT） | Acceptance 明确要求存在该 DUT ⇒ ⇒ 否则判"未覆盖" ⇒ ⇒ 不许勾 |
| **stall 两类来源混淆** ⇒ ⇒ 计数器口径污染 | Acceptance 要求 `perf_load_use_stalls` **不含**反压拍 ⇒ ⇒ 显式断言 |
| **两后端同时改 ⇒ parity 恒绿 ⇒ 失去判别力** | 本项的判别力**全部**来自手推 golden ⇒ ⇒ Acceptance 明写「parity 恒绿不构成通过证据」 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `firmware-load-chain-2026-11` | **顺序依赖**：装载路径先做（写回可见性变了 ⇒ 装载断言要按新时序写）⇒ ⇒ 本项**在**其后 |
| `rtl-emit-product-tooling-2026-11` | 产物形态变化 ⇒ ⇒ manifest 的 `semantic` 可能要更新（`data_ready` 从"死端口"变"反压"）⇒ ⇒ 同步 |
| `rv32i-isa-gap-closure-2026-11` | ISA 项的 golden 依赖稳定时序基线 ⇒ ⇒ **本项先做**（否则 ISA 项的期望值要算两遍） |
| `rv32i-dead-component-consolidation-2026-11` | 无直接耦合；但若 Decoder/Alu 接入改了 ID 级时序 ⇒ ⇒ 建议 I5 在本项**之后**（否则时序金标算两遍） |

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **原稿事实④错**：`data_ready` **有 6+ 处实例化驱动**（不是「仅声明处」） | A.1④ 重写为「**流水线内部无消费者** + 被 `dtcm.ready ≡ true` 钉成常量 1」⇒ ⇒ 「死端口」与「常量 1 的驱动源」是两种不同修法 |
| 2 | 🔴 **决定性遗漏**：真正卡住 BRAM 的是**写口的 RMW 组合读**（`rv32i_tcm.h:112-117`）⇒ ⇒ **只改读口不足以达成「可推断 BRAM」** | A.1② 补记 ⇒ **新增 T2′**（写口替身，三选一：`ch_mem` 加字节使能写 / 保留专用 `aread` 写口 / 写口也 `sread` + 写延后一拍）⇒ ⇒ 架构裁决 MUST 同时覆盖读侧与写侧 |
| 3 | T2 选项 A **自相矛盾**（「地址提前」却写「延后一拍出地址」）；且「地址提前」有 A1（EX 组合出地址）/ A2（加一级寄存器）两个不同形态 | T2 改为**三选一 + 每项的预测计数器表**（A1 预测 `load_use_stalls` 仍 6 / `control_stalls` 仍 4 / `instructions` 仍 178 / `non_retired` 仍 21） |
| 4 | 🔴 **「逐拍 golden 序列」仓内不存在** ⇒ 「全量重算逐拍序列」**不可验收** | T5 改为**不变式优先**：24 条 `(rd,data)` MUST **逐条不变**（「写回内容变了」= bug，不归因接受）；仅计数类允许变且须写出手推值；**删除**逐拍序列要求 |
| 5 | T3 与 T1 **正冲**：固定延迟停顿链（`hazard_unit.h:256-257`）vs「MEM 冻结」（`hazard_unit.h:346-353` 明确要求 EX/MEM MUST NOT 冻结，否则死锁）；且 `dtcm.ready ≡ 1` ⇒ 反压在真实 SoC **恒不可达** | T3 改为**建议拆出本项**（或只定义 `data_ready` 语义、不实现反压）⇒ ⇒ 反压归「接 cache」立项 |
| 6 | `perf_load_use_stalls + hazard.io().stall` ⇒ **任何**新 stall 源自动污染该计数器 | Impact 补写具体改点（`rv32i_pipeline.h:597` 必须先改为只累计 load-use 脉冲） |
| 7 | 独立 TB 与 C++ 测试**同源**（期望值是转录的），且 `perf_instructions` 只 `$display` **不断言** ⇒ 「20/20 + 178」把没断言的算成通过 | I7 同步订正；本项**不得**把独立 TB 当验收证据 ⇒ ⇒ 判别力全部来自**手推 golden** |
| 8 | 「可推断 BRAM」**无可执行判据**；且草案引用的 `verify_flat_rtl_emit.sh` **不存在** ⇒ ⇒ 可逃逸项 | 新增护栏 `scripts/verify_dtcm_read_timing.sh`（结构判据 + `yosys memory_collect` 判据 + `--self-test` 投毒 `sread`→`aread` 必红）⇒ ⇒ 删除「若已落地」的可逃逸表述 |
| 9 | 组件级测试对 0/1 拍**结构性失明**（陷阱 T2）⇒ ⇒ 新写的相位探针若沿用「TB 设地址 → tick → 读」**必然恒绿** | T4 追加硬约束：相位类用例 MUST 用 **`ch_reg` 驱动地址**（SoC 级天然满足 · 组件级必须加 wrapper） |
| 10 | 既有会被打红的工件未点名 | Impact 补：`tests/test_rv32i_timing_dual.cpp` pair 3 · `test_pipeline_exec.cpp` 两个 SECTION · 6 个 `data_ready` 驱动点 · `rv32i_tcm.h` 注释 · `rv32i_pipeline_top.h:77`/`:120-131` 注释 · `include/cpu/AGENTS.md` TCM 段 · 根 `AGENTS.md` 两条 KNOWN LIMITATIONS |
| 11 | **文档同步是 PHASE GATE 第三道门**，原 Impact 表**一行文档都没有** | 已补入 Impact 表 |

### ⚠️ 待用户裁决

1. ✅ **已裁决（2026-11-11 · architect · ADR-068 Accepted）**：**T2′ = (F) 字节道拆分存储** —— D-TCM 的存储体拆成 **4 个 `ch_mem<ch_uint<8>, SIZE>` 字节道数组**，各道写使能 = `write & valid & strbe[i]`、写数据 = `wdata[8i+7:8i]`，读口用 `sread`（1 拍）⇒ ⇒ **消灭 RMW ⇒ 无组合读 ⇒ BRAM 可推断**。<br>⇒ ⇒ **权威** = `docs/adr/ADR-068-dtcm-memory-timing-contract.md`（**Accepted** · oracle ×2 + critic ×2 全闭 · 审稿记录 `.rddf/reviews/ADR-068-dtcm-timing.md`）。<br>⇒ ⇒ ⚠️ **ADR-068 驳回了原先的三选一**：① (A) 现状 = 不解决目标 ② (B) `ch_mem` 加字节使能写 = 改**框架级原语**（**51 处 / 9 个头**）⇒ 独立立项 ③ (C) 保留 `aread` 写口 = 不解决目标 ④ (D) 写口复用寄存读值 = **WAW 合并冒险**（背靠背同字 store ⇒ 静默丢字节）⑤ (E) 读写分离双体 = 面积翻倍 + 第二真源。<br>⇒ ⇒ 🔴 **ADR-068 带出的联动义务**：**D4「指令级 `read-new`」的成立条件 = 「load 地址呈现拍严格晚于 store 提交沿」** ⇒ ⇒ **与 T2 的选择强耦合** ⇒ ⇒ 若 T2 选 **A1（EX 级组合出地址）** ⇒ ⇒ MUST 追加「同址提交值旁路」或「一拍停顿」⇒ ⇒ **V5 是其判别判据** ⇒ ⇒ **T2 的裁决 MUST 与 ADR-068 的 D4 一并读**。
2. ✅ **已裁决（2026-11-10 · critic 第 1 轮 + oracle）**：**T3 拆出本项** ⇒ ⇒ 反压归「**接 cache**」立项 ⇒ ⇒ ⇒ 但 ⚠️ **T3 的 `data_ready` 语义重定义 MUST 随 T1 一起落**（见 §T3 行）⇒ ⇒ ⇒ 「拆出」指的是**反压实现**，**不是**语义重定义 ⇒ ⇒ ⇒ ⇒ 详见 §T3 行

## critic 第 1 轮（plan, build）—— **REJECT** · mergeable: no · scope_exceeded: false

> **轮次**：1/2 · 审查者：critic

### 🔴 BLOCKER（**plan 可修**）：T2′(iii) 改变写可见性时序，**无任何判据覆盖** ⇒ 目标场景静默错

| 事实 | 证据 |
|---|---|
| 现状 = 0-cycle `aread` + 同沿整字写回 ⇒ **store 当拍对下一拍 load 可见** | `include/cpu/pipeline/rv32i_tcm.h:113-117` |
| 改成「RMW 读走 `sread` + 下一拍写回」⇒ **紧邻同址 `sw`→`lw` 读到旧值**（写落在 T+1 沿；load 的 `sread` 也在 T+1 沿采样 ⇒ 非阻塞赋值取旧值） | 由 (iii) 的时序推得 |
| 而 **T3（反压）已被本项拆出** ⇒ **没有任何 stall 通路能兜住这条 hazard** | `memory-timing-contract-2026-11.md:60` |
| T4 只约定「字节写相位」与**跨地址**探针；T2 只约定 load 数据对齐 ⇒ **均不覆盖 store→load 同址顺序** | 本文档 T2/T4 |
| 现有 golden **不判别**：`SW(4,0,0x100)`@word16、`LW(5,0,0x100)`@word20（**间隔 4 字**）；`SB`@48、`LW`@52 | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:431,433,479,481` |

**处置**（planner 可修）：
MUST 新增 **T6 — store→load 同址顺序契约**：
- 显式定义 read-during-write 语义（**read-old** vs **read-new** 二选一并实现）；
- 判别性用例：**紧邻** `SW x1,0(x2)` / `LW x3,0(x2)`（同址、**零间隔**），断言 `x3 == 写入值`；
- ⚠️ 该用例在 (iii) 下**预期转红** ⇒ 必须先定语义再实现。
- 固件层面：现固件 `SW`…`LW` 间隔 4 字 ⇒ **无法发现**该缺陷 ⇒ MUST 补紧邻形态。

### ⚠️ 已采纳的 SHOULD_FIX

- ⑤ `data_ready` 语义重定义落在 **B0**，但「1 拍后数据有效 ⇒ `ready ≡ 1` 语义为真」**只在 T1（B3）之后成立** ⇒ B0 改语义 = 「**声称成立但不成立**」（第三种中间态，比「既不满足旧也不满足新」更糟）⇒ **处置：⑤ 的语义重定义移进 B3**。
- ④ 「BRAM 可推断」降级 + metis#8 要求新建的 `verify_dtcm_read_timing.sh` **含 `yosys memory_collect` 判据** ⇒ 而 `grep -rn yosys .github/ scripts/` **零命中** ⇒ 按 C5 自身 fail-closed 约定（缺工具 ⇒ exit 2）该护栏在 CI **恒红且不可通过**。处置：① **结构判据**（「数组上无组合读」）做 CI 门；② yosys 判据降为**本地/opt-in**；③ 「真实 BRAM 可推断性未在 CI 验证」MUST 登记进 `.rddf/checks/arch-residual-registry.md`（而非只在草案里写「降级」）。
- ①（截断等价）应补**最小判据**而非只靠论证 ⇒ 见 `firmware-load-chain` 的对应处置。
