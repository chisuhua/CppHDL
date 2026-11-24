# design · 提交事件端口脉冲化

> **Change**：`rv32i-commit-event-pulse-2026-11`
> **上游**：`rv32i-soc-platform-2026-11` 的 **S4a**（硬前置）
> **已签批依据**：`openspec/specs/rv32i-commit-path/spec.md`（`:52-99` / `:133-161`）

---

## ⚠️ 订正块（2026-11-24 · 实施期实测 · **本块优先于下文任何相冲突的表述**）

| # | 初稿（**已被实测推翻**） | 实测证据 | 订正 |
|---|---|---|---|
| **C1** | **§3 修法** = 两根端口都加 `& !mem_stall` | `io().data_write_en <<= exmem_is_store & !mem_stall;` ⇒ ⇒ **组合环** ⇒ ⇒ **`FATAL: Cycle detected in IR graph during topological sort`**（`src/core/context.cpp:376`）⇒ ⇒ **跑不起来** | **拆成两处**：写回事件端口（**纯观测面**）在流水线里门控；store 事件端口在**核顶层总线侧**派生（`data_write_en & data_ready_calc`）⇒ ⇒ **零功能变化** |
| **C2** | **§5 M2** = 去掉 `data_write_en` 的 `& !mem_stall` ⇒ ⇒ store 事件次数断言转红 | 今日 store 路径**不反压** ⇒ ⇒ 今日 store 事件**恒 == store 条数** ⇒ ⇒ **恒绿** ⇒ ⇒ **无效判据** | **M2 撤除**（恒绿判据必须撤掉）⇒ ⇒ 判别力归 **S4a**（见 proposal **AC7**） |
| **C3** | 判据形式 = 「停顿期间逐拍为 0」 | `mem_stall` **不是顶层端口** ⇒ ⇒ 核外**不可观测** ⇒ ⇒ 该形式**无法实现为顶层判据** | 改为**可观测的等价形式**：事件次数 == 手推条数 · 每 rd 恰好一次 · 相邻重复 == 0 |
| **C4** | 灌水的触发条件（初稿只说「停顿期间电平持续」） | **充要条件** = 「反压时 MEM/WB 里恰有一条 `rd≠0` 的写回类指令」⇒ ⇒ MEM/WB 装的是「EX/MEM 那条指令的**前 2 条**」⇒ ⇒ 冷 load 前 MUST 有 **≥ 2 条** `rd≠0` 的写回类指令 | 判据固件 MUST 按此构造 ⇒ ⇒ 「load 紧跟 load」**不够**（实测 `wb_high = 2` ⇒ 零灌水 ⇒ **恒绿**） |

---

## 1. 问题定位（精确信号链）

### 1.1 写回事件 `dbg_wb_write_en`

```
MEM/WB 寄存器（memwb_valid / memwb_rd_addr / …）
    │  rv32i_pipeline.h:637-652   memwb_*->next = select(mem_stall, memwb_*, m2_*)
    │                             ⇒ mem_stall 时 **冻结**（保持旧指令）
    ▼
wb_stage.io().mem_valid
    │  wb_stage.h:108-110  write_enable_sig = needs_writeback & rd_not_zero & io().mem_valid
    ▼
wb_stage.io().write_en
    │  rv32i_pipeline.h:891   io().dbg_wb_write_en <<= wb_stage.io().write_en;
    ▼
核端口 `top_core_dbg_wb_write_en`（**电平**）
```

⇒ ⇒ **MEM/WB 冻结期间**，WB 级仍在处理**同一条**指令 ⇒ ⇒ `write_en` **持续为高**。

### 1.2 store 事件 `obs_data_write_en`

```
exmem_is_store（ch_reg）
    │  rv32i_pipeline.h:568   exmem_is_store <<= select(mem_stall, exmem_is_store, …)
    │                          ⇒ mem_stall 时 **冻结**
    ▼
io().data_write_en <<= exmem_is_store;      （rv32i_pipeline.h:693）
    │  ⚠️ **功能路径**：核顶层 → D-L1 的 `we` → `aw_valid` → `aw_fire` → `ready`
    ▼
核端口 `top_core_obs_data_write_en`（**电平**）
```

