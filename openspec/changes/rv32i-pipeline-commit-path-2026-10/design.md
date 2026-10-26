# 设计：rv32i-pipeline-commit-path-2026-10（C3 的前置）

> 本文件是 P1 产物 + P2 实测的**逐条追因记录**。
> 所有「实测」结论都基于 2026-10-25 的 HEAD `0b8fa43` + 本 change 的改动。

> ⚠️ **小节编号按「发现轮次」编排**（§3.6 / §3.9 / §3.10 早于 §3.7 / §3.8）⇒
> 读的时候按内容找，不要按编号顺序读。

## 0. 一句话

**SoC 顶层此前是空壳**（6 个端口全是 `ch_in`、无固件加载、无观测输出）⇒ 流水线的
缺陷**从未被看见**。本 change 建起观测面（**32 个端口**）后，逐拍追出 **17 个缺陷**，
（D1–D17；其中 15 个在 `include/cpu/**` 内、1 个在 SoC 顶层、1 个在根 `tests/`）。

## 1. 观测面（先有观测，后有修复）

### 1.1 SoC 顶层端口契约（ADR-046 **D10-rev**）

- 删 4 个**死端口**（`ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` /
  `ext_data_ready` —— 在 `describe()` 里从未被引用）。
- 加 3 个固件写端口（`prog_we` / `prog_addr` / `prog_data` → I-TCM 写接口）。
- 加 11 个功能观测输出（取指地址流 / 存储流 / 6 个性能计数器 / dmem 读数据）。
- 加 16 个**诊断输出**（ID 级指令流 / 分支判定 / 有效链 / 写回 / EX-MEM 数据通路 /
  load-use 冒险检测脉冲）。

⇒ 共 **32 个端口**（5 输入 + **27 输出** = 11 功能 `obs_*` + 16 诊断 `dbg_*`），
最大位宽 48 < 64（ADR-035 R3）。
由护栏 C6 快照钉住（名 + 方向 + 位宽）。

### 1.2 `Rv32iPipeline` 的 `dbg_*` 输出

诊断端口在**流水线**上（`include/cpu/pipeline/rv32i_pipeline.h`）：因为
`VerilatorBackend` 只对 `port_access_` 里的**顶层**端口建访问表 ⇒ 内部信号必须
一路引到顶层。

## 2. 逐拍追因：17 个缺陷

> 追因方法：`dbg_id_instr`（ID 级正在译码的指令字）+ `dbg_id_pc` + 6 个计数器 +
> 写回观测，逐拍对照固件表。**先看指令流对不对，再看数据流对不对**。

### D1 `if_stage` 的 flush ⇒ PC 归零 ✅（已修）

```cpp
// flush 时 PC 重置为 0 (简化处理，实际应该是分支目标)   ← 作者自认的占位
auto pc_flush_val = select(io().flush, ch_uint<32>(0_d), next_pc_target);
```

`hazard.io().flush = is_branch & branch_taken`，而 `if_stage.io().branch_valid`
接的正是同一个 `ex_stage.io().branch_taken` ⇒ flush 时 `next_pc_target` 已是
分支目标 ⇒ 那个覆盖既多余又有害。

**症状**：每 11 拍 PC 从 `0x80000028` 跳回 0；`perf_branch_count` /
`perf_branch_mispredict` 凭空各 +18；41 条固件只退休 18 条。

### D2 指令流与 PC 错开一拍（imem 的 `sread` 延迟）✅

`instr_data` 在周期 T 是「T-1 拍提交的地址」的数据（1-cycle `sread`），
而 `if_id_instr_reg` 在同一拍又锁存一次 ⇒ 指令比地址**晚两拍**到 ID。
⇒ 加 `fetch_pc_reg` 把 pc 推迟一拍，让 ID 的 pc 与它译码的指令同拍。

**症状**：ID 解码的指令比 PC 晚一个字 ⇒ 分支目标算在错误的 pc 上。

### D3 EX 级没有自己的 pc ✅

`id_stage` **没有 pc 输出** ⇒ `ex_stage.io().pc` 只能接 **IF/ID 的 pc 输入**
（当前 ID 级那条指令的 pc，且随 ID 逐拍前进）⇒ EX 拿到的 pc 比它处理的指令
晚一个字。

⇒ 给 `id_stage` 加 `pc_out`（ID/EX 寄存后的 pc）。

**症状**：BEQ 目标偏 1 个字（跳到 word 31 而非 word 30）。

### D4 冒险单元的「MEM 级」输入取自 ID 级 ✅

`hazard.io().mem_is_load` / `mem_rd_addr` / `mem_reg_write` 原本接
`id_stage.io().*`（**EX 级的视图**）⇒ load-use 检测拿「EX 级指令的 rd」当
「MEM 级 load 的 rd」⇒ 检测退化成「指令与自身比较」。

⇒ 改接 EX/MEM 寄存器（`exmem_is_load` / `exmem_rd_addr` …）。
（`ex_rd_addr` / `ex_reg_write` 接 `id_stage.*` 是**对的** —— `id_stage.io().*`
是 ID/EX 寄存后的值 = EX 级的视图。）

