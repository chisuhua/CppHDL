---
name: rv32i-privileged-csr-2026-11
priority: P1
phase: phase-6
category: cpu
type: feature
状态: draft
进度: **立项（2026-11-16）** · ✅ **P6-2 已闭（2026-11-25 · builder）** · change `openspec/changes/archive/2026-11-25-rv32i-csr-instructions-2026-11/`（`rv32i-privilege` **ADDED ×2**） ⇒ ⇒ 6 种 CSR 指令落地（**读在 ID · 写在 EX · `CsrBank` 单地址口 mux · 相邻两条停 1 拍**） ⇒ ⇒ **零端口面变更**（53 端口逐字不变 · 层次化 module 数仍 8）· **零新增 `HazardUnit` 端口** ⇒ ⇒ 判据 = `test_rv32i_csr_instructions`（8 用例 / 30 断言 · 变异 7 红 + 1 无效 + 1 组合红） · 全量回归 **272/272** · 验收报告 = `.rddf/reviews/rv32i-csr-instructions-2026-11-verify.md` ⇒ ⇒ ⬜ **P6-3..P6-7 未做**（trap 入口 / `ECALL`·`EBREAK` / 中断 / `mtval`） · ⚠️ **D-2（`mcause` 编码表 / `WFI` 归属）仍待裁决**（归 P6-4 / P6-6） · ⚠️ P6-1（G5）**已先闭**（change `2026-11-16-csr-mcause-write-2026-11`）
主题: RV32I **特权级**：CSR 指令 · `ECALL` / `EBREAK` · `MRET` · trap 入口接线 · 中断
依赖: `rv32i-isa-gap-closure-2026-11`（G1 / G4 / G5 已闭 · ADR-071 Accepted）· ADR-048（取指 skid / flush 不变量）· ADR-069（内存时序）· **ADR-047**（前推 ⇒ CSR RAW 判据要用）· **ADR-049**（`mtime` 逐拍）
来源: **R7 侦察**（`.rddf/checks/isa-gap-reuse-survey.md` · 2026-11-15 只读实测）+ capability `rv32i-privilege` 的**范围声明**（5 项排除）+ **metis 1 轮**（17 条 finding · A/B 全采纳）
生成时间: 2026-11-16
evidence: probe:.rddf/checks/isa-gap-reuse-survey.md
---

# Improvement: RV32I **特权级**（CSR 指令 / `ECALL` / `EBREAK` / `MRET` / trap 入口 / 中断）

> ⚠️ **本项是「独立立项」的产物**：根 `AGENTS.md` 的 KNOWN LIMITATIONS 明文
> 「**CSR / `ECALL` / `EBREAK`** 不在任何仓内 capability 覆盖内 ⇒ 需**独立立项**（不可顺手塞进别的条目）」
> ⇒ ⇒ 本草案**只立项**（设计 + 判据 + 排期），**不实施**。
> ⚠️ **本草案经 metis 1 轮**（17 条 finding）⇒ ⇒ **Priority A（7 条）+ B（6 条）已全部采纳**
> ⇒ ⇒ Priority C（4 条）见末节「表述精度」。

---

## Why（为什么现在立项）

**可验证目标（user story）**：跑通「**设 `mtvec` → `ECALL` → handler 读 `mepc`/`mcause` → `MRET` 回 → 再前进一条**」
的**最小固件** ⇒ ⇒ 这条是本项**唯一**能证明「可恢复」的验收形态
（组件级用例只能证明「写进去 / 读回来」，**证明不了「特权软件能跑」**）。

1. **承重组件已经存在，且组件级测试已有**（R7 侦察实测）：`riscv::CsrBank`（9 个 CSR · 单写口）·
   `riscv::ExceptionUnit`（trap 入口 + `MRET`）· `ch::chlib::Clint`（64 位 `mtime` · ADR-049）·
   地址映射单一真相 · MMIO 解码。
2. **生产消费者 = 0**（`include/cpu/pipeline/**` 的符号命中 = 0）⇒ ⇒ 它们**不在交付路径上**。
3. **G1 / G4 / G5 已把「可观测」补齐**（ADR-071 Accepted · capability `rv32i-privilege` 的种子）
   ⇒ ⇒ 剩余差距 = **「可观测」→「可恢复（trap）」**（ADR-071 §2.3 的升级路径**同一个检测点**）。