### 1.3 「停顿」从哪来

```
data_ready ← !data_req | cache.ready            （核接线 · S3）
mem_stall  = !io().data_ready                    （rv32i_pipeline.h:276 / :710 / :832）
```

⇒ ⇒ 只要**数据通路**有多拍等待（**D-L1 fill** / **未来的 MMIO 事务**），
⇒ ⇒ `mem_stall=1` ⇒ ⇒ 上面两根电平被灌水。

### 1.4 为什么今天没暴露

核级 / SoC 级现有程序**只有 `sw`、没有 `lw`**：

| word | 指令 | 数据通路 | 是否多拍 |
|---|---|---|---|
| 0 | `addi x1, x0, 42` | 无访存 | — |
| 2 | `addi x2, x0, 11` | 无访存 | — |
| 4 | `add x3, x1, x2` | 无访存 | — |
| 5 | `sw x3, 0(x0)` | D-L1 **write-through 单拍**（`d_cache_sync.h` 写路径 = `ready = aw_fire`）⇒ ⇒ **无 fill** | **否** |
| 6 | `jal x0, 0` | 无访存 | — |

⇒ ⇒ **无 `mem_stall`** ⇒ ⇒ 两根电平**恰好**等于事件次数 ⇒ ⇒ 实测 `store_count == 1` / `writeback_count == 3` 绿。
⇒ ⇒ **这是巧合，不是设计** ⇒ ⇒ 任何引入 `lw`（含 S4a 的 MMIO 访问）的程序都会击穿它。

### 1.5 与正确口径的偏差（对照 `commit_count`）

```
commit_valid = mem_stage.io().mem_valid_out & !mem_stall;      （rv32i_pipeline.h:833）
commit_count->next = select(reset_active, 0, commit_count + commit_valid);   （:834）
io().perf_instructions <<= commit_count;                        （:871）
```

⇒ ⇒ **退休计数已经正确排除了停顿** ⇒ ⇒ 两根事件端口与它**口径不一致**
⇒ ⇒ 这是**一致性缺陷**（不是新需求）⇒ ⇒ 修法 = 对齐到 `commit_count` 的口径。

### 1.6 灌水的**充要条件**（2026-11-24 实测确立 · 判据构造的**唯一依据**）

```
EX/MEM ──▶ M2 ──▶ MEM/WB ──▶ WB        （级序 · 「落后 2 级」）
  ▲                  ▲
  │                  └── 装的是「EX/MEM 那条指令的**前 2 条**」
  └── 冷 load 在这里 ⇒ ⇒ 反压 ⇒ ⇒ MEM/WB 冻结 ⇒ ⇒ 那条指令**停留多拍**
```

⇒ ⇒ **充要条件** = 「反压发生时，MEM/WB 里恰有一条 `rd≠0` 的**写回类**指令」
（ALU / load / jump ⇒ `wb_stage.h:97` 的 `needs_writeback`）
⇒ ⇒ ⇒ 判据固件 MUST 在冷 load 前放 **≥ 2 条** `rd≠0` 的写回类指令
⇒ ⇒ ⇒ 实测反证（**旧实现**）：
- 固件 `lw x2,0x100` · `lw x3,0x120` · `jal x0,0` ⇒ ⇒ `wb_high = 2` ⇒ **零灌水** ⇒ **恒绿**
- 固件 `addi x1,42` · `lw x2,0` · `addi x3,1` · `jal x0,0` ⇒ ⇒ `wb_high = 3` ⇒ **零灌水** ⇒ **恒绿**
- 固件 `addi x2,7` · `addi x3,9` · `addi x4,11` · `lw x5,0x100` · `jal x0,0`
  ⇒ ⇒ `wb_high = 12` · `x3` 写回 **9** 次 · 相邻重复 **8** ⇒ ⇒ **必红** ✓

## 2. 判据：「事件端口」vs「级属性端口」