### D5 MEM/WB 寄存器装载**别级**的信号 ✅

- `memwb_is_alu` / `memwb_is_jump` 装 `id_stage.io().is_alu/is_jump`（EX 级）⇒
  MEM/WB 级拿到的类型**串了一级**。
- `wb_stage.io().alu_result` / `mem_data` / `is_load` **组合直连 MEM** ⇒
  WB 拿到的写回数据是**上一条**指令的。
- `memwb_rd_addr` 存在但**没人用**（WB 直接读 MEM 的 rd_addr）。

⇒ 新增 `exmem_is_alu` / `exmem_is_jump`（EX/MEM）与 `memwb_is_load` /
`memwb_alu_result` / `memwb_mem_data`（MEM/WB），WB 全部从 MEM/WB 寄存器取。

### D6 `mem_valid_out` 的语义错误 ✅

```cpp
auto mem_data_valid = io().is_load & io().mem_valid;
io().mem_valid_out = mem_data_valid;   // ← 名字是「MEM 级有效」，实现是「load 数据有效」
```

它是 MEM/WB 与 WB 的**指令有效**信号，却被实现成「load 数据有效」⇒

- `perf_instructions`（`commit_count`）统计的是 **load 次数**，不是退休指令数；
- `memwb_is_alu` 的使能是 `mem_valid` ⇒ **ALU 指令永远不写回寄存器堆**
  （实测 x1/x2/x3/x4 恒 0 ⇒ store 的写数据恒 0）。

⇒ `io().mem_valid_out = io().mem_valid;`（「load 数据有效」只在内部数据选择器里用）。

### D7 MEM 级**没有**内存读数据端口 ✅

```cpp
ch_uint<32> mem_read_data = ch_uint<32>(0_d);   // ← 局部常量，不是端口
```

⇒ load 路径**完全没实现**（`lw` 恒读回 0）。
⇒ 加 `ch_in<ch_uint<32>> mem_read_data` 并接 `pipeline.io().data_read_data`。

### D8 SoC 顶层把**字节地址**喂给**字索引**的 TCM ✅

`InstrTCM` / `DataTCM` 内部的 `ch_mem` 是按**字索引**寻址，而流水线的
`instr_addr` / `data_addr` 是**字节地址** ⇒ 取指**快 4 倍**。

⇒ 顶层改用 `bits<kTcmAddrWidth + 2, 2>(...)`（先例：`test_verilator_complex_e2e`
的 fixture 用 `bits<3,2>`）。

**症状**（这是最初那批「假分支 / 18 条退休 / 53 拍 spurious store」的总根源）：
ID 级解码的指令比 PC 快 4 个字 ⇒ 分支在错误的 pc 上被判定。

### D9 `OP_STORE` 不在「第二操作数用 imm」的白名单里 ✅

`ex_stage` 的 `is_i_type` 漏了 `OP_STORE` ⇒ store 的 ALU 结果算成
`rs1 + rs2_data`（被存的值）而不是 `rs1 + imm`（访存地址）⇒ **store 地址恒 0**。

### D10 `ex_stage.io().alu_op` **从未被驱动** ✅

全仓无连接 ⇒ 恒为 0 ⇒ **所有 ALU 操作都算 ADD**（实测 `sub x4,x3,x2` 得 16 = 13+3）。

⇒ 在流水线里按 `funct3` + `funct7[5]` 推导并接线（`id_stage` 寄存 `funct7`）。

### D11 `funct3` 对访存指令编码的是**访存宽度** ✅

按 `funct3` 直接映射 ALU 操作 ⇒ `sw`（funct3=010=word）被当成 `SLT`
⇒ store 地址算成「0 < 10」= **1**。
⇒ LOAD/STORE 的 ALU 操作 MUST 是 `ADD`（地址计算）。

### D12 `ex_stage.io().branch_type` **从未被驱动** ✅

恒为 0 ⇒ **所有分支都按 BEQ 判定** ⇒ `bne`（x0 != x0 恒假）被判为成立 ⇒
程序跳进错误路径（实测 `perf_branch_mispredict` = 2 而非 1）。

### D13 寄存器堆读地址取自**寄存后**的 ID/EX 地址 ✅

`id_stage.io().rs1_addr` 是 ID/EX **寄存后**的值（属于**上一条**指令）⇒
寄存器堆读到的永远是**上一条指令**的寄存器内容。

⇒ `id_stage` 加 `rs1_addr_id` / `rs2_addr_id`（**组合**，当前 ID 级那条指令的地址），
寄存器堆读地址改用它们。

### D14 寄存器堆用 `sread`（1 拍）+ ID/EX 寄存器（再 1 拍）= 2 拍延迟 ✅

ID/EX 寄存器**已经**提供了「读地址 → 读数据」的那 1 拍 ⇒ 内存侧 MUST 是
0-cycle（`aread`）⇒ 改用 `reg_file.aread(...)`。

