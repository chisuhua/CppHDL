# Design: RV32I 取指 skid buffer（缺陷 A）+ 填充期 `primed` 门控（缺陷 B）

- **change**：`rv32i-fetch-skid-buffer-2026-10`（**有** spec 变更 ⇒ 归档 **禁** `--skip-specs`）
- **架构依据**：`docs/adr/ADR-048-rv32i-fetch-skid-buffer.md`（Accepted；D1–D13.1）
- **5 段草案**：`.rddf/improvements/rv32i-fetch-skid-buffer-2026-10.md`
- **本文件定位**：**实施设计** —— 逐拍时序 / 精确表达式 / 金标推导方法 / 变异计划
- **一个 change、两个 commit**（ADR-048 D13.1）：C1 = 缺陷 A（skid）、C2 = 缺陷 B（`primed`）

## 1. 实测时序（**读码 + 逐拍 trace 双向确认**，2026-10-27）

⚠️ 本节的三条结论**与既有文档的措辞不同**，实施 MUST 按本节，不得按注释里的旧说法。

以既有固件（`firmware()`，108 字）的停顿 #1 为例
（`RV32I_SOC_DEBUG=200 ./build/examples/riscv-mini/test_rv32i_soc_dual_parity` 实测）：

```
cyc  instr_addr  dbg_id_pc  ID 字   lu_stall(累计)   说明
 21  0x80000058  0x80000050   20       0            正常
 22  0x8000005c  0x80000054   21       0   ← stall **在本拍**已拉高（23 拍累计值变 1）
 23  0x8000005c  0x80000054   21       1
 24  0x8000005c  0x80000054   21       2
 25  0x8000005c  0x80000054   21       3   ← stall 已释放（26 拍累计值仍是 3）
 26  0x80000060  0x8000005c   23       3   ← **word 22 被丢弃**（它从未出现在 ID）
 27  0x80000064  0x8000005c   23       3   ← **word 23 出现两次**（重复装载）
 28  0x80000068  0x80000060   24       3
```

### 1.1 结论 1：ID 级看到的是 `pc_reg` **晚 2 拍**的值

`dbg_id_pc(n) == pc_reg(n-2)`（等价地 `== instr_addr(n-2)`）。
链路：`pc_reg` → 组合节点 → `instr_addr` → `InstrTCM` 的 `sread`（**1 拍**）
→ `instr_data` → `fetched_instr`；`fetch_pc_reg->next = pc_reg` 再补 1 拍，
由 IF/ID 同时锁存（`if_stage.h:203-216`）。

### 1.2 结论 2：停顿首拍到达的正是**被丢弃的那一条**

停顿首拍（上表 cyc 22）：
- `fetch_pc_reg` = `pc_reg`(前一拍) = **word 22 的地址**；
- `fetched_instr` = `mem[instr_addr(前一拍)]` = **word 22**；
- ⇒ 二者**同拍成对**，且 `if_id_update_en = 0`（`stall`）⇒ **无处可存** ⇒ word 22 永久消失
  （其地址 `pc_reg` 已越过它，**不会**再被提交）。

⇒ **skid 的填充就是抓这一对**：`skid_instr ← fetched_instr`、`skid_pc ← fetch_pc_reg`
（ADR-048 D2.1 —— 取 `pc_reg` 会错 4 字节）。

### 1.3 结论 3（**ADR-048 未记载的新事实**）：停顿之后还有一次**重复装载**

停顿释放那一拍（cyc 25）与下一拍（cyc 26）加载的都是 **word 23**：
`pc_reg` 在停顿期间冻结在 word 23 的地址上 ⇒ 恢复后 `fetch_pc_reg` 与
`fetched_instr` **连续两拍给出同一对 (word23, word23)**。
⇒ 当前实现下每次停顿**既丢一条、又重一条**（净退休数不变 ⇒ 既有金标看不出）。

