# tasks · RV32I CSR 指令（6 种 `funct3`）

> **Change**：`rv32i-csr-instructions-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（**P6-2**）
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（53 端口契约表 + C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**（`CsrBank` / `ExceptionUnit` 的 `__io` 一字未动）
> ⇒ ⇒ **新增 CTest 条目 1 个**（`test_rv32i_csr_instructions` · 流水线级）
> ⇒ ⇒ `CTEST_TOTAL` **+1** · `EXAMPLES_TEST_SOURCES` **+1**（**24 → 25**）
> （新测试源落在 `examples/riscv-mini/tests/` ⇒ ⇒ 命中该计数口径 (c)）
> ⇒ ⇒ 锚点**同批同步**（⚠️ 若与 phase-6 收口批次并行 ⇒ ⇒ **锚点编辑权 = 最后合入者**）

---

## T0 · 缺陷定位（**先见红**）

- [x] **T0.1** **先见红** ✅（2026-11-25 · builder）—— 探针 = **手写 `itcm.mem`**
      （`340020f3` = `csrrs x1, mscratch, x0`）+ **现成的 `tb_rv32i_soc --firmware`**
      ⇒ ⇒ 实测 `FIRMWARE-TRACE: retired=0 fetch0=0x340020f3 wb= stores=`
      ⇒ ⇒ `RESULT: checks=5 fails=2`（**F3 / F4 红**）⇒ ⇒ **取指成功 · 退休 = 0 ⇒ 冻结**
      ⇒ ⇒ ⚠️ **观测限制**：`tb_rv32i_soc` 的固件模式**观测不到 `illegal_reason` 的值**
      ⇒ ⇒ 「冻结」由 **retired = 0** **间接**得出 ⇒ ⇒ ⇒ `reason` 的具体值 MUST 由
      流水线级用例**直接观测 `dbg_illegal_reason`**（⇒ ⇒ 归 T4 的 AC7 / AC8）
- [x] **T0.2** **负向对照** ✅ —— 同链路换成 `addi x1, x0, 5`（`00500093`）
      ⇒ ⇒ 实测 `retired=15` · `wb=1:0x5` · `RESULT: checks=5 fails=0`（**5/5 绿**）
      ⇒ ⇒ ⇒ **同一 TB · 同一装载链路 · 同一镜像格式** ⇒ ⇒ 唯一差别是**指令种类**
      ⇒ ⇒ ⇒ **证明冻结是 CSR 指令特有的** ✔
- [x] **T0.3** **REDUCE** ✅ —— `funct3 = 4`（`340040f3`）⇒ ⇒ 实测 `retired=0` · `fails=2`
      ⇒ ⇒ ⇒ **保留组合当前也冻结** ⇒ ⇒ **整个 `OP_SYSTEM` 都是 `reason = 1`**
      ⇒ ⇒ ⇒ **「拿不到 `reason = 3`」已实测记录**（AC7 的先见红）

---

## T1 · 译码（**零新增端口** · design §1）

- [x] **T1.1** ✅ `rv32i_decoder.h` 内实现 `is_op_system` / `csr_legal_f3` /
      `sys_known_unimpl`（= `funct3 == 0`）/ `sys_reserved`（= `funct3 == 4`）
      **四个局部信号**（单一来源；**零新增 `io()` 端口**）
- [x] **T1.2** ✅ `illegal_reason` 的 select 链改为
      `select(sys_known_unimpl, 1, select(sys_reserved, 3, select(reserved, 3, select(legal_opcode, 0, 2))))`
      ⇒ ⇒ `legal_opcode = known_opcode | (is_op_system & csr_legal_f3)`
      ⇒ ⇒ ⚠️ **`known_opcode` 仍不含 `OP_SYSTEM`**（否则 `funct3 = 0` 掉到 0）
      ⇒ ⇒ `io().illegal_reason` 仍是**最后一个** `io()` 赋值
- [x] **T1.3** ✅ CSR 判定信号**不在 decoder 新增端口**（C1）⇒ ⇒ 由**流水线顶层**
      从「`id_stage.io().instruction` + `id_stage.io().illegal_reason`」推导
      ⇒ ⇒ **合法性单一权威仍是 decoder**（`csr_in_id()` = `valid & op==OP_SYSTEM & reason==0`
      ⇒ ⇒ **不重写** `funct3` 合法表 ⇒ 不会静默分叉）
- [x] **T1.4** ✅ T0 复跑：`funct3 = 010` ⇒ 正常退休（`retired = 15` · `wb=1:0x0`）；
      `funct3 = 4` ⇒ 仍冻结（`retired = 0`）· `funct3 = 0` ⇒ 仍冻结
      ⇒ ⇒ 具体 `reason` 值（1 vs 3）由 T4 的流水线级用例直接观测 `dbg_illegal_reason` ✔

---

## T2 · 数据通路（读 → 写回 · design §2 / §8 的**载体订正**）

> ⚠️ **实施期载体订正**（design §8.1）：`rv32i_pipeline_regs.h` 的 4 个结构体
> **在流水线实现里零使用**（实测 grep）⇒ ⇒ 真实载体 = **`rv32i_pipeline.h` 顶层的 `ch_reg`**
> ⇒ ⇒ **改动面 3 个文件**（decoder / pipeline / wb_stage）+ id_stage 的**内部** mux 一行。
> ⚠️ **T2.3 / T2.5 / T2.6 的「改 stage」条目作废**（顶层已能做全部事）⇒ ⇒
> 只在 id_stage 做**一处内部改动**（`rs1_addr_mux` 含 `OP_SYSTEM`）。

- [x] **T2.1** ✅ `rv32i_pipeline.h` 顶层新增 9 个 `ch_reg`：
      `csr_idex_valid` / `csr_idex_addr` / `csr_idex_read_value`（ID→EX）+
      `exmem_is_csr` / `exmem_csr_read_value` + `m2_is_csr` / `m2_csr_read_value` +
      `memwb_is_csr` / `memwb_csr_read_value`（**M2 段不可省** —— MEM 区是 3 段）
      ⇒ ⇒ **零新增端口**（顶层内部状态）
- [x] **T2.2** ✅ `id_stage.h`：`rs1_addr_mux` 纳入 `OP_SYSTEM`（**内部信号** ·
      **`IdStage.__io` 一字未动**）⇒ ⇒ CSR 的寄存器型源走寄存器堆 + 前推
      ⇒ ⇒ ⚠️ 立即数型把 `rs1` 位当地址读是**无害**的（值不被消费）
- [x] **T2.3** ✅ `CsrBank` 在**顶层**实例化（`ch::ch_module<CsrBank> csr_bank`）
      ⇒ ⇒ `mtip` / `meip` **显式 tie 0**（AC9）· 读值锁存进 `csr_idex_read_value`
      ⇒ ⇒ **地址口**在「EX 级有 CSR」时切到 EX 的地址（写 MUST 用**本指令**的地址）
- [x] **T2.4** ✅ `csr_wdata` 的 6 种语义（EX 级）：`CSRRW/CSRRWI` = 源值 ·
      `CSRRS/CSRRSI` = `csr_old | 源` · `CSRRC/CSRRCI` = `csr_old & ~源`
      ⇒ ⇒ ⚠️ 源 = `zimm`（立即数型）或 **前推后的** `ex_stage.io().rs1_data`
      ⇒ ⇒ ⚠️ 写与读用**同一个** `csr_old`（`csr_idex_read_value`）
      ⇒ ⇒ 写使能 = `CSRRW/CSRRWI 恒写` ∪ `源 ≠ 0`（set/clear 型）
- [x] **T2.5** ✅ 写回 mux **三处同构**：`wb_stage.h`（新 +2 输入端口 · **被顶层消费 ⇒ 不冒泡**）·
      `m2_write_data`（档位 2 数据源）· 档位 1 的 `ex_mem_alu_result` 选择
      ⇒ ⇒ `rd = 0` 由既有 `rd_not_zero` 覆盖（零新增逻辑）
- [x] **T2.6** ✅ 前推：`hazard.ex_reg_write` / `mem_reg_write` 补 CSR
      ⇒ ⇒ 距离 1 / 2 / 3 的消费者都能读到 CSR 的写回值
- [x] **T2.7** ✅ **验证**（固件级实测 · 手推）：`csrrw/csrrs/csrrc/csrrwi/csrrsi/csrrci`
      序列的写回序列 = **手推值逐条相等**（见 T4 的判据用例）

---

## T3 · RAW 停顿（design §4）

- [x] **T3.1** ✅ `csr_stall = csr_in_id() & csr_idex_valid & !flush` 落 **`rv32i_pipeline.h` 顶层**
      ⇒ ⇒ **零新增 `HazardUnit` 端口**（`AGENTS.md:256` 红线）
- [x] **T3.2** ✅ OR 进 `id_stage.io().stall` 与 `if_stage.io().stall`
      ⇒ ⇒ `flush` 优先（与既有 PC 更新门一致）
- [x] **T3.3** ✅ **判别力实测**（变异 M1：`csr_stall = ch_bool(false)`）
      ⇒ ⇒ 相邻 `csrrw x2, mscratch, x1` + `csrrs x3, mscratch, x0` 的 `x3`
      从 **0x12345678（新值）** 变 **0x0（旧值）** ⇒ ⇒ **判据必红** ✔
      ```bash
      # 复现（M1）
      #   把两处 `auto csr_stall = csr_in_id() & csr_idex_valid & !hazard.io().flush;`
      #   改成 `auto csr_stall = ch_bool(false);` ⇒ 重建 ⇒ 跑
      #   build/examples/riscv-mini/test_rv32i_csr_instructions
      #   ⇒ ⇒ 「CSR - 相邻两条 MUST 有序（读到新值）」转红
      ```

---

## T4 · 判据（**逐条手推** + **每条一条变异**）

> ⚠️ **期望值 MUST 手推**（RV32I ISA 语义）⇒ ⇒ **禁抄仿真输出**
> ⇒ ⇒ 解释器与 codegen 由**同一份 C++**生成 ⇒ ⇒ **两后端一致地错** ⇒ ⇒
> ⇒ ⇒ **parity 对该族缺陷结构性失明** ⇒ ⇒ ⇒ **parity 只能当辅助证据**

- [x] **T4.1** ✅ 新建 `examples/riscv-mini/tests/test_rv32i_csr_instructions.cpp`
      （**流水线级** · 核 IP 顶层 DUT · **8 用例 / 30 断言**）⇒ ⇒ 覆盖：
      6 种 `funct3`（AC1）· 读回（AC2）· RAW 新值（AC3）· set/clear 语义（AC4）·
      立即数型（AC5）· 不读/不写（AC6）· `funct3=4` ⇒ reason 3（AC7）·
      `funct3=0` ⇒ reason 1（AC8）· `mip` 读 0（AC9）
- [x] **T4.2** ✅ **变异登记**（每条都**能编译**；跑判据二进制 · 见下表）
      | # | 变异 | 结果 |
      |---|---|---|
      | M1 | 去掉 `csr_stall` | **RED**（AC3：`x3` 旧值 0） |
      | M2 | `CSRRC` 用 `^` 代替 `& ~` | **RED**（AC4：`x4`/`x6` 错值） |
      | M3 | 立即数型改用 rs1 值 | **RED**（AC5：`x3`/`x4`/`x6` 错值） |
      | M4 | `CSRRW` 源 0 时不写 | **RED**（AC6：`x3` 错值） |
      | M5 | `CSRRW/CSRRWI` 写成旧值 | **RED**（AC1：写回序列错） |
      | M6 | 把 `OP_SYSTEM` 整体归合法（**单改 `legal_opcode`**） | **GREEN = 无效变异** ⇒ 原因：`sys_known_unimpl` 在 select 链**更前** ⇒ 值不变（**不是**判别力缺口） |
      | M6b | 同时去掉 `sys_known_unimpl` + 整体归合法 | **RED**（AC8：期望 1，实得 0） |
      | M7 | 保留组合仍归 1 | **RED**（AC7：期望 3，实得 1） |
      | M8 | `mtip` 不 tie 0 | **RED**（AC9：`mip` 读到 0x80） |
- [x] **T4.3** ✅ 负向对照（AC8 / AC7）：`funct3 = 0` ⇒ `illegal_reasons == {1}` + `retired == 0`；
      `funct3 = 4` ⇒ `{3}` + `retired == 0`；合法 CSR 程序 ⇒ `illegal_reasons` **为空**
- [x] **T4.4** ✅ 注册 CTest（`examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test`）
      ⇒ ⇒ `CTEST_TOTAL` **282 → 283** · `EXAMPLES_TEST_SOURCES` **24 → 25**（锚点同批同步）
- [x] **T4.5** ✅ **全量回归**：全量重建后 `ctest -LE perf`（结果见 T5 的报告）

---

## T5 · 产物面 / 契约 / 归档

- [x] **T5.1** **零端口面实测比对**（AC10）⇒ ⇒ 核 IP 顶层 **53** 端口契约表**逐字不变**
      + C6 快照**逐字不变** + **产物顶层端口表逐字不变**
      ⇒ ⇒ ⚠️ 「推断不冒泡」是**推断** ⇒ ⇒ **实测红 ⇒ 即端口冒泡了** ⇒ ⇒ 届时 MUST 同批改
      独立 Verilog TB 的端口映射 + `examples-rtl-sim-targets.tsv` + 期望值表
      ⇒ ⇒ ✅ **实测通过**：扁平 / 层次化产物的顶层端口表 **各 53 根（逐字不变）** ·
      层次化 module 数仍 **8**（`CsrBank` inline · 不新增层）·
      `scripts/verify_riscv_soc_core_ports.sh` **V1/V2/V3/V4/V5 全绿**
      （53 端口逐字逐序 + 核自有 51 与 C6 快照闭环）
- [x] **T5.2** ✅ **spec delta**：`openspec/changes/rv32i-csr-instructions-2026-11/specs/rv32i-privilege/spec.md`
      ⇒ ⇒ `## ADDED Requirements`（**禁** `--skip-specs`）⇒ ⇒ **既有 Requirement 不动**
      ⇒ ⇒ `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 无错
- [x] **T5.3** ✅ **文档同步**：`include/cpu/AGENTS.md`（CSR 通路段 + `illegal_reason` 语义订正
      + **行数锚点** 7169 → **7384** · `rv32i_pipeline.h` 972 → **1137**）
      + 根 `AGENTS.md`（若「CSR 不在覆盖内」的表述变化）⇒ ⇒ ⚠️ **根 AGENTS.md 的
      AUTO 块不动**（`verify_roadmap_fragments.sh` 守）⇒ ⇒ ⇒ **计数锚点段照惯例改**
- [x] **T5.4** ✅ **归档后手工更新** `openspec/specs/rv32i-privilege/spec.md` 的 `## Purpose`
      （2026-11-25 · 归档完成 ⇒ 已改写为「两段契约」：陷阱入口写入 + **CSR 指令语义与顺序**；
      删除「**不**覆盖 CSR 指令语义」并标注**已作废**；仍不覆盖项 = `funct3=0` / `mtval` / `mtvec` /
      `mstatus` 陷阱侧 / `MRET` / 中断 / 并发排队）
      ⇒ ⇒ 删掉「**不**覆盖 CSR 指令语义（`csrrw`/`csrrs`/`csrrc`）」一句
      ⇒ ⇒ ⚠️ **delta 里的 `## Purpose` 不会被合并** ⇒ ⇒ MUST 在 archive **之后**手工改
      （见 `docs/developer_guide/openspec-specs-guide.md` §「Purpose 段 MUST 在 archive 之后手工写入」）