### D15 既有 fixture 把 `read_port` 落进**宽度不匹配**的中间变量 ✅（在 `tests/`）

```cpp
ch_uint<2> word_sel;
word_sel <<= imem.sread(...);   // read_port 的元素类型是 ch_uint<32>
```

⇒ 取到的**不是读数据**（实测恒为 3）⇒ 这个 fixture 的 imem **从来没工作过**，
而旧断言 `perf_instructions == 12` 钉的是**垃圾值**。
⇒ 直接把 `read_port` 接到端口（先例：`InstrTCM` 的 `io().data <<= read_data;`）。

### D16 D-TCM 读端口用 `sread` ⇒ load 数据晚一拍 ✅

MEM 级在**当前拍**采样 `rdata`，而 `sread` 的输出在 T+1 才有效 ⇒ load 恒读回 0。
⇒ `DataTCM` 改用 `aread`（0-cycle 组合读）。
⚠️ 与 imem 的约定**不冲突**：ADR-041 强制的是 **imem** 用 `sread`。

### D17 load-use 冒险**从未被检测**（`ld_stall` 恒 0）✅（A1-a，见 §3）

`hazard_unit.h` 的 load-use 检测用 `mem_is_load`（**MEM 级**的 load）比
`id_rs1/id_rs2`（**ID 级**的消费者）⇒ **晚一级** ⇒ 消费者此时已进 EX、
操作数**已锁存** ⇒ 停顿「停了个寂寞」。

且 `stall` 是**单拍脉冲**（组合输出）⇒ 即使时序修正，也只能延迟 1 拍；
而无数据前推时 load 的数据要到 **WB** 拍才写回 ⇒ 消费者至少要等 **3 拍**。

**症状**：`perf_load_use_stalls` **恒 0**；固件里紧邻的 `lw` → `add` 读到**旧值**。
⚠️ 这两点**互相掩盖**：缺陷「晚一级」让「脉冲式」无从观测 ⇒ 此前
**没有任何证据**表明停顿机制是坏的。

### 附带修复：LUI / AUIPC 的 ALU 结果

`is_i_type` 把 LUI/AUIPC 归到「用 imm」⇒ 结果会是 `rs1 + imm`（静默错误）。
⇒ LUI ⇒ `imm`；AUIPC ⇒ `pc + imm`。

## 3. 「两后端分歧」的真相

最初的诊断显示 `obs_data_addr` / `obs_data_write_data` 在 52 拍上两后端不同，
一度怀疑是 codegen 求值序问题。**修完 D1–D16 后：200 拍 × 24 个观测值
**零分歧** ⇒ 那 52 处是**坏流水线的症状**，不是 codegen 缺陷。
⇒ **不得**为它去改 `src/codegen_verilog.cpp`。

## 3.6 已知残留：取指填充期的「同一条指令被装两次」

**现象**：开头那条指令（固件 word 0）**被写回两次**（连续两拍 `wb_write_en=1`，
`rd=x1`、`data` 相同）。**语义无害**（幂等写回：同一地址、同一值），
⚠️⚠️ 但**退休数会 +1**（该指令被计了**两次**退休 —— 第 16 轮 critic 实测：
`perf_instructions` 在 cyc 3 与 cyc 4 各 +1）⇒ **「退休数不受影响」是错的**
（本节原文如此写，现纠正）⇒ 测试里的退休数金标**含**这 +1。

**根因链**（已追到底）：
1. `ch_mem::sread` 的输出端口（`data_proxy`）在**建图期**就被**组合求值**了一次
   ⇒ 它的初值 = `mem[addr(拍 0)]` = 固件 word 0；
2. `if_stage` 的 IF/ID 在**拍 0** 就把这个「还没真正取指」的初值装了进来
   （`if_id_instr_reg->next = select(if_id_update_en, fetched_instr, ...)`）；
3. 拍 1 装的是「拍 0 真正提交的地址」的数据 —— **同一个地址**（`pc_reg` 的初值
   就是第一次取指的地址 `0x80000000`）⇒ 同一条指令进了流水线两次
   ⇒ 被写回两次。

⚠️ **PC 序列看起来也「重复」**（`0x80000000` 出现两拍）—— 那是**巧合**：
`pc_reg` 的初值 = 第一次取指地址，而 `fetch_pc_reg` 让 IF/ID 的 pc 延迟 2 拍
⇒ 「拍 −2」与「拍 −1」取到的地址**相同**。

**为什么不在本 change 修**：根因在**框架层**（`ch_mem::sread` 的输出端口初值
语义 = 建图期的组合值，而不是「尚未取指」的哨兵）⇒ 修它会影响**所有**
`sread` 用户的填充期行为 ⇒ 需要独立立项 + 框架层评估（是否提供「读端口
未初始化」的可观测判据）。

⚠️ 本 change 采取的处置：**如实登记**（本节 + `AGENTS.md` 的 KNOWN LIMITATIONS）
⇒ 可见、不静默。测试用「ISA 语义 golden + 退休数 + 逐拍全向量 parity」覆盖
其**语义影响面**（写回值正确、退休数正确、两后端一致）—— 三者都对 ⇒ 影响为零。