⚠️ **对实施的影响**：skid 的排空拍恰好**顶替**这次重复装载（第 2 节的时序表）
⇒ 下游节拍**不后移** ⇒ 既有金标（退休数 / 写回序列）**大概率零变化**
（由实施后的实测确认，见 §5.2）。这与 ADR-048 D9.2 的「`+k`」预期不同
（ADR 未考虑本节的重复装载）⇒ **以实测为准，差异记入 §5.2**。

## 2. C1 逐拍时序表（缺陷 A）

设：停顿窗口 = `T`（首拍）..`T+d-1`，`T+d` 为释放拍；ID 在 `T`..`T+d-1` 持有消费者 `A_c`；
`pc_reg` 在 `T`..`T+d-1` 冻结在 `A_c+8`。

| 拍 | `stall` | `skid_valid` | `fetched_instr` / `fetch_pc_reg` | IF/ID 装什么 | 说明 |
|---|---|---|---|---|---|
| `T-1` | 0 | 0 | `(A_c+4, A_c+4)` | `(A_c+4, A_c+4)` | 正常 |
| `T` | **1** | 0 | `(A_c+4, A_c+4)` | 冻结 | ⚠️ **填充**：存 `A_c+4`（= 会被丢弃的那条） |
| `T+1..T+d-1` | 1 | 1 | `(A_c+8, A_c+8)` | 冻结 | 不再填充（`!skid_valid`）；数据可重读 ⇒ 丢弃安全 |
| `T+d` | **0** | 1 | `(A_c+8, A_c+8)` | **`(A_c+4, A_c+4)` = skid** | ⚠️ **排空**：流上数据被丢弃；`pc_reg` 仍指向 `A_c+8` ⇒ 下一拍重读 |
| `T+d+1` | 0 | 0 | `(A_c+8, A_c+8)` | `(A_c+8, A_c+4?)`→见注 | 正常流 |
| `T+d+2` | 0 | 0 | `(A_c+12, …)` | `(A_c+12, …)` | 正常 |

注：`T+d+1` 拍的 IF/ID 装载取自**流上**：`fetched_instr` 与 `fetch_pc_reg`
**同拍成对**（`fetch_pc_reg` 无条件跟 `pc_reg`，而 `instr_data` 跟 `instr_addr`）
⇒ 装载对恒为 `(A_c+8, A_c+8)`，与 `skid_pc` 无关。

⇒ 序列：`A_c`（保持）→ **`A_c+4`**（救回）→ `A_c+8` → `A_c+12` ⇒ **无丢失、无重复**。

## 3. C1 的精确表达式（`include/cpu/pipeline/stages/if_stage.h`）

新增 3 个寄存器（放在 `fetch_pc_reg` 之后、`if_id_update_en` 之前）：

```cpp
ch_reg<ch_uint<32>> skid_instr(0_d, "skid_instr");
ch_reg<ch_uint<32>> skid_pc(0_d, "skid_pc");
ch_reg<ch_bool>     skid_valid(false, "skid_valid");
```

条件（**用 `select` 表达，绝不用 C++ `if`** —— `ch_bool` 有 `explicit operator bool()`
⇒ C++ `if` 拿到的是「节点是否非零」，不是运行期条件，见 `AGENTS.md §KNOWN LIMITATIONS`
的 cache wrapper 事故）：

```cpp
// 排空 MUST 与 IF/ID 的实际装载同拍原子 ⇒ 直接由 if_id_update_en 派生
auto skid_drain_en = if_id_update_en & skid_valid;       // 含 !rst / !stall / !flush / !flush_d1
// 填充：停顿拍把「已到达但装不进 IF/ID」的那一对接下来
auto skid_fill_en  = select(io().rst, ch_bool(false),
    select(skid_drain_en, ch_bool(false),
    select(io().stall & io().instr_ready & !skid_valid &
           !(io().flush | flush_d1), ch_bool(true), ch_bool(false))));
```

寄存器更新（**排空优先于填充且同拍互斥** —— 排空要求 `!stall`、填充要求 `stall`）：

