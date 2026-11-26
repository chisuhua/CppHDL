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

- [x] **T1.1** ✅ `proposal.md`（Why / What Changes / **IN·OUT 逐条** / **AC1–AC12** / Capabilities）
- [x] **T1.2** ✅ `design.md`（载体与拓扑 / 三条具名不变量 / **同拍仲裁表** / **写口仲裁** /
      判据设计 / **ADR dated addendum 草案** / D-1 事实订正）
- [x] **T1.3** ✅ `tasks.md`（本文件）+ **spec delta**（`rv32i-privilege` **ADDED**）
- [x] **T1.4** ✅ `openspec validate` **valid**（零 WARNING）· `verify_openspec_delta.sh` **4/4 绿**

## T2 · **开工前置**（**architect 域** · builder **不写** ADR）

- [x] **T2.1** ✅ **ADR-048 的 dated addendum 已落档**（**DA1–DA5 五条** · 2026-11-26 · architect）
      ⇒ ⇒ 载体 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` **§8（dated addendum v1.1）**
      ⇒ ⇒ 连带 `docs/adr/ADR-071-...md` 的 **v1.3** 标注（D3 部分取代 + D3-b 公式扩展）
      ⇒ ⇒ **审稿链**：oracle（**REJECT** · 5 条 finding 全处置）→ critic（**APPROVE_WITH_CONDITIONS** ·
      **0 BLOCKER** · 2 SHOULD_FIX + 2 NIT 全处置）⇒ ⇒ 报告 =
      `.rddf/reviews/ADR-048-pc-redirect-second-path-review-2026-11.md`
      ⇒ ⇒ ⚠️ **零 BLOCKER ⇒ 未动用复审额度**
- [x] **T2.3** ✅ **architect 复核后的同步修订（B1–B7）已做完**（2026-11-26 · builder）：
      ① 触发集订正为 `{2,3}`（`proposal` S2 · `design` §2.1 · `tasks` T4.3 · 母 improvement 正确性条款）
      ② `rv32i-privilege` delta 的 Requirement 头删去「`funct3 = 0`」（原与其末条 Scenario 打架）
      ③ **新增** `rv32i-isa-semantics` 的 **MODIFIED delta**（两条 Requirement · 既有内容一条不删）
      ④ `design` §4.2 订正：CSR 写口的**冲突窗口 = 拍 T 本身**（`ExceptionUnit` 同拍就拉高写使能）
      ⑤ `design` §7.1 改为**指针**到 ADR §8（避免两份漂移）
      ⑥ 根 `AGENTS.md` 的 RV32I 红线改写（`{2,3}` ⇒ 陷入 · `1` ⇒ 仍冻结）
      ⑦ 本文件 T2.1 / T3 / T5 / AC12 的同步（见下）
      ⇒ ⇒ 依据 = 审稿报告 §4 的 **B1–B8** 清单

- [ ] **T2.2** ⚠️ **D-1 的裁决**（用户 / architect 域）：`mtvec` 取值语义（base / 模式位 /
      复位值是否合理）⇒ ⇒ **不阻塞本项**（本项只把 `mtvec` 的**当前值**当输入）⇒ ⇒
      但**阻塞 P6-4** 的固件级恢复验证

## T3 · `if_stage` 的**第二路**重定向

- [x] **T3.1** ✅ `if_stage.h`：`__io` **+2 输入端口**（`trap_target` / `trap_flush`）
      ⇒ ⇒ ⚠️ **追加在输入端口组末尾**（端口按声明顺序编号 ⇒ 插中间会位移）
- [x] **T3.2** ✅ `trap_flush_d1`（**逐字同构** `flush_d1`）
- [x] **T3.3** ✅ `next_pc_target` 三路（**trap > branch > PC+4**）+ `pc_update_en` 含 `trap_redirect`
      ⇒ ⇒ **trap 与 flush 同权**（都压过 stall）
- [x] **T3.4** ⚠️ **ADR-048 §8 DA3 的四个 MUST 站点**（**第 4 个是承重件** · oracle F1）：
      ① `if_id_update_en` 的门 ② `skid_valid` 的**清空** ③ `skid` 的**填充排除**
      ④ ⚠️ **`if_id_valid_reg` 的清零条件**（**内层** select · 现为 `io().flush | flush_d1 | !primed`）
      ⇒ ⇒ 漏 ④ ⇒ ⇒ `valid` 永不清零 ⇒ ⇒ trap **每拍重触发** ⇒ ⇒ **活锁**
      ⇒ ⇒ ⚠️ **skid MUST 清空**（ADR-048 D5 同构 · 且填充条件 MUST **排除**）
      ⇒ ⇒ ⚠️ **`if_stage.h` 的「flush 唯一来源」注释块 MUST 重写**（否则代码注释与 ADR 漂移）
      ⇒ ⇒ 依据 = ADR-048 §8.2 **DA3**

## T4 · `pipeline` 的接线 + **写口仲裁**

- [x] **T4.1** ✅ 实例化 `ExceptionUnit`（**inline** `ch_module` ⇒ ⇒ 层次化 module 数**不变**）
- [x] **T4.2** ✅ `ExceptionUnit` 的 CSR 读输入 ← `csr_bank` 的**输出端口**（零端口变更）
      ⇒ ⇒ ⚠️ MUST 读**输出端口**侧（C1：读子组件输入 = 跨模块边 ⇒ 层次化发射失败）
- [x] **T4.3** ✅ 触发点：**`exception_valid = (illegal_reason == 2 | illegal_reason == 3) & id_valid & !flush`**
      ⇒ ⇒ ⚠️ **触发集 = `{2,3}`**（**不是** `illegal_active` = `!= 0`）⇒ ⇒ `reason = 1` 维持冻结
      ⇒ ⇒ `illegal_active`（`!= 0`）**继续只服务 stall**
      ⇒ ⇒ ⚠️ **`!flush` 是承重条款**（AC5）
- [x] **T4.4** ✅ `current_pc` ← **`if_stage.io().pc`**（**ID 级**那条指令的 PC ⇒ ⇒ 不是 `pc_out`）
- [x] **T4.5** ✅ `exception_cause` = **2**（illegal instruction · RISC-V 规范）⇒ ⇒ **常量**
- [x] **T4.6** ✅ **写口仲裁**：`ExceptionUnit` **优先**（design §4.2）⇒ ⇒ 父层 mux 出
      `csr_addr` / `csr_wdata` / `csr_write_en` 三者
- [x] **T4.7** ✅ `mret_instruction` 接 **0**（P6-5）⇒ ⇒ `mret_valid` **被消费**（不进 PC mux 会冒泡）

## T5 · 判据（**逐条手推** + **每条一条变异**）

> ⚠️ **期望值 MUST 手推**；⚠️ **观测 MUST 零新增端口**（读 `pipeline.csr_bank.instance().io().mepc_value`）

- [x] **T5.1** ✅ 新建 `examples/riscv-mini/tests/test_rv32i_trap_entry.cpp`
      ⇒ ⇒ ⚠️ **实现期订正：DUT = 核级（`rv32i_core_top.h`）而不是直接实例化 `Rv32iPipeline`**
      ⇒ ⇒ **原因（实测）**：imem 是 **1 拍 `sread`** ⇒ ⇒ 把镜像**组合**喂给流水线的
      `instr_data` ⇒ ⇒ 取指与 PC **错开一个字** ⇒ ⇒ 译码到的指令与 `dbg_id_pc` 不对应
      ⇒ ⇒ 判据全红且**测的不是产品形态**（真 `sread` 由核级 I-TCM 提供）
      ⇒ ⇒ 5 用例：AC1（`mepc`/`mcause`）· AC2（不退休 + IF/ID 被杀）· AC3（PC 重定向）·
      AC4（在途更老指令照常退休）· AC5（`flush` 优先）· AC6（连续两拍 ID 级空）·
      AC12（`reason = 1` 仍冻结）· `reason = 3` ⇒ 陷入（触发集的另一半）
- [x] **T5.1b** ✅ **组件级用例 6 个**（**Batch 5**）⇒ ⇒ 宿主 =
      **`tests/test_rv32i_pipeline_behavior.cpp`**（**既有 target ⇒ `CTEST_TOTAL` 不变**；
      `DUAL_BACKEND_PARITY_PAIRS` **291 → 297**）
      ⇒ ⇒ **5-a** trap 优先于 branch（V1' · DA1）· **5-b** branch 仍优先于 stall（ADR-071 未破）·
      **5-c** `stall ∧ flush ∧ ¬branch_valid` ⇒ PC 冻结（DA2 的既有契约）·
      **5-d** trap 清 IF/ID 的 `valid`（V3 · 抓活锁）· **5-e** trap 多杀一拍（V3 · `trap_flush_d1`）·
      **5-f** trap 清 skid **且**不填 skid（V4 · DA3 ②③ 两站点）
      ⇒ ⇒ ⚠️ **夹具已扩展**：`if_tick` 原先**只驱动 4 个输入** ⇒ ⇒ 现**每拍驱动全部 9 个**
      （`instr_ready` / `branch_valid` / `branch_target` / `trap_flush` / `trap_target`）
      ⇒ ⇒ 依据 = ADR-048 §8.4 代价 ⑤ + §8.5（**D9.4**：流水线级不可达 ⇒ 判别力落组件级）
      ⇒ ⇒ ⚠️ **实测订正**：DA3 的 ②③ 两站点**互相掩护** ⇒ ⇒ 排程 MUST 让 skid 在 trap 拍
      **为空**（否则「填充排除」被 `!skid_valid` 挡住 ⇒ 变异不可观测）
- [x] **T5.2** ✅ **每条判据一条变异**（还原错误实现 ⇒ 转红 · **全部仍能编译**）
      ⇒ ⇒ 逐条登记（**11 条，全部实测**）：
      | # | 变异 | 期望转红 | 结果 |
      |---|---|---|---|
      | M1 | 触发条件去掉 `!flush` | AC5（flush 优先） | ✅ 红 |
      | M2 | `if_stage.trap_flush` 恒 0 | AC1/AC3（重定向） | ✅ 红（3 用例） |
      | M3 | `current_pc` 恒 0 | AC1（`mepc`） | ✅ 红（3 用例） |
      | M4 | `exception_cause` 恒 0 | AC1（`mcause`） | ✅ 红（3 用例） |
      | M5 | 触发集放宽到 `!= 0` | AC12（`reason = 1`） | ✅ 红 |
      | M6 | `exmem_en` 再门控 `!trap_trigger`（trap 杀整条流水线） | AC4（在途指令退休） | ✅ 红 |
      | M7 | kill 站点 ④ 去掉 `trap_flush` | AC1（活锁）+ Batch 5-d | ✅ 红（双侧） |
      | M8 | kill 站点 ① 去掉 `trap_flush_d1` | Batch 5-e | ✅ 红（组件级） |
      | M9 | kill 站点 ③ 去掉 `trap_flush \| trap_flush_d1` | Batch 5-f | ✅ 红 |
      | M9b | kill 站点 ② 去掉 `trap_flush \| trap_flush_d1` | Batch 5-f | ✅ 红 |
      | M11 | 未知 opcode 的 reason 2 → 1 | AC1（触发集） | ✅ 红（2 用例） |
      ⇒ ⇒ ⚠️ **如实登记**：**M8 在核级不红**（组件级才红）⇒ ⇒ 理由 = `trap_flush_d1` 若漏，
      **pc↔指令的配对**会错一个字，但 `mepc`（触发指令的 PC）仍对、handler 仍执行
      ⇒ ⇒ 核级的观测面**看不到**配对错误（本核没有「读当前 PC」的 CSR）⇒ ⇒
      判别力由 **Batch 5-e** 承担（ADR-048 §8 D9.4 的定位）
- [x] **T5.3** ✅ **固件级**判据已并入 `test_rv32i_trap_entry.cpp`：**别名落点 word `0x040`**
      放处理器（`csrrs` 读回 `mepc` / `mcause` + 自环停机）⇒ ⇒ 5 个用例**全部**经该落点
      验「别名跳转可达 + handler 真的执行」
- [x] **T5.4** ✅ 注册 CTest ⇒ ⇒ `CTEST_TOTAL` **283 → 284** · `EXAMPLES_TEST_SOURCES`
      **25 → 26** · `DUAL_BACKEND_PARITY_PAIRS` **291 → 297**（锚点**同批**同步）
- [x] **T5.5** ✅ **全量回归**：全量重建（0 error）⇒ ⇒ `ctest -LE perf` **273/273 通过
      （0 失败）** ⇒ ⇒ ⚠️ 首次回归红过 3 项 —— **全部**是本项语义变更的**合法影响**
      （爆炸半径见 `design.md` §8.4 与验收报告 §6）⇒ 订正后复跑全绿

## T6 · 产物面 / 契约 / 归档

- [x] **T6.1** ✅ **零端口面实测**（AC8）：`verify_riscv_soc_core_ports.sh` **PASS**
      （V1 逐字逐序 53 · V2 = 53 · V3 C6 快照 **51**）· `verify_no_duplicate_rv32i_headers.sh`
      的 **C6** PASS（51 端口集合不变）⇒ ⇒ `ExceptionUnit` inline ⇒ **不新增层**
- [x] **T6.2** ✅ **spec delta**：`rv32i-privilege` **ADDED** + `rv32i-isa-semantics`
      **MODIFIED**（**禁** `--skip-specs`）⇒ `openspec validate` **valid** ·
      `verify_openspec_delta.sh` **4/4** ⇒ ⇒
      **既有 Requirement 不动** ⇒ ⇒ ⚠️ 同时**逐项**修订 capability 的「不覆盖」清单
      （**D-3 的接缝兑现**：本项关闭「`#2 mtvec 取值语义`」的**机制部分**）
