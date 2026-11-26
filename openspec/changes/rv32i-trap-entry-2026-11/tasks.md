# tasks · RV32I **trap 入口接线**（P6-3）

> **Change**：`rv32i-trap-entry-2026-11` · **母 improvement**：`rv32i-privileged-csr-2026-11`（P6-3）
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（核 IP 顶层 53 端口契约表 + C6 快照**不动**）
> ⇒ ⇒ ⚠️ **开工前置 = ADR-048 的 dated addendum 落档**（architect 域 · design §7）

---

## T0 · 侦察（**只读**）

- [x] **T0.1** ✅ `ExceptionUnit` 的接口与既有语义（两拍写 `mepc`/`mcause` · `trap_pc = mtvec & ~3` ·
      `mret_pc = csr_mepc`）⇒ ⇒ **零改动复用**（design §1.1）
- [x] **T0.2** ✅ `CsrBank` 的**单写口**约束 + 已有 5 个 CSR 读输出
      （`mstatus_value` / `mtvec_value` / `mepc_value` / `mie_value` / `mcause_value`）
      ⇒ ⇒ `ExceptionUnit` 的读输入**无需新增端口** ✔
- [x] **T0.3** ✅ `if_stage` 的 flush / `flush_d1` / skid / `pc_update_en` 全貌 ⇒ ⇒
      `trap_flush` 与 `trap_flush_d1` 的**逐字同构**位置已定位
- [x] **T0.4** ✅ **D-1 的事实订正（实测）**：`mtvec` 复位值 `0x2000_0100` ⇒ ⇒ 取指地址
      `instr_addr[13:2]` ⇒ ⇒ **实际取到 I-TCM 的 word 0x040** ⇒ ⇒
      **「落在 TCM 外 ⇒ 静默重启」不成立**（是**别名**跳转）⇒ ⇒ 固件级判据**可行**

## T1 · 立项（**本 change**）

- [x] **T1.1** ✅ `proposal.md`（Why / What Changes / **IN·OUT 逐条** / AC1–AC11 / Capabilities）
- [x] **T1.2** ✅ `design.md`（载体与拓扑 / 三条具名不变量 / **同拍仲裁表** / **写口仲裁** /
      判据设计 / **ADR dated addendum 草案** / D-1 事实订正）
- [x] **T1.3** ✅ `tasks.md`（本文件）+ **spec delta**（`rv32i-privilege` **ADDED**）
- [x] **T1.4** ✅ `openspec validate` **valid**（零 WARNING）· `verify_openspec_delta.sh` **4/4 绿**

## T2 · **开工前置**（**architect 域** · builder **不写** ADR）

- [ ] **T2.1** ⚠️ **ADR-048 的 dated addendum 落档**（design §7.1 的 D1–D4 四条）
      ⇒ ⇒ **MUST 由 architect 落档**（`docs/adr/**` = architect 写集）
      ⇒ ⇒ **builder 只提供草案**（已在 design §7.1）⇒ ⇒ **未落档前不得实施**
- [ ] **T2.2** ⚠️ **D-1 的裁决**（用户 / architect 域）：`mtvec` 取值语义（base / 模式位 /
      复位值是否合理）⇒ ⇒ **不阻塞本项**（本项只把 `mtvec` 的**当前值**当输入）⇒ ⇒
      但**阻塞 P6-4** 的固件级恢复验证

## T3 · `if_stage` 的**第二路**重定向

- [ ] **T3.1** `if_stage.h`：`__io` **+2 输入端口**（`trap_target` / `trap_flush`）
      ⇒ ⇒ ⚠️ **追加在输入端口组末尾**（端口按声明顺序编号 ⇒ 插中间会位移）
- [ ] **T3.2** `trap_flush_d1`（**逐字同构** `flush_d1`）
- [ ] **T3.3** `next_pc_target` 三路（**trap > branch > PC+4**）+ `pc_update_en` 含 `trap_redirect`
      ⇒ ⇒ **trap 与 flush 同权**（都压过 stall）
- [ ] **T3.4** `if_id_update_en` 与 `skid_valid` 的清空条件含 `trap_flush | trap_flush_d1`
      ⇒ ⇒ ⚠️ **skid MUST 清空**（ADR-048 D5 同构）

## T4 · `pipeline` 的接线 + **写口仲裁**

- [ ] **T4.1** 实例化 `ExceptionUnit`（**inline** `ch_module` ⇒ ⇒ 层次化 module 数**不变**）
- [ ] **T4.2** `ExceptionUnit` 的 CSR 读输入 ← `csr_bank` 的**输出端口**（零端口变更）
      ⇒ ⇒ ⚠️ MUST 读**输出端口**侧（C1：读子组件输入 = 跨模块边 ⇒ 层次化发射失败）