| 类别 | 语义 | 正确形态 | 本仓实例 |
|---|---|---|---|
| **事件端口** | 「本拍**发生**了 X 次」 | **单拍脉冲**（与事件时长无关） | `obs_data_write_en` · `dbg_wb_write_en` · `dbg_load_use_detected`（已是脉冲） |
| **级属性端口** | 「**当前寄存器**里那条指令是 X」 | **电平**（跟随寄存器内容） | `dbg_exmem_is_store` · `dbg_id_is_store` · `dbg_exmem_valid` · `dbg_mem_valid_out` |

⇒ ⇒ **本 change 只改「事件端口」** ⇒ ⇒ `dbg_exmem_is_store` 等**一律不动**
⇒ ⇒ （改它们会把「级属性」错改成「事件」⇒ 反而制造第二套语义 ⇒ 禁止）

⇒ ⇒ `dbg_load_use_detected` 已是单拍脉冲（`rv32i_pipeline.h:892` ← `hazard.io().load_use_detected`）
⇒ ⇒ ⇒ 本 change 让两根事件端口**与它对齐** ⇒ ⇒ 三根事件端口口径统一。

## 3. 修法（**2026-11-24 订正**）

### 3.1 写回事件端口（**库层 · 纯观测面**）

```cpp
// include/cpu/pipeline/rv32i_pipeline.h（build_debug_outputs）
// ⚠️ 事件端口 MUST 是「单拍脉冲」（与事件时长无关）：
//    MEM/WB 在 mem_stall 时冻结 ⇒ ⇒ 同一条指令在 WB 停留多拍 ⇒ ⇒ 电平持续为高
//    ⇒ ⇒ 与 commit_count 同口径（commit_valid = … & !mem_stall）
auto mem_stall = !io().data_ready;
io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;
```

⇒ ⇒ **恰好一次**：MEM/WB 的内容只在**非停顿拍**才更新 ⇒ ⇒ 每条指令的 MEM/WB 驻留的
**首个**拍必是非停顿拍 ⇒ ⇒ 脉冲**不丢也不重**（实测：4 次写回 ⇒ 4 个脉冲）。
⇒ ⇒ 本端口**零内部消费者** ⇒ ⇒ 门控 `mem_stall` **不成组合环** ✓。

### 3.2 store 事件端口（**核顶层 · 总线侧派生**）

⚠️ **不可**在流水线里门控 `io().data_write_en` ⇒ ⇒ **组合环** ⇒ ⇒ 实测 `FATAL`：

```
data_write_en → D-L1.we → aw_valid → aw_fire → dcache.ready
              → data_ready → mem_stall → data_write_en     （环）
```

⇒ ⇒ 修法 = 在**核顶层**（总线装配处）派生**观测**语义，**功能信号零改动**：

```cpp
// examples/riscv-mini/src/rv32i_core_top.h
auto data_ready_calc = (!pipeline.io().data_req) | dcache.io().ready;
pipeline.io().data_ready <<= data_ready_calc;
// store 提交事件 = 写使能 **且** 总线就绪（对 store 而言 = aw_fire = 写地址握手拍）
io().obs_data_write_en <<= pipeline.io().data_write_en & data_ready_calc;
```

⇒ ⇒ **功能侧仍用 `pipeline.io().data_write_en`（电平）** ⇒ ⇒ D-L1 的写握手次数**恒为 1**
⇒ ⇒ **零功能影响** ⇒ ⇒ 端口名 / 方向 / 位宽全部不变 ⇒ ⇒ **核端口面仍是 34** ⇒ ⇒ C6 快照**不动**
⇒ ⇒ 今日**零可观测变化**（既有固件 store 事件 = 5 = store 条数）⇒ ⇒ 判别力归 S4a（proposal AC7）

### 3.3 与 `obs_data_addr` / `obs_data_write_data` 的同拍关系

`obs_data_addr` ← `exmem_alu_result`（`:665`）· `obs_data_write_data` ← `shl(exmem_rs2_data, …)`（`:687`）
⇒ ⇒ 三者**同拍同源** ⇒ ⇒ 脉冲化后「`write_en=1` 的那拍」仍能取到对应的 `(addr, data)`
⇒ ⇒ 核级 S5 判据（「∃ 拍 `obs_data_write_en=1` ⇒ addr==0 ∧ data==53」）**不受影响**。