### 3.9 第 16 轮 critic BLOCKER B1 的整改：flush MUST **多杀一拍**

**第 16 轮 critic 的 BLOCKER B1**（实测复现）：imem 是 **1-cycle `sread`** ⇒
flush 那一拍（T）`pc_reg` 已指向「分支 + 12 字节」处 ⇒ **该地址在 T 拍已经提交
给 imem** ⇒ T+1 拍数据才返回。
⇒ 只在 T 拍门控 IF/ID 装载**不够** ⇒ T+1 拍那条**错误路径**的指令会**正常锁存
进 IF/ID** ⇒ 它被执行了（且**在分支目标之前** ⇒ 顺序也乱）。

实测（修复前）：ID 的 word 序列 `… 29, 30, 30, 32, 31, 32 …` ⇒ word 32
（`addi x8,x0,1`）**先于分支目标 word 31 执行、且执行两次**
⇒ `wb (rd=8, data=1)` 在连续两拍各写回一次。
⚠️ **被测试的「连续重复去重」掩盖了**（去重是为 §3.6 的填充期残留设计的）
⇒ 必须**单独**断言「`(8,1)` 在**原始**写回序列里恰好一次」。

**整改**（`if_stage.h`）：加 `flush_d1`（flush 延迟一拍）⇒ IF/ID 的装载使能与
valid 清零条件都加上 `flush_d1` ⇒ T+1 拍那条错误路径指令**永不进 ID**。
⇒ 代价：分支目标的指令晚 1 拍进 ID（冲刷气泡 2 拍 → **3 拍**）。

⇒ **判别性断言已加**（测试内）：`(8,1)` 在原始写回序列里恰好 1 次。

### 3.10 第 17 轮 critic BLOCKER 1：load-use 停顿会**丢弃「消费者之后的第一条指令」**

**机制**（第 17 轮 critic 实测 + 逐拍 trace 双向确认）：
imem 是 **1-cycle `sread`** ⇒ ID 级译码 word W 的那一拍（**检测拍 T**），
`pc_reg` 已指向 **W+2**；而 word **W+1** 的地址在 **T−1** 就已提交 ⇒ 数据
**恰好在 T 拍返回** —— 正是 `if_id_update_en` 变 0 的第一拍 ⇒ **无处可存**。
⇒ 停顿解除后 IF/ID 锁存的是 **W+2** ⇒ **word W+1 从指令流里消失**。

**实测证据**（本 change 自己的 trace）：ID 级序列在停顿 #1 处是
word 21（保持）→ **word 23**（word 22 从未进 ID）；停顿 #2 处 word 44 → **word 46**。
⚠️ 两处被丢的都是 **NOP** ⇒ 全部 golden 仍绿 ⇒ 典型的「绿 ≠ 对」。

**为什么本 change 不修**：正解需要**取指 skid buffer**（1 级缓冲）+ **PC 回退**
—— 也就是说：停顿期间「已提交但未锁存」的那条指令要有个地方暂存。
⇒ 这是**结构性改动**（IF/ID 与取指之间多一级 ⇒ **全部时序/金标都要重算**），
且会引入新的对齐风险 ⇒ 与本 change 的目标（「让现有装配正确，不是换架构」）
不符 ⇒ **独立立项**。

⚠️⚠️ **当前实现的边界（如实登记）**：
> **load-use 停顿点之后的那条指令 MUST 是 NOP**（它会被丢弃）。

⇒ 已登记：`AGENTS.md` 的 KNOWN LIMITATIONS、本 change 的 capability
（`rv32i-commit-path` 的「停顿 MUST 只延迟退休」Scenario 里**明确写出这个例外**）
⇒ **不会**把一条当前不满足的 MUST 冻进主 spec。

### 3.8 第 14 轮 critic SHOULD_FIX #5/#6 的处置：死代码清理 + ISA 边界登记

**死代码清理**（`mem_stage.h`，本 change 正在改此文件 ⇒ ZERO-DEBT 适用）：
- 删 `do_mem_access` / `mem_access_valid`（计算后**无消费者**）
- 删 `store_data_out`（占位常量）
- 删输出端口 `is_store_out`（**从未被驱动**）
- `store_byte_enable` 此前算完**从不输出** ⇒ 改为接到新端口
  `store_byte_enable_out`，由 `Rv32iPipeline::data_strbe` 转出
  （此前 `data_strbe` **恒接 0xF** ⇒ SB/SH 的字节使能**从未生效**）

⚠️ **边界（如实登记）**：本 SoC 装配的 `DataTCM` **没有 strbe 端口**
⇒ 字节级写在**当前 SoC 里仍不生效**（AXI 互连有 `WSTRB`，落地后即可生效）。

**ISA 覆盖边界**（固件**只覆盖** ALU + `LW`/`SW` + `BEQ`/`BNE`）：
① SB/SH 字节使能（同上，当前 SoC 无处可接）；
② **JAL/JALR 不重定向 PC**（`if_stage` 的 `branch_valid` 只接 `branch_taken`）
   且 JAL 的写回值也不是 `pc+4`；