```cpp
skid_instr->next = select(skid_fill_en, fetched_instr, skid_instr);
skid_pc->next    = select(skid_fill_en, fetch_pc_reg, skid_pc);   // ⚠️ 不是 pc_reg（D2.1）
skid_valid->next = select(skid_fill_en, ch_bool(true),
                   select(skid_drain_en | io().flush | flush_d1 | io().rst,
                          ch_bool(false), skid_valid));
```

IF/ID 取数源（`skid_valid ? skid : 流上`）：

```cpp
if_id_pc_reg->next    = select(if_id_update_en,
                               select(skid_valid, skid_pc, fetch_pc_reg), if_id_pc_reg);
if_id_instr_reg->next = select(if_id_update_en,
                               select(skid_valid, skid_instr, fetched_instr), if_id_instr_reg);
```

⚠️ `if_id_valid_reg->next` 的 `instr_ready` 分支**不动**（`instr_ready ≡ 1`）。

### 3.1 顺带 ZERO-DEBT（提案 §What Changes #4）

`if_stage.h:105-112` 与 `:113-119` 是**逐字重复**的两段「flush 与 stall 不会同拍」
注释（复制粘贴事故）⇒ **删掉后一段**（保留前一段）。

## 4. C2 的精确表达式（缺陷 B）

```cpp
ch_reg<ch_bool> primed(false, "primed");          // 复位后首个**非复位**周期结束即置 1
primed->next = select(io().rst, ch_bool(false), ch_bool(true));
```

默认方案（ADR-048 D6 方案 1）：**只门控 `valid`**

```cpp
if_id_valid_reg->next = select(if_id_update_en & primed, io().instr_ready,
                               select(io().flush | flush_d1, ch_bool(false), if_id_valid_reg));
```

- 幻影装载的 `pc` / `instruction` **仍然装入**（其 `pc` = `0x80000000`、指令 = word 0，
  与下一拍真取指**逐字相同**）⇒ `dbg_id_pc` 观测序列**零变化**；
- 只有 `dbg_id_valid` 在**第 0 拍**由 1 变 0 ⇒ 幻影不进 ID/EX
  （`idex_update_en = io().valid`，`id_stage.h:248`）⇒ **退休数 −1**；
- ⚠️ 判据 MUST **内生于 `IfStage`**：`instr_ready` 恒 1（`rv32i_tcm.h:58`）⇒ 不能由它导出。

## 5. 测试与金标

### 5.1 判别性固件（**新增**，与既有固件**独立**）

宿主 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（**既有 target ⇒ 计数不变**）。
新增 `skid_firmware()`（**24 字**）+ 独立 `TEST_CASE`
`Rv32iSoc - FetchSkidBufferDiscriminating`（**既有的 `firmware()` 与它的 golden 不动**
⇒ 既有 golden 的变化 MUST 全部可归因；本 change 的归因 = 「C2 的 −1」+「fill 段 +1」）。

固件布局（word N 的地址 = 0x80000000 + 4N；立即数一律**字节**为单位）：

| word | 指令 | 判别对象 | 反向期望（未修时） |
|---|---|---|---|
| 0 | `ADDI x16, x16, 1` | **缺陷 B**（自依赖首条 ⇒ 双装非幂等） | 双装 + 前推 ⇒ `x16 = **2**` |
| 1–3 | NOP | — | — |
| 4 | `ADDI x21, x0, 0x33` | 提供非零的 load 源 | — |
| 5–7 | NOP | — | — |
| 8 | `SW x21, 0x140` | `[0x140] = 0x33`（距离 4 ⇒ 寄存器堆路径） | — |
| 9 | NOP | — | — |
| 10 | `LW x17, 0x140` | `x17 = 0x33` | — |
| 11 | `ADD x18, x17, x0` | 消费者（距离 1 ⇒ 触发 load-use 停顿） | — |
| 12 | `ADDI x19, x0, 0x5A` | **缺陷 A**（停顿点之后**非 NOP**） | 该指令**消失** ⇒ `x19` 从不写回 |
| 13 | `ADDI x20, x0, 0xA5` | **重复装载**（design §1.3 的形态） | `x20` 被写**两次** |
| 14–15 | NOP | — | — |
| 16 | `SW x19, 0x144` | 救回的指令**真的执行到 MEM** | store 拍**不出现** |
| 17–19 | NOP | — | — |
| 20 | `SW x20, 0x148` | 重复装载会写**两次** | — |
| 21–23 | NOP | — | — |

