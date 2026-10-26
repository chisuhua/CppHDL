# 提案：RV32I 流水线 commit / valid 通路修复（C3 的前置）

## Why

C3（`rv32i-soc-dual-parity-2026-10`）的立论是「让 RV32I SoC 级双路 parity 成立」。
P2 起步时先把 `Rv32iPipelineTop` 从**空壳**（6 个端口全是 `ch_in`、无固件加载、
无观测输出）改造成真 DUT，再写 41 条指令的双后端逐周期比对测试 ⇒

> **测试立刻证明 SoC 根本跑不动程序。**

| 实测（41 条固件 / 200 拍） | 现象 |
|---|---|
| `perf_instructions` | **18**（固件 41 条）——且这 18 次全是「重启后头几条」 |
| `perf_branch_count` / `perf_branch_mispredict` | **18 / 18**（固件只有 2 条分支） |
| `obs_instr_addr` | 每 11 拍从 `0x80000028` 跳回 **0** |
| `obs_data_write_en` | 53/200 拍置位，地址是 ALU 结果（10 / 13 / 0），**不是** store 地址 |

注入探针（固件开头插一个 NOP，整体右移一字）⇒ 重启点从 cyc 10 移到 cyc 12
⇒ **触发点跟着程序走** ⇒ 是「某条指令触发 flush ⇒ PC 归零」而非固定周期噪声。

### 为什么必须先修，而不是「记一笔继续做 C3」

1. **C3 的验收不可达**：没有可运行的程序，「≥20 指令双路 parity」写不出来。
   强行写只能断言「两端一致地错」—— 与 C1 立的「绿 ≠ 对」原则直接冲突。
2. **这些缺陷不是 C3 引入的**，而是**此前不可观测**：`if_stage` 的 flush 分支带着
   作者注释「简化处理，实际应该是分支目标」；`mem_valid_out` 的语义错误直接决定
   `perf_instructions` 的含义。SoC 顶层此前**没有任何输出端口** ⇒ 谁也看不见。
3. **既有测试是「绿而错」**：`tests/test_verilator_complex_e2e.cpp` 的
   `perf_instructions == 3`（3 条 **ALU** 指令，无 load）在「退休 = load 数据有效」
   的语义下**不可能**是「3 条指令退休」⇒ 该断言被伪事件满足，钉不住任何东西。

⇒ 本 change = **把观测面建起来 + 把 commit / valid 通路修对**，
让 C3（SoC 级双路 parity + JIT 回落契约）有可验证的对象。

## What Changes

### P1 观测面（DUT 端口契约，ADR-046 **D10-rev**）

- `Rv32iPipelineTop`：删 4 个死端口（`ext_instr_data` / `ext_instr_ready` /
  `ext_data_read_data` / `ext_data_ready` —— 在 `describe()` 里从未被引用）+
  加固件写端口（`prog_we` / `prog_addr` / `prog_data` → I-TCM 写接口）+
  加 **11** 个功能观测输出（取指地址流 / 存储流 / 6 个性能计数器 / dmem 读数据）。
- TCM 规模取 `ADDR_WIDTH = 12`（4096 字 = 16 KB）：`1<<20` 字的 `ch_mem` 在
  verilator 下是 4 MB 展开数组，对 20 条指令的固件毫无收益。
  代价 = 地址高位被截断 ⇒ 固件与数据 MUST < 16 KB。
- `Rv32iPipeline` 加**诊断用 dbg 输出**（ID 级指令字 / PC / is_branch /
  branch_taken / MEM 级有效）—— 仓库已有 `dbg_*` 先例（`SpiFsmFixture`）。
- 护栏 C6 快照更新 + 零消费者豁免条目移除（`EXPECTED_DEAD_ALLOWANCES 1 → 0`）
  + `--self-test` 的假顶层头同步。

### P2 commit / valid 通路修复（D2–D5）

- **D2** `mem_stage.mem_valid_out` 的语义：它**是**「MEM/WB 与 WB 的指令有效信号」，
  当前实现是「load 数据有效」（`is_load & mem_valid`）⇒ `perf_instructions`
  统计的是 load 次数。改为 MEM 级有效（`io().mem_valid`），
  `mem_data_valid` 保留给数据选择器内部使用。
- **D3** 「流水线级寄存器装载其它级信号」一整族：
  - 冒险单元的 **MEM 级**输入（`mem_is_load` / `mem_rd_addr` / `mem_reg_write`）
    MUST 取自 **MEM 级**（EX/MEM 寄存器），当前取自 ID 级
    ⇒ load-use 检测退化成「指令与自身比较」。
  - MEM/WB 寄存器（`memwb_is_alu` / `memwb_is_jump`）MUST 装载 **MEM 级**那条
    指令的属性，当前装载 ID 级。
  - ⚠️ 逐条核实后再改：ID→EX 在本设计里是**组合**连接（无 ID/EX 寄存器），
    所以 `exmem_*` 装 `id_stage.*` 在**同拍语义下**未必错 —— MUST 用实测判定，
    不得凭「看起来像 bug」改。
- **D4** `data_write_en` 在非 store 拍置位：根因待逐拍定位。
- **D5** 存储地址 / 写数据在两后端分歧（52/200 拍）：根因待定位
  （候选：父组件读子组件输出时的求值序、`ch_reg::operator<<=` 的 codegen 形态）。