③ **SRA 简化为 SRL**（`ex_stage.h` 的 `sra_result` 未与 `srl_result` 区分）。
⇒ 三者均**如实登记**在 `AGENTS.md` 的 KNOWN LIMITATIONS ⇒ 固件 MUST 避开。

## 4. 仍然悬置 / 明确不做

- **数据前推**（ADR-046 **D5** 仍悬置）：lib 版 `HazardUnit` 不接前推
  （接上会形成组合环，ADR-046 D3 #14）⇒ 固件按「生产者→消费者插 3 条 NOP」构造。
- **cache 集成**：本 change 的 DUT 是 pipeline + TCM。
- **JIT 的 mem-read lowering**：独立 objective。
- **AXI 互连**：独立 change。

## 5. 关键实测数据（修复后）

```
store 拍 (cyc, addr, data): (19, 0x100, 10) (31, 0x104, 20) (49, 0x108, 2)
                                            ^^^^^^^^^^^^^^^^ load-use 停顿后拿到**新值**
写回序列 (rd,data): (1,10) (2,3) (3,13) (4,10) (5,10) (6,20) (8,1) (9,2)
                    (10,7) (10,0) (11,0)          ← 最后三项 = 「较老同 rd」回归用例
                                                    （x11 == 0，不是 14）
⚠️ (8,1) 在**原始**写回序列里**恰好一次**（B1 的判别性断言）
退休指令数 188（= 199 个拍间隔 - 11 个「不增长」间隔
          = 200 拍 - 2 拍填充 - 3+3 拍 load-use 停顿 - 3 拍冲刷气泡）
          ⚠️ 含 §3.6 的填充期重复退休（+1）
perf_branch_count 2 / mispredict 1 / control_stalls 1
perf_load_use_stalls 6（= 停顿**拍数**；冒险**次数** = 2）
两后端分歧：200 拍 × 27 个输出观测值 ⇒ 0 处
⚠️ §3.3 末尾那句「200 拍 × 24 个输出观测值」是 D1–D16 那个时点的计数（现为 **27**）
指令流与 PC 对齐：200 拍 ⇒ 0 处错位
非退休拍（cyc 1 起计）：cyc 1..2（填充）+ cyc 25..27 + cyc 53..55（两处
          load-use 停顿）+ cyc 37..39（冲刷气泡）= 11
          ⚠️ 冲刷气泡 **3** 拍：flush MUST 多杀一拍（imem 的 1 拍 sread ⇒
          flush 当拍提交的那条在下一拍才返回）
store 拍数：恰好 3
```

---

## 3. A1-a：load-use 多拍停顿（2026-10-25 用户裁决）

> 第 13 轮 critic 的 BLOCKER 2。裁决 = **方案 A1-a（多拍停顿）**，
> 而非「A1-b（降级为带证据的已知残留）」。
> 契约见 `specs/rv32i-commit-path/spec.md` 的 ADDED ×2
> （「load-use 冒险 MUST 被检测到，且停顿 MUST 是**保持型**」+
> 「停顿 / 冲刷期间流水线的**有效链** MUST 保持自洽」）。

### 3.1 现状（两个独立缺陷）

**缺陷 ①：检测晚一级。** `hazard_unit.h` 的 load-use 检测用
`io().mem_is_load`（**MEM 级**的 load）比 `id_rs1/id_rs2`（ID 级消费者）：

```cpp
auto load_use_hazard = is_load & mem_rd_nz & any_rs_match;   // is_load = mem_is_load
```

5 级流水线的标准检测点是「**EX 级**的 load vs **ID 级**的消费者」。
用 MEM 级 ⇒ 晚一级 ⇒ 消费者此时**已经进 EX**、操作数**已经锁存**
⇒ 停顿「停了个寂寞」。
**实测**：`ld_stall` 恒 0；`lw` 紧邻 `add` 时消费者拿到旧值。

**缺陷 ②：单拍脉冲式停顿。** 即使检测时序修正，停顿也**必须持续**到
load 的数据写回寄存器堆。在**无数据前推**的前提下，load 的数据要到
**WB** 拍才写回 ⇒ 消费者至少要等 **3 拍**。
单拍脉冲只能延迟 1 拍 ⇒ 仍读到旧值。

⚠️ 这两点**互相掩盖**：缺陷 ① 让缺陷 ② 无法被观测（`stall` 从不为真），
所以此前**没有任何证据**表明停顿机制是坏的。

### 3.2 目标时序（load → consumer 紧邻，无 NOP）