断言（**全部手工推导**）：

1. **写回序列**（**强断言**：既抓「丢」也抓「重」）—— 期望 **6** 项：
   `(16,1) (21,0x33) (17,0x33) (18,0x33) (19,0x5A) (20,0xA5)`
   （固件是**直路径** ⇒ 程序顺序 = 写回顺序；⚠️ **不去重** —— 任何连续重复都是真回归）；
2. **缺陷 B**：`x16` 的写回**恰好 1 次**且值 = 1，**且 MUST NOT 出现 2**
   —— 反向期望 `2` 的推导：双装时第二条 word 0 与第一条**紧邻**（指令距离 1）⇒ 由
   **档位 1**（EX/MEM 合并写回）前推 ⇒ 读到第一条写入的 1 ⇒ `1 + 1 = 2`；
3. **缺陷 A**：`has_store(0x144, 0x5A)` 且 `MUST NOT` 出现 `0x144 = 0`
   —— 反向期望：该指令**不出现**（store 拍**不存在**）；
4. **重复装载**：`x20` 的写回**次数恰好 1**
   —— ⚠️ 值与「重了一次」相同 ⇒ 集合 / 去重 / 序列断言都**抓不到** ⇒ MUST 用**计数**断言；
5. **取指流的顺序不变式**：ID 看到的**去重后** word 序列 MUST 逐 4 递增
   （本固件**无分支** ⇒ 程序顺序 = 取指顺序）⇒ **24 个 word 一个不少**且
   `words[i] == i` ⇒ 跳号 = 丢指令、倒退 = 重取指令；
   ⚠️ 停顿期间 ID **保持**同一个 word ⇒ 必须先去重再判（否则正常停顿会误报）。

编译期护栏（**方向反转**，取代 `stall_drop_slots_are_nop`）：

```cpp
constexpr bool skid_slot_is_discriminating();   // 停顿点之后 MUST 是**非 NOP**
static_assert(skid_slot_is_discriminating(), "...");   // 且写非 x0；且至少命中一处
```

### 5.2 既有金标的处置（ADR-048 D11）

| 对象 | 位置 | 处置 |
|---|---|---|
| `stall_drop_slots_are_nop` | `:573-592` | **删除**（前提已不成立；判别方向由 §5.1 的 `skid_slot_is_discriminating` 取代） |
| `fwd_stall_drop_slots_are_nop` | `:1134-1147` | **删除**（同上；该固件的 load 消费者距离 ≥ 2 ⇒ 检查本就是空转） |
| `fwd_first_instruction_not_self_dependent` | `:1154-1166` | **删除**（C2 修掉缺陷 B ⇒ 前提消失；判别改由 §5.1 的 word 0） |
| 连续重复去重掩码 | `:941-945` | **删除**（它掩盖**真实**双写回回归；C2 后无合法重复 ⇒ 必须让重复直接暴露） |
| 退休数金标 | **改前**位置 `:905,917`（179 / 20）⇒ **改后**位置 `:907-908`（178 / 21） | **已按实测回填**（见 §5.4：C1 单独**不变**、C2 只把 fill 段 2 → 3） |
| 根 `AGENTS.md §KNOWN LIMITATIONS` | 两条残留 | 消解 + 订正（**PC 回退已否决**；根因改为**加载拍预填**） |

### 5.3 组件级负向对照（ADR-048 D9.4，**必需**）

宿主 `tests/test_rv32i_pipeline_behavior.cpp`（既有 dual-backend target ⇒ **计数不变**）。
`ch_device<IfStage>` + **同拍驱动 `stall` 与 `flush`**：