4. ⚠️ **真正的驱动力 = 「冻结是终态」必须被消除**（ADR-071 §4.3 风险① 明写「没有恢复路径，只能复位」）
   ⇒ ⇒ 这决定验收重点：**冻结 → trap 的状态转移** 比「CSR 指令能读写」**更重要**。
5. ⚠️ **一个未登记的动机**：本项是「**接 cache / 真中断 SoC**」的**前置**
   ⇒ ⇒ 没有 trap handler ⇒ 任何带错误的固件**只能冻死** ⇒ ⇒ 故**中断不能后置**到本项之外。
6. ⚠️ **消费者事实（如实登记）**：仓内**无**「下游特权级软件消费者」的证据
   （ChipForge 只出现在 API 可见性 / 析构 / 分层 RTL / ADR-059/061/062）
   ⇒ ⇒ **MUST NOT** 把「必须能跑某段外部固件」写成验收（那会变成**不可判定项**）。

---

## ⚠️ 必须先裁决（**blocking decisions** · 未裁决前不得开工）

| # | 决策 | 现状 / 冲突 | 为什么阻塞 |
|---|---|---|---|
| **D-1** | **trap 目标的**可达性** | `CsrBank` 的 `mtvec` 复位值 = `0x2000_0100`（base = `0x2000_0000`）· 而 SoC 取指做 `bits<13,2>(instr_addr)`（**高位被静默丢弃**）⇒ ⇒ trap 目标落在 TCM 外 ⇒ ⇒ **从固件第一条重新开跑**（= 「可恢复」是**假陈述**） | 不裁决 ⇒ 「trap 落地后不设 `mtvec`」**不等于**可恢复，而是**静默重启** ⇒ ⇒ 验收形态无法定义 |
| **D-2** | **`mcause` 编码表** | ⚠️ **`bit31` = 中断位**（`3` vs `0x8000_0003` 必须写死）· `ECALL` = **11**（M-mode；组件头注释的 **8** 是 **U-mode** ⇒ **不可达**）· `EBREAK` = 3 · **`WFI` 归属未定**（见下） | 不裁决 ⇒ P6-4/P6-6 的期望值**无唯一解** |
| **D-3** | **5 项排除的**逐项**归属 step** | 原草案写「接线 change **一次**全改」⇒ ⇒ 那时 P6-5/P6-6/P6-7 **还没落地** ⇒ ⇒ spec 会**声称未实现的覆盖** | 必须**逐项由关闭它的那一步修订**（见 Acceptance #2） |