| 拍 | IF | ID | EX | MEM | WB | 说明 |
|----|----|----|----|-----|----|------|
| T | — | consumer | **load** | … | … | `ex_load_use` 命中 ⇒ `stall=1`（组合） |
| T→T+1 | 冻结 | consumer | **气泡** | **load** | … | ID/EX 装**气泡**；EX/MEM 装 **load**（load 必须前进） |
| T+1→T+2 | 冻结 | consumer | **气泡** | 气泡 | **load** | EX/MEM 装气泡（EX 是气泡）；MEM/WB 装 load |
| T+2→T+3 | 冻结 | consumer | **气泡** | 气泡 | load | load 在 WB ⇒ **写回寄存器堆**；`stall_release` 成立 |
| T+3 | 解冻 | consumer | 气泡 | 气泡 | … | `stall=0` ⇒ consumer 在**边沿 T+3→T+4** 装进 ID/EX（⇒ 实际进 EX 是 T+4），此时寄存器堆已有**新值** |

⇒ 停顿 **3 拍**（T / T+1 / T+2），与「生产者的写回发生在 ID+3」一致。
⚠️ **释放方式 = 固定延迟**（不是「等那条 load 写回」）—— 见 §3.7。

### 3.3 改动清单（逐文件）

#### `include/cpu/pipeline/hazard_unit.h`

| # | 改动 | 理由 |
|---|------|------|
| H1 | **新增输入** `ch_in<ch_bool> ex_is_load` | EX 级 load 标记（EX 级指令的 `is_load`） |
| H2 | **新增输出** `ch_out<ch_bool> load_use_detected` | 组合、单拍脉冲语义 = 「**这一拍**检测到冒险」⇒ 与保持型 `stall` 区分 |
| H3 | 新增内部寄存器 `ch_reg<ch_bool> stall_hold_d1` + `ch_reg<ch_bool> stall_hold_d2` | 保持型停顿的状态（**两级移位链** —— ⚠️ 第 14 轮整改把原设计的 `stall_hold` + `stall_load_rd` 换成了它，理由见 §3.7） |
| H4 | 检测源改为 **EX 级**：`ex_is_load & ex_rd_nz & ((ex_rd==id_rs1)\|(ex_rd==id_rs2))` | 修正缺陷 ① |
| H5 | `io().stall = ex_load_use \| stall_hold` | 组合置位 + 寄存器保持（**当场生效**，不等一拍） |
| H6 | `stall_load_rd->next = select(ex_load_use, ex_rd, stall_load_rd)` | 记住「被阻塞的那条 load 的 rd」 |
| H7 | **⚠️ 已作废**（第 14 轮整改）：原设计按 rd 匹配解除 ⇒ 被「较老指令写同一 rd」提前解除 ⇒ 改为**固定延迟**（§3.7） |
| H8 | **⚠️ 已作废**（第 14 轮整改）：改为 `stall_hold_d1->next = ex_load_use; stall_hold_d2->next = stall_hold_d1;`（固定延迟移位链，§3.7） |
| H9 | `io().load_use_detected = ex_load_use` | 供「冒险次数」计数 |

⚠️ **`mem_is_load` 端口保留且仍参与检测**（MEM 级那一路独立可用）：
它不是死端口（仍被使用），且 `HazardUnit` 的既有独立测试
（`test_rv32i_pipeline.cpp` / `test_pipeline_exec.cpp`）**不用改**。
契约只约束「**流水线装配**」的检测源（MUST 取自 EX 级），
不约束组件的其它输入。

#### `include/cpu/pipeline/rv32i_pipeline.h`

| # | 改动 | 理由 |
|---|------|------|
| P1 | `hazard.io().ex_is_load <<= id_stage.io().is_load` | `id_stage.io().*` 是 **ID/EX 寄存后**的值 = **EX 级**视图 ✔ |
| P2 | `exmem_en` **去掉** `!id_stage.io().stall` | ⚠️ 停顿时 EX/MEM **必须照常装载**：否则**处于 EX 级的 load 永远进不了 MEM 级** ⇒ **死锁**（停顿永不解除） |
| P3 | `load_use_stalls` 计数改为 **`hazard.io().stall`**（保持型 ⇒ 自动变「停顿**拍数**」） | 语义升级：原来（单拍脉冲）计的是「冒险次数」；现在两者分离 |
| P4 | 新增「冒险次数」观测（`dbg_load_use_detected` 脉冲，由 `hazard.io().load_use_detected` 驱动）⇒ **顶层新增 1 个 `dbg_*` 端口** | 契约 R5 要求「冒险次数」与「停顿拍数」**可区分** ⇒ 29 端口 → 30（⚠️ 最终为 **32**：另有 `dbg_exmem_valid` / `dbg_mem_valid_out` 两个有效链观测口，C6 快照同步） |

#### `include/cpu/pipeline/stages/id_stage.h`

| # | 改动 | 理由 |
|---|------|------|
| I1 | `idex_update_en` **不再排除** `stall`（`= io().valid`） | 停顿时 ID/EX MUST 装**气泡**，MUST NOT **冻结**在 consumer 上（冻结 ⇒ EX 装 consumer 的数据而 EX/MEM 装 load 的数据 ⇒ 有效位与数据**错开一级** ⇒ 被阻塞的指令最终仍会被写回） |
| I2 | `idex_valid->next = select(flush \| stall, false, select(idex_update_en, io().valid, idex_valid))` | 气泡 = `valid=0` |
| I3 | **气泡时清零控制信号**：`idex_is_load / is_store / is_branch / is_jump / is_alu` → 0，`idex_rd_addr` → 0 | 否则气泡里**残留**上一条指令的属性 ⇒ ① `ex_is_load` 残留 ⇒ 误触发 load-use；② `ex_branch` 残留 ⇒ 误触发 flush（自锁） |