- [x] **T6.3** ✅ 文档同步：`include/cpu/AGENTS.md`（**新增**「trap 入口接线」段 + 行数锚点
      **7384 → 7507**）· 根 `AGENTS.md`（3 个计数锚点）· `examples/riscv-mini/AGENTS.md`
      （target 24 → 26 · test source 22 → 24）
- [x] **T6.4** ✅ **归档后手工更新** `openspec/specs/rv32i-privilege/spec.md` 的 `## Purpose`
      （两段 → **三段**：trap 触发与重定向 / 上下文写入机制 / CSR 指令语义）
      ⇒ ⇒ ⚠️ **另手工订正 8 处合并残留 + 过时旧文**（`openspec archive` 的 MODIFIED 是
      **追加**语义，不删旧文）⇒ ⇒ 详见验收报告 §7 · `openspec validate --specs` **54 passed**
- [x] **T6.5** ✅ 验收报告：`.rddf/reviews/rv32i-trap-entry-2026-11-verify.md`
- [x] **T6.6** ✅ 回写 roadmap：`phase-6.md` 的 **P6 行**（🟡 已立项 → **✅ 已闭**）+
      `phase-6-next-steps.md` §9.1（实施与收口）+ 母 improvement 的 `进度` 行（✅ P6-3 已闭）

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