- [ ] **T4.3** 触发点：`exception_valid = illegal_active & id_valid & !flush`
      ⇒ ⇒ ⚠️ **`!flush` 是承重条款**（AC5）
- [ ] **T4.4** `current_pc` ← **`if_stage.io().pc`**（**ID 级**那条指令的 PC ⇒ ⇒ 不是 `pc_out`）
- [ ] **T4.5` `exception_cause` = **2**（illegal instruction · RISC-V 规范）⇒ ⇒ **常量**
- [ ] **T4.6** **写口仲裁**：`ExceptionUnit` **优先**（design §4.2）⇒ ⇒ 父层 mux 出
      `csr_addr` / `csr_wdata` / `csr_write_en` 三者
- [ ] **T4.7** `mret_instruction` 接 **0**（P6-5）⇒ ⇒ `mret_valid` **被消费**（不进 PC mux 会冒泡）

## T5 · 判据（**逐条手推** + **每条一条变异**）

> ⚠️ **期望值 MUST 手推**；⚠️ **观测 MUST 零新增端口**（读 `pipeline.csr_bank.instance().io().mepc_value`）

- [ ] **T5.1** 新建 `examples/riscv-mini/tests/test_rv32i_trap_entry.cpp`（**流水线级**）
      ⇒ ⇒ AC1（`mepc`/`mcause`）· AC2（不退休 + IF/ID 被杀）· AC3（PC 重定向）·
      AC4（在途更老指令照常退休）· AC5（`flush` 优先）· AC6（`trap_flush_d1` ⇒ 连续两拍空）·
      AC7（`mepc`+`mcause` 成对）
- [ ] **T5.2** **每条判据一条变异**（还原错误实现 ⇒ 转红）⇒ ⇒ **MUST 仍能编译**
      ⇒ ⇒ 逐条登记变异编号与结果
- [ ] **T5.3** **固件级**补充判据（镜像 word 0x040 放停机指令 ⇒ ⇒ 验「别名跳转」可达）
- [ ] **T5.4** 注册 CTest ⇒ ⇒ `CTEST_TOTAL` +1 · `EXAMPLES_TEST_SOURCES` +1（锚点**同批**同步）
- [ ] **T5.5** **全量回归**：全量重建 ⇒ ⇒ `ctest -LE perf` **零失败**

## T6 · 产物面 / 契约 / 归档

- [ ] **T6.1** **零端口面实测**（AC8）：核 IP **53** 端口逐字不变 + C6 快照不变 +
      层次化 module 数不变（`ExceptionUnit` inline ⇒ 不新增层）
- [ ] **T6.2** **spec delta**：`rv32i-privilege` **ADDED**（**禁** `--skip-specs`）⇒ ⇒
      **既有 Requirement 不动** ⇒ ⇒ ⚠️ 同时**逐项**修订 capability 的「不覆盖」清单
      （**D-3 的接缝兑现**：本项关闭「`#2 mtvec 取值语义`」的**机制部分**）
- [ ] **T6.3** 文档同步：`include/cpu/AGENTS.md`（trap 通路段 + 行数锚点）· 根 `AGENTS.md`（计数锚点）
- [ ] **T6.4** **归档后手工更新** `openspec/specs/rv32i-privilege/spec.md` 的 `## Purpose`
- [ ] **T6.5** 验收报告：`.rddf/reviews/rv32i-trap-entry-2026-11-verify.md`
- [ ] **T6.6** 回写 roadmap：`phase-6.md` 的 **P6 行** + `phase-6-next-steps.md` + 母 improvement 进度

---

## 附：本项**不做**的事（防越界）

- ❌ `ECALL` / `EBREAK` / `MRET` / `WFI`（`funct3 = 0`）⇒ ⇒ **P6-4 / P6-5**
- ❌ 中断（`mtip` / `mie` / `mip` / `mstatus` 陷阱侧）⇒ ⇒ **P6-6**
- ❌ `mtval` 真值 ⇒ ⇒ **P6-7**
- ❌ `mtvec` 取值语义 ⇒ ⇒ **D-1**（用户 / architect 域）
- ❌ 只读 / 未实现 CSR 的可写性语义 ⇒ ⇒ 母 improvement **第 6 项排除**
- ❌ 动 `HazardUnit` 端口面 ⇒ ⇒ **红线**（`AGENTS.md:256`）
- ❌ 改 `CsrBank` / `ExceptionUnit` 的 `__io` ⇒ ⇒ **零公开 API 变更**
- ❌ 改 `docs/adr/**` ⇒ ⇒ **architect 写集**（builder 越权 = 流程违规）