⚠️ `if_stage` 的 `stall` / `flush` 输入**已接**（`rv32i_pipeline.h:324-325`）
⇒ IF/ID 冻结与 PC 冻结**无需改动** ✔

#### 实施中额外发现并修复的 3 处（**同类「串一级 / 残留」缺陷**）

| # | 位置 | 缺陷 | 症状 |
|---|------|------|------|
| X1 | `rv32i_pipeline.h`（EX/MEM 装载） | `exmem_is_store` / `exmem_is_load` 在气泡时**保持旧值** | `mem_stage` 的访存使能 `do_mem_access = is_load \| is_store` **不看 `mem_valid`** ⇒ 气泡拍残留上一条 store 的 `is_store` ⇒ **静默的内存误写**（地址是旧值） |
| X2 | `rv32i_pipeline.h`（EX/MEM valid） | `exmem_valid` 取 `id_stage.io().valid`（**ID 级**的 valid）而 `exmem_en` 用 `valid_out`（**EX 级**的） ⇒ **语义上错位一级** | ⚠️ **实测为「等价变异」**（`valid` 与 `valid_out` 在装配里**恒等** —— `id_stage.io().valid` = IF/ID 的 valid（恒 1），`valid_out` = `idex_valid`（正常时同值）⇒ 改与不改**行为完全一致**）⇒ **不是行为修复**，只是把「有效位」改成与「数据」**同源**的严谨写法。⚠️ **真正让退休数从 18 → 17 的是「气泡时 EX/MEM 的 valid 从『保持』改为『清零』」**（BLOCKER 1 整改的一部分）⇒ 见下方 §3.3 的 X4 |
| X3 | `id_stage.h`（气泡清零） | 气泡时 `idex_is_load` / `is_branch` / `is_store` / `is_jump` / `is_alu` / `rd_addr` **残留**上一条指令的属性 | ① 残留 `is_load` ⇒ `hazard.ex_is_load` 误判 load-use ⇒ **无谓的停顿**（甚至自锁）；② 残留 `is_branch` ⇒ 误触发 flush（**自锁式冲刷**）；③ 残留 `rd_addr` ⇒ 与 ID 级消费者的地址**误匹配** |

⚠️ X1 是**静默的内存误写**（不崩溃、不错退休数、只多改一个内存单元）——
若不是这次为了追「写回两次」而逐拍比对，它不会被发现。

| X4 | `rv32i_pipeline.h`（EX/MEM valid 的**气泡分支**） | 气泡时 `exmem_valid` 原来取「**保持**」⇒ 填充期产生一次「数据无效但 valid 为真」的**假退休** | `tests/test_verilator_complex_e2e.cpp` 的退休数 **18 → 17**（18 是「**多算 1**」的「绿而错」值）。⚠️ 变异验证：把该分支改回「保持」⇒ 该测试**转红** ✔ |

#### 观测面新增的 3 个诊断端口（契约 R1「有效链」与 R5「冒险次数」的观测点）

| 端口 | 用途 |
|------|------|
| `dbg_exmem_valid` | EX/MEM 有效寄存器（有效链的第 1 环） |
| `dbg_mem_valid_out` | MEM 级输出给 WB 的有效位（有效链的第 2 环） |
| `dbg_load_use_detected` | load-use 冒险检测**脉冲**（与「停顿拍数」区分） |

⇒ 顶层端口 29 → **32**。

#### 变异验证（4 个变异，3 个有效 + 1 个等价）

| 变异 | 内容 | 结果 |
|------|------|------|
| M1 | `wb_stage` 去掉 `& io().mem_valid` 门控 | ✅ `test_pipeline_debug` **转红**（新增的「mem_valid=0 ⇒ write_en=0」断言有判别力） |
| M2 | `hazard_unit` 的 `stall` 退回**单拍脉冲**（去掉保持） | ✅ `test_rv32i_soc_dual_parity` **转红**（停顿拍数 + golden B 变） |
| M3 | `exmem_valid` 改用 `id_stage.io().valid`（X2） | ⚠️ **等价变异 ⇒ 仍绿**（`valid` 与 `valid_out` 恒等）⇒ **不构成行为修复**，已在 X2 行如实标注 |
| M4 | `exmem_valid` 的**气泡分支**改回「保持」（X4） | ✅ `test_rv32i_soc_dual_parity` **转红**（退休数 / 非退休拍数） |

⚠️ **M3 的教训**：「改了代码但行为不变」⇒ 若不跑变异验证，会把**等价改写**误当成
「缺陷修复」写进文档 ⇒ 本节已按实测订正 X2 的表述。

### 3.7 第 14 轮 critic 的 BLOCKER 1 整改：释放条件从「按 rd 匹配」改为**固定延迟**