- 断言 A：`stall & flush` 同拍 ⇒ skid **不被填充**（`flush` 拍之后先撤 `flush`，
  再撤 `stall`，第一个进 ID 的指令 MUST **不是** 填充候选）；
- 断言 B：`skid_valid` 已为 1 时来 `flush` ⇒ skid **被清空**（后续排空不再发生）。

⚠️ 这两条在 SoC 级**不可达**（ADR-048 D5）⇒ **组件级是唯一判别力来源**
⇒ 变异「删掉 flush 清空 skid」MUST 由本用例抓到。
⚠️ 该组件有 **7 个**输入端口（`stall` / `flush` / `rst` / `branch_target` /
`branch_valid` / `instr_data` / `instr_ready`）MUST **逐个显式驱动**
（ADR-048 的教训：未驱动端口读到未定义值 ⇒ 两后端可能分叉）。
⚠️ `IfStage` 的 `rst` 是**高有效**（`pc_update_en` / `if_id_update_en` 都是
`select(rst, 0, …)`）⇒ 正常驱动 **0**（不是 1！）。

### 5.4 实测结果（2026-10-27）

#### C1 单独（skid，只改 `if_stage.h`）

| 观察项 | 改前 | 改后 | 归因 |
|---|---|---|---|
| ID 序列（停顿 #1） | `…21, 21, 21, 21, **23**, 23, 24…` | `…21, 21, 21, 21, **22**, 23, 24…` | word 22 被救回 + 重复的 word 23 消失 |
| ID 序列（停顿 #2） | `…44, 44, 44, 44, **46**, 46, 47…` | `…44, 44, 44, 44, **45**, 46, 47…` | 同上（word 45） |
| `non_retired` | 20 | **20** | 救回 1 条（+1 增长）+ 消掉 1 条重复（−1 增长）**相抵** |
| 退休数 | 179 | **179** | 同上 ⇒ **既有金标零变化**（§1.3 的预测成立） |
| 写回序列 | 24 项 | **24 项（逐项相同）** | 被救回的是 NOP ⇒ 无写回 |

⇒ **C1 是纯正确性修复、零观测面扰动**（既有金标全部保持）。

#### C2（`primed`）

| 观察项 | 改前 | 改后 | 归因 |
|---|---|---|---|
| `non_retired` | 20 | **21** | fill 段 2 拍 → **3 拍**（cyc 1..3） |
| 退休数 | 179 | **178** | 幻影（重复执行的首条指令）消失 = **−1** |
| 不增长拍的**分段** | fill 2 + 停顿 6 + 冲刷 12 | fill **3** + 停顿 **6** + 冲刷 **12** | 停顿 / 冲刷段**逐段不变** ⇒ ADR-048 D9.2 的**分段不变式**成立 |
| `dbg_id_valid`(cyc 0) | 1 | **0** | 幻影的 `valid` 被压掉 |
| `dbg_id_pc` 全序列 | — | **逐拍完全不变** | 「只门控 `valid`」⇒ D6 方案 1 的承诺成立 |
| `obs_data_addr`(cyc 2) | — | 一处变化 | 幻影不再进 EX/MEM（下游少一次假访存地址） |

#### 用例与断言计数

- SoC 级：**5 个 TEST_CASE / 10859 断言**全绿（其中新增判别性用例
  `Rv32iSoc - FetchSkidBufferDiscriminating` 含 24 个 word 的顺序不变式）；
- 组件级：`tests/test_rv32i_pipeline_behavior.cpp` **38 用例 / 74 断言**全绿
  （Batch 2/3 的 16 pair + 新增 Batch 4 的 **3** pair）。

## 6. 变异验证（每条**先确认编译成功**，否则跑的是旧二进制 ⇒ 假绿）

**实测 2026-10-27**：6/6 **全部被抓到**，且每条变异都先确认 `error:` 计数 = 0
（脚本 `/tmp/opencode/mutate2.sh`：注入 → 编译 → 跑两个 target → 还原 → **重建**）。