- [x] **T5.5** ✅ **验收报告**：`.rddf/reviews/rv32i-csr-instructions-2026-11-verify.md`
      （AC1..AC13 逐条 + 工具证据 · 全量回归 **272/272** · 遗留清扫 · 已用轮次 1）
      ⇒ ⇒ 判据来源 = proposal 的「验收标准」段（AC1..AC13）⇒ ⇒ **逐条 + 工具证据**
      ⇒ ⇒ 遗留清扫复核（TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 /
      未替换的 mock / 临时 fallback）⇒ ⇒ 已用轮次
- [x] **T5.6** ✅ **回写 roadmap**：`phase-6.md` 的 **P6 行** = ✅ **P6-2 已闭**（含归档路径与判据）
      + `phase-6-next-steps.md` **§8 P6-2 闭合记录**（实施期 5 条发现 + 仍待办 P6-3..P6-7）
      + 母 improvement 的进度行
      ⇒ ⇒ 记 P6-2 已闭 ⇒ ⇒ 后续 P6-3..P6-7 的前置状态随之更新
      ⇒ ⇒ ⚠️ **`phase-6.md` 是状态真相** ⇒ ⇒ MUST 同批更新（否则制造漂移源）

---

## 附：本项**不做**的事（防越界）

- ❌ `ECALL` / `EBREAK` / `MRET` / `WFI`（`funct3 = 0`）⇒ ⇒ **P6-4 / P6-5**
- ❌ trap 入口接线（`mepc` / `mcause` 写）⇒ ⇒ **P6-3**
- ❌ 中断（`mtip` / `mie` / `mip` / `mstatus` 陷阱侧更新）⇒ ⇒ **P6-6**
- ❌ `mtval` 真值 ⇒ ⇒ **P6-7**
- ❌ `misa` / `mip` 的可写性语义 ⇒ ⇒ 母 improvement 的**第 6 项排除**
- ❌ 动 `HazardUnit` 端口面 ⇒ ⇒ **红线**（`AGENTS.md:256`）
- ❌ 新增 decoder / stage 的 `io()` 端口 ⇒ ⇒ **端口位移**（design §0 的 C1 / C2）