⚠️ **`OP_SYSTEM` 的 `funct3 = 0` 全表没人认领**：6 种 CSR `funct3`（1/2/3/5/6/7）**刻意跳过 0**，
而 `ECALL`/`EBREAK`/`MRET`/**`WFI`** 全在 0 里 ⇒ ⇒ **`WFI`（`imm=0x105`）没有 step 认领**
（`test_csr_unit.cpp` 里已有 WFI 用例）⇒ ⇒ 落地后是「trap 成 illegal」/「NOP」/「仍 `reason=1`」**未定** ⇒ ⇒ 属 D-2。
⚠️ **`funct3 = 4`（保留）现在是 `reason=1`**（`known_unimplemented` 优先级 1）⇒ ⇒ P6-2 后**必须变 `reason=3`**
⇒ ⇒ 这是**行为变更**（草案初版**漏了**）。
⚠️ **第 6 项排除（新登记）**：**未实现 / 只读 CSR 地址的可写性语义** —— 现在 `CsrBank` **静默丢弃**
（`rv32i_csr.h` 的 misa / mip 分支）⇒ 读返回 0 ⇒ ⇒ RISC-V 要求 illegal ⇒ ⇒ 本版**不 trap**（排除）。

---

## What Changes（**按接入顺序** · 判据前瞻）

⚠️ **P6-1（`mcause` 可写）已闭**（G5 · 跨两拍）⇒ ⇒ 下表从 **P6-2** 起。

| 步 | 内容 | 依赖 | 判据（前瞻） |
|---|---|---|---|
| **P6-2** | **CSR 指令**：6 种 `funct3`（1/2/3/5/6/7）· 读改写语义 · `rd`/`rs1` = `x0` 的「不读 / 不写」 | P6-1 | 6 种 `funct3` **逐条**用例（期望值**手推**）**+ 每条一个变异**（还原错误实现 ⇒ 转红）· `illegal_reason` 对它们**不再**报 `1` · **`funct3=4` 改为 `reason=3`** · **CSR RAW**（读写距离 1 ⇒ 读到**旧值**或 stall，必须选一） |
| **P6-3** | **trap 入口接线**：**第二路 redirect 输入**（**内部** trap / mret mux — ⚠️ 不是「第三路 PC 源」：新增的 redirect **源有两个**）· `trap_flush` · `mepc` 保现场 | P6-1 · P6-2 | 见下「P6-3 的三条具名不变量」+ **同拍仲裁表** |
| **P6-4** | `ECALL` / `EBREAK`（`mcause` = **11** / **3**） | P6-3 | trap 后 `mepc` 指向**该指令**（两者**不前进**）· `mcause` 手推值 · ⚠️ **`MRET` 返回 `mepc` = ECALL 的 PC ⇒ 会重执行 ECALL ⇒ 无限 trapping**，除非 handler 自己 `+4` ⇒ ⇒ 测试模型 MUST 体现 |
| **P6-5** | `MRET` | P6-3 | 从 `mepc` 返回 + `mstatus.mie` 恢复（`mpie → mie`）· `mepc` 对齐 · **`MRET` 由哪一级发起**（ID / EX？决定它与 taken 分支是否同拍） |
| **P6-6** | **中断**（CLINT ⇒ `mtip` · `mie`/`mip`/`mstatus`） | P6-5 · **D-3 的 #3/#5 必须先做** | ⚠️ **受理条件 = `mip & mie & mstatus.MIE` · 采样点在**哪一级哪一拍**（P6-6 的**唯一**真设计问题）· 异常**优先**于中断（RISC-V）· `mcause[31]=1` · ⚠️ **trap 入口不清 `mstatus.MIE`** ⇒ handler 里定时器立即重入 ⇒ 每个中断覆盖 `mepc` ⇒ **`MRET` 回不来** ⇒ ⇒ 该「陷阱侧 `mstatus` 更新」是 **P6-6 的硬前置** |
| **P6-7** | `mtval` 真值（收敛「恒 0」） | P6-3 | ⚠️ **不是「加一根线」**：`mtval` 是 **cause 相关**的 ⇒ ⇒ **cause × 手推表**：illegal ⇒ **触发指令字** · misaligned ⇒ 地址 · ECALL/EBREAK ⇒ **0** |

### P6-3 的三条**具名不变量**（取代「重新论证」这种不可执行的动词）

1. **`pc_update_en` 新表达式**（含 `trap_redirect`）⇒ ⇒ 优先级表见下「同拍仲裁」；
2. `trap` ⇒ **IF / ID 均被杀**，**EX / MEM / WB 不受影响**（在途的更老指令继续退休）；
3. ⚠️ **`trap_flush` 也 MUST 有 `trap_flush_d1`**（imem 是 1 拍 `sread` ⇒ ⇒ **多杀一拍** ⇒ ⇒ 与
   `if_stage.h` 的 `flush_d1` **同构**）· **`skid` MUST 在 `trap_flush | trap_flush_d1` 拍清空**
   （ADR-048 D5 的同构要求）。

### 同拍仲裁表（**MUST 逐行写死** · 5 种同时组合）

| `branch_redirect`（taken） | `trap` | `mret` | 谁赢 |
|---|---|---|---|
| 1 | 0 | 0 | branch（现状） |
| 0 | 1 | 0 | trap |
| 0 | 0 | 1 | mret |
| 1 | 1 | 0 | ⚠️ **待裁决**（默认 = **trap**：陷阱优先于错误路径上的分支） |
| 0 | 1 | 1 | ⚠️ **不发生**（`mret` 与 trap 语义互斥）⇒ ⇒ `MUST NOT` 断言它 |

⚠️ **正确性条款（承重）**：`exception_valid` MUST = `illegal_active & id_valid & !flush`
⇒ ⇒ **`id_stage.h` 的门控现在只有 `valid`、不含 `flush`** ⇒ ⇒ taken 分支的 fall-through 槽里的非法字
（ADR-071 §2.3 记录的**可达轨迹**）会让 `ExceptionUnit` 在 `flush` 同拍 `start_trap`
⇒ ⇒ **用错误路径的 PC 写坏 `mepc`** ⇒ ⇒ 判据 = 「taken 分支 fall-through 放非法字 ⇒ `mepc` / `mcause` **不变**」。

---

## Acceptance（含 **接缝的兑现点**）

- [ ] **P6-2..P6-7 逐项**：每项一个独立 change + spec delta 进主 spec（**禁** `--skip-specs`）
- [ ] ⚠️ **接缝兑现 #1**：接线 change **MUST 以 MODIFIED delta 修订**
      `rv32i-isa-semantics` 的「未知 opcode ⇒ **流水线级冻结**」Requirement
      （真 trap 落地后处置 MUST 从**冻结**改为 **trap** ⇒ ⇒ **两套机制不得同时生效**）
- [ ] ⚠️ **接缝兑现 #2（D-3 · **逐项**由关闭它的那一步修订）**：
      `#2 mtvec 取值语义` → **P6-3** · `#4 MRET 恢复` → **P6-5** ·
      `#3 mstatus 陷阱侧更新` + `#5 并发异常排队` → **P6-6（硬前置）** · `#1 mtval` → **P6-7** ·
      **`#6（新）` 未实现/只读 CSR 的可写性语义** ⇒ ⇒ **本版仍排除**（RISC-V 要求 illegal，但本版不 trap）
- [ ] **可恢复性可观测**：顶层新增 `obs_mepc` / `obs_mcause` / `dbg_trap_count`
      （否则 trap 后 `illegal_reason` **回落 0** ⇒ ⇒ 「发生过 trap」在 SoC 层**零信号**）
      ⇒ ⇒ 同步 `SOC_TOP_PORTS` **快照 + 自检 fixture**（fixture MUST **从快照生成** ⇒ 否则 R24 重演）
- [ ] **退休数 / 时序 golden（**点名三处载体 + 形态**）**：
      · `test_rv32i_soc_dual_parity.cpp` 的 `UnknownOpcodeFreezes` 场景表 = **语义反转**
        （`kIdxIllegalReason` → 0 · 「ID 停在探针上」两条断言**反号** · 退休数不再是 8）
      · 同文件的 `REQUIRE(retire == kCycles - 24)` = **分段分解 MUST 扩展一个 trap 段**（重新逐段手推）
      · `test_verilator_complex_e2e.cpp` 的 `perf == 16`
      ⇒ ⇒ ⚠️ **golden 变更次数 MUST ≤ 2**（分批：**`P6-3 + P6-4` 同批** · **`P6-5 + P6-6` 同批**）
      ⇒ ⇒ 否则重演 ADR-071 的「G1 单飞改两次 golden」教训 ⇒ ⇒ **禁抄仿真输出**
- [ ] **复位一致性裁决**：`CsrBank` / `ExceptionUnit` **都没有 `rst` 端口**
      ⇒ ⇒ SoC 复位**不清 CSR**，且 `trap_phase` 残留（复位正好在 `phase=1` ⇒ 复位后**多写一次 `mcause`**）
      ⇒ ⇒ MUST 二选一：加 `rst`（= **23** 处测试驱动义务）**或** 显式登记「不受 SoC 复位」+ 一个「复位中途 trap」用例
- [ ] **单写口仲裁**：CSR 指令写（P6-2）× trap 两拍写（P6-1）**共用同一条** `csr_write_en`
      ⇒ ⇒ MUST 有仲裁规则 + 同拍冲突的可判用例
- [ ] **实例化点数量（精确）**：**23** 处，**全在测试**（`test_csr_unit.cpp` 11 + `test_interrupt_flow.cpp` 12）·
      **生产消费者 = 0** ⇒ ⇒ 新增端口 ⇒ **23 处逐点显式驱动**（未驱动读到**未定义值**）
- [ ] **每条新判据附一条变异**（至少）：`!flush` 门控删掉 ⇒ 转红 · `trap_flush_d1` 删掉 ⇒ 转红 ·
      `mcause` 改回同拍写 ⇒ 转红 · `start_trap` 门控删掉 ⇒ 转红
- [ ] **`rv32i-commit-path` MUST MODIFIED**（⚠️ 不是「可能」）：「trap 指令**自身不退休**、
      **更老的指令继续退休**」是**退休语义的契约变更**
- [ ] **计数锚点同步**：新增用例若改 CTest 条数 ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
      `DUAL_BACKEND_PARITY_*` 必须同步
- [ ] **中断与异常的优先级**：RISC-V 已有答案（**异常优先 · 中断只在指令边界采样**）
      ⇒ ⇒ MUST **直接裁决**（**不**留「未定」）

---

## Capabilities（预期新增 / 修改）

| capability | 变化 |
|---|---|
| `rv32i-privilege` | **扩展**：CSR 指令语义 · trap 入口（redirect + `mepc`/`mcause`/`mtval`）· `ECALL`/`EBREAK`/`MRET` · 中断 |
| `rv32i-isa-semantics` | **MODIFIED**：冻结 Requirement（接缝 #1） |
| `rv32i-commit-path` | **MUST MODIFIED**：退休语义（trap 自身不退休 / 更老指令继续退休） |

---

## Impact（影响面）

- `include/cpu/pipeline/**`：`if_stage`（redirect 输入 + `trap_flush_d1` + skid 清空）·
  `id_stage`（`illegal_reason` → `mcause` 映射 + `!flush` 门控）· `rv32i_pipeline.h`（trap 接线）·
  `rv32i_decoder.h`（`OP_SYSTEM` 的 `funct3` 译码 + `funct3=4` → `reason 3`）
- `include/cpu/riscv/**`：`rv32i_csr.h`（第二写口 / `rst`）· `rv32i_exception.h`（`mtval` 的 cause 相关数据源）·
  `clint.h`（`mtip` 接线）
- **SoC 顶层**：新增 `obs_mepc` / `obs_mcause` / `dbg_trap_count` ⇒ ⇒ `SOC_TOP_PORTS` 快照 + 独立 TB 同步
- **既有 golden**：**退休数会变**（ECALL/EBREAK/非法指令不再「冻结」而是「trap 后继续」）
  ⇒ ⇒ **必须**手推重算 + 分批 ≤ 2 次（见 Acceptance）

---

## 风险与已知边界（**来自 R7 侦察 + metis**）

| # | 风险 | 依据 |
|---|---|---|
| R1 | **`CsrBank` 单写口** ⇒ trap 同拍要写两个 CSR ⇒ 只能跨拍或加第二写口（**公共 API 变化**）⇒ ⇒ ⚠️ 实测 **23** 处实例化点，**全在测试**（生产消费者 0）⇒ ⇒ 交付风险**低**、测试改动**大** | `rv32i_csr.h` 写路径 |
| R2 | **第二路 redirect 输入**会打破 ADR-048 的「flush 唯一来源」⇒ ⇒ 见 P6-3 的三条具名不变量 | ADR-048 · `if_stage.h` |
| R3 | **ADR-071 的冻结与 trap 并存** ⇒ ⇒ 接线 change MUST 切换（接缝 #1） | ADR-071 §2.3 |
| R4 | `mepc` 的写入时机与 `hazard.flush` 的交互 ⇒ ⇒ **必须** `!flush` 门控（见 P6-3 正确性条款） | `id_stage.h` 的门控 |
| R5 | 中断与异常共用 trap 入口 ⇒ ⇒ 优先级**必须显式**（RISC-V：异常优先） | — |
| R6 | ⚠️ **`mtip` 比 CLINT 晚一拍**（`mip` 是从 `mtip` **沿寄存**的）⇒ ⇒ 期望值 MUST **逐拍手推** | `rv32i_csr.h` |
| R7 | ⚠️ **`mtvec` 的 mode 位**：`ExceptionUnit` 恒取 **Direct**，但 `mtvec` 默认值低 2 位 = 00（注释却写 `MODE=01`）⇒ ⇒ **本版 MUST 钉住 = Direct** | `rv32i_exception.h` |

## 表述精度（metis Priority C · **已采纳，不阻塞**）

- 「逐拍等价」的**观测集** = `{obs_mepc, obs_mcause, dbg_trap_count, 既有 9 项}`（否则对 trap 状态**结构性失明**）。
- `mtval` 判据 = **cause × 手推表**（不是「加一根线」）。
- **第 6 项排除** = 未实现 / 只读 CSR 的可写性语义（本版**不** trap）。
- 依赖行补 **ADR-047**（CSR RAW）与 **ADR-049**（`mtime` 逐拍）。