### 3.4 「折连续相同」workaround 的处置

`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:965-974` 逐拍收集写回序列并**折掉相邻相同**。
⇒ ⇒ 脉冲化后**同一条指令只产生 1 个条目** ⇒ ⇒ 该折算**变成幂等**（不再折掉任何东西）
⇒ ⇒ **golden 序列逐项不变** ⇒ ⇒ **期望值零变化** ✓
⇒ ⇒ ⚠️ 但它**继续掩盖**「相邻两条**不同**指令写回**相同** `(rd,data)`」这一形态
⇒ ⇒ ⇒ **追加**一条**不折**的断言（proposal AC3）正面覆盖。

## 4. 残留（如实登记 · 本 change **不做**）

| # | 残留 | 为何不做 | 风险 |
|---|---|---|---|
| R1 | **`reg_file.write` 在停顿期间仍每拍写同一 `(rd, data)`** | **幂等** ⇒ 寄存器堆内容**无可观测差异**；改它要动 `reg_file.write` 的**使能**口径 ⇒ ⇒ 触及「退休」语义 ⇒ 风险面远大于收益 | **零可观测影响**（值恒等）⇒ ⇒ 但若将来 `reg_file` 引入「非幂等」行为（如写计数器）⇒ MUST 重评 |
| R2 | **`dbg_exmem_is_store` 等级属性端口仍是电平** | **正确语义**（见 §2）⇒ ⇒ 不是缺陷 | 无 |
| R3 | **「相邻两条不同指令写回相同 `(rd,data)`」在既有 golden 里可能被折掉** | 既有固件**恰好**无此形态 ⇒ ⇒ 本 change 的 AC3 用**新程序**覆盖，不动既有 golden | 低（AC3 已补判别力面） |
| R4 | **store 事件端口的脉冲化今日**不可判别** | 今日 store 路径**不反压** ⇒ ⇒ 断言**恒绿** ⇒ ⇒ **恒绿判据必须撤掉** | 低 ⇒ ⇒ 判别力归 **S4a**（MMIO 事务必然多拍） |

## 5. 判别力自证（变异 · **2026-11-24 实测**）

| # | 变异 | 实测结果 | 判据 |
|---|---|---|---|
| **M1** | `io().dbg_wb_write_en <<= wb_stage.io().write_en;`（去掉 `& !mem_stall`） | ✅ **转红**：`wb_events = 12`（≠ 4）· `x3` 写回 **9** 次 · 相邻重复 **8** | AC1 / AC2 / AC3 |
| **M2** | ~~去掉 `data_write_en` 的 `& !mem_stall`~~ | ❌ **撤除**：该写法**本身成环**（`FATAL`）⇒ ⇒ 且今日 store 事件**恒绿** ⇒ ⇒ **恒绿判据不留** | —（归 S4a，见 R4） |
| **M3** | 两根事件端口**对调**（核顶层：`obs_data_write_en` ↔ `dbg_wb_write_en`） | ✅ **转红**：`wb_events = 0`（≠ 4） | AC1 / AC2 |

⇒ ⇒ 每个变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）
⇒ ⇒ **还原源码 ≠ 复原** ⇒ ⇒ MUST **重建**后再复跑。

## 6. 实施顺序（TDD 5 步 · 见 tasks.md）

1. **先写判据**（追加到既有 `test_rv32i_soc_dual_parity.cpp`）⇒ ⇒ 旧实现下 AC1/AC2/AC3 **必红**（先见红）
2. **改 2 处**（流水线 1 处 + 核顶层 1 处）
3. **全量重建 + 复跑** ⇒ ⇒ AC 全绿 · **核级 24 条仍绿** · `ctest` 全绿
4. **判别力自证**（M1 / M3 实测转红 ⇒ 还原复绿）
5. **登记面**（`public-api-surface.md` / `CHANGELOG.md` / 两份 AGENTS.md）