### P3 测试与护栏

- `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（**example 侧** ——
  根 `tests/**` 不许 include example 路径，护栏 C3 会拦）：**48 字**固件 +
  编码器（手算常量交叉校验）
  + 双实例逐周期比对 + `RV32I_SOC_DEBUG=N` trace dump。
- 既有 `test_verilator_complex_e2e` 的 RV32I 断言改为**语义断言**。

## Acceptance

### A1 观测面

- [x] `Rv32iPipelineTop` 端口集合 = **32 个**（5 输入 + 27 输出 = 11 功能 `obs_*`
      + 16 诊断 `dbg_*`），
      无死端口，最大位宽 ≤ 64
- [ ] 护栏 C6 快照同步；零消费者豁免条目移除；`--self-test` 全绿

### A2 SoC 能跑完真实程序（核心）

> 固件 = **48 字**（ALU-only + `sw`→`lw`→**紧邻 use** ×2 + taken/not-taken 分支）。
> 数值 MUST 由**逐拍实测**确定并把推导记进 `design.md`（MUST NOT 拍脑袋）。

- [x] 退休指令数 == **程序结构推导值**（**188** = 199 个拍间隔 − 11 个不增长间隔；
      ⚠️ 含 §3.6 填充期的重复退休 +1）
- [x] `obs_instr_addr` 序列与取指顺序一致（含 taken 分支的跳转）
- [x] ID 级 PC 与它译码的指令**逐拍同拍**（imem 1-cycle `sread` ⇒ 极易错开）
- [x] `obs_data_write_en` **只在** store 拍置位（3 条 `sw` ⇒ 恰好 **3** 个 store 拍）
- [x] 三个 store 的 `(地址, 数据)` == ISA 正确值
      （`0x100←10`（纯 ALU 结果）/ `0x104←20`（**load-use 停顿后**的正确值）/ `0x108←2`）
- [x] **被冲刷的指令 MUST 从未执行**（错误路径的寄存器 MUST 保持 0）
- [x] `perf_branch_count` == 固件分支数；`perf_branch_mispredict` 有确定值
- [x] **`perf_load_use_stalls` == 6**（load-use 冒险 MUST 被检测到 ——
      修复前它**恒为 0** ⇒ 检测从未生效）
- [x] 「紧邻 load→use」的消费者拿到 **load 写回后的新值**（不是旧值）
- [x] 「较老指令写同一 rd」的场景**同样**自保（`x11 == 0`，不是 14）

### A3 双后端逐周期一致

- [x] **全部 27 个输出观测值** 200 拍**逐拍全等**（**无掩码**；掩码只允许覆盖
      「契约规定无意义」的拍，且 MUST 有非空断言）

### A4 测试诚实性

- [ ] 既有 `test_verilator_complex_e2e` 的 RV32I 断言改为语义断言
- [ ] 每个断言都能回答「它钉的是哪个行为」（避免再次出现「绿而错」）

### A5 门禁

- [ ] `ctest -LE perf` 零回归；`CTEST_TOTAL` 同步（`199 → 200`）
- [ ] examples 28/28；samples 全绿；7 个护栏 exit 0
- [ ] `openspec validate --strict` valid

## Non-Goals

- **不接数据前推**（ADR-046 **D5** 仍悬置未裁决）—— 本 change 落地的是
  「**无前推 + 多拍停顿自保**」：load-use 冒险 MUST 被检测到，停顿 MUST 是
  **保持型**（持续到那条 load 写回寄存器堆）⇒ 固件**不再需要**在
  「生产者→消费者」之间插 NOP（但仍会停顿若干拍）
- **不裁决 D5**：D5 的对象是「要不要接数据前推」；本 change 只是把
  「多拍停顿」这一侧落地，使 D5 将来若选「前推」时有可替换的基线
- **不实现 JIT 的 mem-read lowering**（独立 objective）
- **不重写流水线**：目标是让现有装配正确，不是换架构
- **不重开 C3 的 spec delta**：C3 保留 `verilator-rv32i-e2e` 的 5 条 ADDED
  （SoC 级双路 parity + JIT 回落断言），在本 change 落地后从 T2 续跑

## Impact

| 风险 | 等级 | 缓解 |
|------|------|------|
| D3 家族逐条修改会连环改行为 | 🔴 高 | 每改一处就跑 C3 的逐周期 diff（观测面已就位 ⇒ 可即时判别） |
| D2 改变 `perf_instructions` 的含义 ⇒ 既有断言全要重审 | 🟡 中 | 先 grep 全部 `perf_instructions` 断言（`tests/` + `openspec/specs/`），逐个改语义 |
| D5 若落在 `src/codegen_verilog.cpp` ⇒ 爆炸半径大 | 🟡 中 | 先用最小 DUT 二分定位；确认是 codegen 问题再评估是否拆独立 change |
| 修完 commit 通路后 load/store 才真正执行 ⇒ 可能暴露 dmem 时序问题 | 🟡 中 | 固件已按无前推构造；load-use 间隔 ≥3 NOP（**紧邻的已由停顿自保覆盖**；距离 ≥2 仍需间隔） |

**复现材料**：C3 的草稿（`openspec/changes/rv32i-soc-dual-parity-2026-10/drafts/`）
已含 DUT 端口契约、41 条固件、编码器、双实例比对与 trace dump ⇒ 本 change 直接复用。