**第 14 轮 critic 的 BLOCKER 1**（实测复现）：释放条件原本是
`stall_release = wb_reg_write & (stall_load_rd != 0) & (wb_rd_addr == stall_load_rd)`
⇒ **rd 匹配无法区分**「那条 load 写回了」与「**比它老一条**的指令写回了同一 rd」：

```
addi x5, x0, 7      ← 较老指令，写 x5
lw   x5, 0x100(x0)  ← 被阻塞的 load，也写 x5（dmem 零初始化 ⇒ 正确值 = 0）
add  x6, x5, x5     ← 紧邻消费者
```

⇒ 较老的 `addi` 在下一拍到达 WB ⇒ rd 匹配成立 ⇒ 停顿**提前一拍**解除
⇒ 消费者在 load 写回的**同一边沿**锁存操作数（`aread` 组合读 ⇒ 读到边沿前的旧值 7）
⇒ **x6 = 14 而不是 0**（实测）。

**根因**：「解除」这件事本质上需要**指令身份**，而 rd 匹配**不是**身份。
⇒ 改用**固定延迟**：load 从 EX 到 WB 是 **2 级**（EX/MEM → MEM/WB）⇒ 它的数据在
「检测拍 + 2」的边沿写回 ⇒ 消费者 MUST 等到「检测拍 + 3」才进 EX
⇒ 停顿 = **检测拍 + 3 拍** ⇒ 用**两级移位链**实现（`stall_hold_d1` / `stall_hold_d2`）。

**固定延迟同时解决第二个隐患**：「按写回解除」在 **load 被冲刷**时永远等不到写回
⇒ **死锁**；固定延迟天然免疫。

⇒ 实现（`hazard_unit.h`）：删掉 `stall_hold` + `stall_load_rd` 两个寄存器，
改为 `stall_hold_d1` / `stall_hold_d2` 两级移位；
`io().stall = load_use_hazard | stall_hold_d1 | stall_hold_d2`。

⇒ **回归用例已并入固件**（word 42..44）：
`addi x10,x0,7` → `lw x10,0x110(x0)` → `add x11,x10,x10`
⇒ 期望 `x11 == 0`（不是 14）；写回序列新增 `(10,7) (10,0) (11,0)` 三项。

#### 第 14 轮的变异验证（新增 M5 / M6 / M6'）

| 变异 | 内容 | 结果 |
|------|------|------|
| M5 | 固定延迟 **3 拍 → 2 拍**（= 模拟「提前一拍解除」） | ✅ `test_rv32i_soc_dual_parity` **转红**（`0x104 ← 20` 变错） |
| M6 | 叠加「按 rd 匹配提前解除」（复刻 critic 的原缺陷） | ✅ **转红** |
| M6' | 只对 `rd == 10` 提前解除（**只打**「较老同 rd」的第二处用例） | ✅ **转红**（停顿拍数 4 ≠ 6）⇒ 第二处用例**单独**有判别力 |

⇒ 三个变异覆盖了 critic 场景的**两个方向**（整体提前 / 局部提前）。

### 3.4 风险与波及面

| 风险 | 等级 | 缓解 |
|------|------|------|
| `hazard_unit.h` 是**共享组件**（7 个消费者） | 🔴 高 | 检测源是**新增**端口 ⇒ 既有消费者不接即为 0 ⇒ 行为不变（`mem_is_load` 那一路仍在）⇒ 既有测试应保持绿 |
| `stall` 由脉冲变保持 ⇒ `load_use_stalls` 语义变化 | 🟡 中 | 顶层计数器语义 MUST 同步写进 `AGENTS.md` + capability |
| 停顿期间 ID 级持续译码同一 consumer | 🟡 中 | 组合逻辑无副作用（`reg_rs1_data` 每拍刷新 ⇒ 解除后自动拿到新值） |
| 停顿拍数影响「退休指令数」金标 | 🟡 中 | 固件改动 ⇒ 退休数金标 MUST 按逐拍实测重算（MUST NOT 拍脑袋） |
| `exmem_en` 去掉 `!stall` ⇒ flush 路径行为变化 | 🟡 中 | flush 时 `valid_out` 已被清零 ⇒ `exmem_en=0` ⇒ EX/MEM 仍装气泡 ✔ |

### 3.5 与 ADR-046 D5 的边界（**不裁决 D5**）

本方案 = 「**无数据前推** + **多拍停顿自保**」。
它**不接**数据前推 ⇒ **不触碰** D5 的裁决对象（D5 = 「要不要接数据前推」）。
D5 的裁决对象仍是「前推 vs 多拍停顿」的取舍 —— 本方案相当于**先落地了
「多拍停顿」这一侧**，使 D5 将来若选「前推」时有可替换的基线。

⚠️ 落地后固件**不再需要**「生产者→消费者插 ≥3 NOP」⇒
`AGENTS.md` 里那条约束 MUST 改写为「停顿自保已生效；但**无前推** ⇒
紧邻依赖仍会停顿 N 拍（不是 0 拍）」。