| # | 变异 | 编译 | SoC 级（`test_rv32i_soc_dual_parity`） | 组件级（`test_rv32i_pipeline_behavior`） |
|---|---|---|---|---|
| M1 | 填充时锁 `pc_reg` 而非 `fetch_pc_reg` | ✅ 0 err | **1 failed**（word 序列不变式：被救回的指令 pc 错 4 字节） | **2 failed**（Batch 4-c 的 `pc == 0x80000000`） |
| M2 | 关掉排空（`skid_drain_en` 恒 0） | ✅ 0 err | **2 failed** | 全绿 ⚠️ 见 §6.1 |
| M3 | 排空条件写反（`stall` 时排空） | ✅ 0 err | **2 failed** | 全绿 ⚠️ 见 §6.1 |
| M4 | 删掉「flush / `flush_d1` / rst 清空 skid」 | ✅ 0 err | **全绿**（流水线级**不可达** ⇒ 与 ADR-048 D5 的预测一致） | **2 failed**（Batch 4-b）⇒ **组件级是唯一判别力来源** |
| M5 | 去掉 `primed` 门控 | ✅ 0 err | **2 failed**（`dbg_id_valid(0) == 0` / `non_retired == 21`） | 全绿 ⚠️ 见 §6.1 |
| M6 | `primed` 门控**全部三个装载** | ✅ 0 err | **1 failed**（`ta[0][kIdxIdPc] == 0x80000000`） | 全绿（组件用例不查 `valid`） |

### 6.1 ⚠️ 三条「组件级抓不到」的**如实记录**

- **M2 / M3（排空）**：`if_id_instr_src = select(skid_valid, skid, 流上)` ⇒ 只要
  `skid_valid` 还是 1，IF/ID 就**一直**取 skid（`skid_valid` 只由排空 / flush 清）
  ⇒ 关掉排空后 skid 变成**粘性**的（同一指令反复供给 IF/ID）
  ⇒ 组件级的两条断言（只看规则路径的 instr/pc）**看不出**差别
  ⇒ 但 SoC 级的写回序列 / `x19` / `x20` 计数断言**抓到了**。
- **M5（`primed`）**：组件级用例只断言 `instruction` / `pc`（`primed` 只门控 `valid`）
  ⇒ 抓不到 ⇒ 由 SoC 级的 `dbg_id_valid(0)` 与退休数抓。
- ⇒ 结论：**判别力按变异分工**，不是每个用例都要抓每种变异。M4 是唯一
  「只有组件级能抓」的一条 —— 这正是 ADR-048 D9.4 要求组件级用例的理由。

### 6.2 M1 的一个**差点漏掉**的点（记录）

组件级 Batch 4-a/4-b 的填充都发生在停顿的**第 3 拍** —— 那时 `pc_reg` 已被冻结、
`fetch_pc` 已追上 ⇒ **二者相等** ⇒ 那两条用例**无法**区分 M1。
⇒ 为此增设 **Batch 4-c**（停顿**首拍**填充，此时二者相差 4 字节）⇒ M1 才在组件级转红。
⚠️ Batch 4-a 的注释已按此**订正**（原注释错误地声称能抓 M1）。

## 7. 规格与簿记

- **spec delta**：`specs/rv32i-commit-path/spec.md`（**MODIFIED ×2**，已由 planner 定稿）
  ⇒ 归档 **禁** `--skip-specs`
- **archive 后手工订正主 spec 的 `## Purpose`**（`:33-36`）：delta 的 `## Purpose`
  不会被 CLI 合并进**既有** capability ⇒ 必须手改
- **文档**：根 `AGENTS.md`（§KNOWN LIMITATIONS 两条消解 + §NOTES）、
  `include/cpu/AGENTS.md`（CONVENTIONS 增补取指侧契约）、
  `docs/architecture/rv32i-microarchitecture.md`（「已知残留」节）
- **簿记**：`feat-rv32i-isa-completeness` ④ 勾选 + `roadmap.md`
- **计数三者均不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
  `ctest -L verilator_e2e` 37（两个用例都进**既有** target）
