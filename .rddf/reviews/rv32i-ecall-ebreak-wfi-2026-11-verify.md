# 验收报告 · `rv32i-ecall-ebreak-wfi-2026-11`（RV32I `ECALL`/`EBREAK`/`WFI` + `mtvec`/`mepc` WARL · P6-4）

> **判据来源**（按优先级）：
> 1. `openspec/changes/rv32i-ecall-ebreak-wfi-2026-11/proposal.md` 的 `## 验收标准`（**AC1..AC12**）
> 2. `design.md`（§1 裁决表 / §2 实现 / §3 判据设计 / §8 变异矩阵）+ `tasks.md`（T0–T5）
> 3. capability delta（`rv32i-privilege` **MODIFIED ×2 + ADDED ×1**）
> 4. 裁决单 `.rddf/checks/decision-request-rv32i-privileged-d1-d2-2026-11.md`（D-1 = A · D-2 = 全部按推荐）
> 5. 规范依据 = Privileged Spec **v20260120**（§2.1.1.7 / §2.1.1.14 / §2.1.1.15 / §2.1.3.1 / §2.1.3.3）
>
> **判定纪律**：每条判据 MUST 有**工具证据**；期望值**手推**（禁抄仿真输出 —— 解释器与 codegen
> 同源 ⇒ 双后端一致地错 ⇒ parity 对该族缺陷结构性失明）。
> **已用轮次**：**2**（轮 1 = 首次验收 ⇒ **FAIL**（1 BLOCKER + 3 SHOULD_FIX）· 轮 2 = 复验 ⇒ **PASS**）。
> **分流**：`rddf rdd-verify --dry-run` ⇒ 「No eligible changes to verify (empty queue)」
> ⇒ ⇒ **未命中** skill 分流三条件（条件 2 = P3 archive gate 未触发 · 权威口径 `discover_eligible()`）
> ⇒ ⇒ 走**本地验收**（判据来源 = 上表第 1 项）。
>
> **最终结论**：✅ **PASS**（轮 2）⇒ ⇒ **归档闸门已开** ⇒ ⇒ T5.4 归档在轮 2 之后执行
> （归档的**残留订正**见 §9 · 归档后复核见 §10）。
> **轮 1 的 FAIL 与其闭合证据**完整保留在 §2 / §3（不删改历史）。

---

## 1. 逐条判定

### AC1 · `ECALL` ⇒ 核级陷入（`mepc` = 自身 PC · `mcause` = 11 · `reason` 恒 0 · 不退休）✅ PASS

- **代码证据**（手读，非仅凭测试）：
  - `include/cpu/pipeline/rv32i_decoder.h`：`is_ecall = sys_f3_zero & (sys_imm == 0x000)`
    ⇒ 归入 `sys_f3z_legal` ⇒ `legal_opcode` ⇒ `illegal_reason = 0`（合法）
    ⇒ `io().sys_trap_kind = select(is_ecall, 1, …)`；
  - `include/cpu/pipeline/rv32i_pipeline.h`：`trap_trigger` 追加 `id_is_ecall | id_is_ebreak`
    ⇒ `exception_cause` mux ⇒ `select(id_is_ecall, 11_d, …)`；
  - `include/cpu/pipeline/stages/id_stage.h`：`sys_trap_kind` **透传 + `valid` 门控**（无效拍 ⇒ 0）
    ⇒ ⇒ 不会产生「幽灵 ECALL」。
- **工具证据**：`./build/examples/riscv-mini/test_rv32i_trap_entry` ⇒ **11 用例 / 1196 断言全过**（exit 0）。
  该用例断言写回序列**恰好** `{1, 0x80000000}`（`mepc` = ECALL 自身）· `{2, 11}`（`mcause` = from **M**）·
  全程 `reason == 0` · ECALL 在 ID 级那一拍 `retired == 0`。
- **判别力**：本报告 §4 的 **V2**（`mcause` 11 → **8**，即 design.md §6 明列的错法）⇒ 该用例**转红** ✔

### AC2 · `EBREAK` ⇒ 核级陷入（`mepc` = 自身 PC · `mcause` = 3）✅ PASS

- **代码证据**：`is_ebreak = sys_f3_zero & (sys_imm == 0x001)` ⇒ 合法（`reason = 0`）+ `sys_trap_kind = 2`
  ⇒ pipeline mux ⇒ `mcause = 3`（breakpoint · 异常侧 `bit31 = 0`）。
- **工具证据**：同一二进制（`test_rv32i_trap_entry`）的「TRAP - EBREAK 陷入：mcause = 3（breakpoint）」
  用例 ⇒ 写回序列**恰好** `{1, 0x80000000}` · `{2, 3}` · 全程 `reason == 0`。
- **判别力**：本报告 §4 的 **V6**（**decoder 侧**把 EBREAK 的 `sys_trap_kind` 由 2 改成 1
  ⇒ ⇒ `mcause` 变 11）⇒ **仅**该用例转红（10 passed / 1 failed）✔
  ⇒ ⚠️ 该变异点与 builder 的 **M3**（pipeline 侧原因码 mux）**不同** ⇒ 两条独立证据。

### AC3 · `SRET`（`imm = 0x102`）⇒ `reason = 3` ⇒ 陷入 · `mcause = 2`（**MUST NOT** reason 1）✅ PASS

- **代码证据**：`is_mret` 只认 `0x302` ⇒ `0x102` 落 `sys_f3z_illegal` ⇒ `reason = 3`
  ⇒ `trap_trigger` 命中 ⇒ `mcause = 2`（illegal instruction）⇒ **不**落「冻结」桶。
- **工具证据**：「TRAP - SRET（0x102）不可编码 ⇒ reason 3 ⇒ 陷入（mcause = 2）」⇒
  写回序列 `{1, 0x80000000}` · `{2, 2}` + `CHECK(s.reason != 1u)`（逐拍）+ `CHECK(saw_reason3)`。
- **判别力**：本报告 §4 的 **V4**（把 `0x102` 从 `sys_f3z_illegal` 里摘掉 ⇒ 落 `reason = 2`）
  ⇒ 该用例**转红**（`saw_reason3` 断言）✔ ⇒ ⚠️ 与 builder 的 **M4**（改成 reason 1）**不同**。

### AC4 · `WFI`（`imm = 0x105`）⇒ **合法 NOP**（`reason = 0` · 不 trap · 指令流继续）✅ PASS

- **代码证据**：`is_wfi` ∈ `sys_f3z_legal` ⇒ `reason = 0` · `sys_trap_kind = 0` ⇒
  既不进触发集、也不进 `illegal_active` ⇒ **不装气泡、不 trap** ⇒ 正常退休。
- **工具证据**：「TRAP - WFI（0x105）⇒ 合法 NOP：不 trap、指令流继续」⇒
  写回序列**恰好** `{1, 0}` · `{2, 0}`（`mepc`/`mcause` **保持复位值** ⇒ 无 trap）·
  全程 `reason == 0` · `out.samples.back().retired > 0`（指令流确实前进）。
- **判别力**：本报告 §4 的 **V3**（把 `is_wfi` 从 `sys_f3z_legal` 摘掉 ⇒ `WFI` 落 `reason = 3` ⇒ 陷入）
  ⇒ 该用例**转红** ✔ ⇒ ⚠️ 与 builder 的 **M5**（改成 reason 1 = 冻结）**不同**。

### AC5 · `funct3 = 0` 的**未定义 `imm`** ⇒ `reason = 3` ⇒ 陷入 · `mcause = 2` ✅ PASS

- **代码证据**：`sys_f3z_illegal = sys_f3_zero & !(sys_f3z_legal | is_mret)` ⇒ 任何未定义 `imm`
  （`0x002` / `0x102` / …）⇒ `reason = 3`。
- **工具证据**：「TRAP - SYSTEM funct3=0 的未定义 imm ⇒ reason 3 ⇒ 陷入」⇒
  写回序列 `{1, 0x80000000}` · `{2, 2}` + `CHECK(saw_reason3)`。
- **判别力**：builder 的 **M11**（未定义 `imm` 不归 reason 3）⇒ 转红 ✔；
  本报告 **V4** 在**相邻编码**上给出同族独立证据（见 AC3）。

### AC6 · `MRET`（`imm = 0x302`）⇒ **仍 `reason = 1`**（冻结 · 不 trap · 零写回）✅ PASS

- **代码证据**：`is_mret` ⇒ `sys_known_unimpl = is_mret` ⇒ `illegal_reason` 优先级链最前 ⇒ `reason = 1`
  ⇒ 既不在触发集（`{2,3}` ∪ `kind≠0`）也不在 `sys_f3z_legal` ⇒ **冻结**（不退休）。
- **工具证据**：「TRAP - MRET（0x302）P6-5 前仍 reason=1 冻结」⇒
  `REQUIRE(out.writebacks.empty())` + 逐拍 `retired == 0` + 逐拍 `reason != 2 / != 3` + `saw_reason1`。
- **判别力**：builder 的 **M12**（`MRET` 不归 reason 1）⇒ 转红 ✔；
  ⚠️ 同时 **V3/V4** 证明 `sys_f3z_legal` / `sys_f3z_illegal` 的边界**没有**误伤 `0x302`。

### AC7 · `mtvec.MODE` **WARL = {0}**（写 `…01` / `…11` ⇒ 读回 `…00`）✅ PASS

- **代码证据**：`include/cpu/riscv/rv32i_csr.h` 的 `mtvec->next` 写路径
  `io().csr_wdata & ch_uint<32>(0xFFFFFFFCu)` ⇒ `[1:0]` 恒掩零 ⇒ 读回恒 Direct。
- **工具证据**：`./build/examples/riscv-mini/test_csr_unit`（**15 用例 / 60 断言**全过）里的
  「CSR WARL coercion: mtvec MODE and mepc IALIGN=32」⇒ 写 `0x80000103` ⇒ 读 `0x80000100` ·
  写 `0x80000101` ⇒ 读 `0x80000100`；
  另 `tests/test_rv32i_arch_dual.cpp` 的拍表新增行 `{0x305, 0x80000103, 1, …, 0x80000100, …}`（**双后端**）。
- **判别力**：builder 的 **M7**（去掉 `mtvec` 写掩码）⇒ 转红 ✔。

### AC8 · `mepc[1:0]` 恒 0（IALIGN = 32）✅ PASS

- **代码证据**：`mepc->next` 写路径 `& 0xFFFFFFFCu` ⇒ **两位**掩零（design.md §2.4 明令
  「MUST 是 `0xFFFF_FFFC` · **MUST NOT** 用 `~0x1`」）。
- **工具证据**：`test_csr_unit` 的 WARL 用例 ⇒ 写 `0x1237` ⇒ 读 `0x1234` · 写 `0x123F` ⇒ 读 `0x123C`；
  `test_rv32i_arch_dual.cpp` 拍表 `{0x341, 0x00001237, 1, …, 0x00001234, …}`（**双后端**）。
- **判别力**：本报告 §4 的 **V1**（掩码由 `0xFFFFFFFC` 改成 **`0xFFFFFFFE`** = 只掩 bit0）
  ⇒ 实测读回 `0x1236` / `0x123E` ⇒ 该用例**转红** ✔
  ⇒ ⚠️ **V1 补上了 builder 的 M8（整体去掉掩码）覆盖不到的格**：
  M8 只判「有没有掩码」，**判不出**「掩的是 1 位还是 2 位」⇒ ⇒ design.md §2.4 的
  「MUST NOT 用 `~0x1`」这条**规范约束**此前**无判别力证据**，现由 V1 钉住。

### AC9 · `mtvec` 复位值 = `0x8000_0100`（I-TCM 区内）· 落点字索引仍为 `0x040` ✅ PASS

- **代码证据**：`rv32i_csr.h` 的 `ch_reg<ch_uint<32>> mtvec(0x80000100)`；`rv32i_mem_map.h` 的
  `kItcm{0x80000000ull, kTcmBytes}` ⇒ `0x8000_0100` **在** I-TCM 区间内（不再靠「取指丢高位」的偶然别名）。
- **字索引的机械核验**：`examples/riscv-mini/src/rv32i_core_top.h:204`
  `itcm.io().addr <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr)`
  且 `kTcmAddrWidth = 12` ⇒ `(0x8000_0100 >> 2) & 0xFFF` = **`0x040`** ⇒ ⇒ **落点不变** ✔
- **工具证据**：`test_csr_unit` 的「CSR reset values」⇒ `REQUIRE(mtvec_val == 0x80000100)`；
  `test_rv32i_arch_dual.cpp` 拍表 `{0x305, 0, 0, …, 0x80000100, …}`（**双后端**）；
  固件级判据（`test_rv32i_trap_entry` / `test_rv32i_soc_dual_parity` 的落点处理器放在
  **字 `0x040`**）在 P6-4 后**仍绿** ⇒ ⇒ 端到端证明「同一落点」。
- **判别力**：builder 的 **M10**（复位值回 `0x2000_0100`）⇒ 转红 ✔。

### AC10 · **零端口面变更**（53 端口 / C6 快照 51 / 独立 TB / tsv / 层次化 module 数 8）✅ PASS

- **工具证据**：
  - `bash scripts/verify_riscv_soc_core_ports.sh` ⇒ **PASS**（V1 = 53 行逐字逐序 · V2 = 53 · V3 = C6 **51** · V4/V5）；
  - `bash scripts/verify_no_duplicate_rv32i_headers.sh` 的 **C6** ⇒ **OK：端口集合与快照一致（51 个）**；
  - `bash scripts/verify_hierarchical_rtl.sh` ⇒ **PASS**（R1–R9，含 **R8′** = RV32I 产品语料顶层端口表 == 手推 **53** 根）；
  - `bash scripts/rv32i_rtl_sim_env.sh` ⇒ `[hier] module 数：8`（IfStage IdStage ExStage MemStage WbStage
    HazardUnit ForwardingMux_32_ top）· 顶层端口 **53** ⇒ 两种形态各 `checks=24 fails=0`；
  - `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:169` 的 `REQUIRE(module_count(t1) == 8)` ⇒ ctest 条目全量回归中**通过**。
- **机制核对**：`sys_trap_kind` 是**具名**输出，**追加在 `illegal_reason` 之后**（decoder / id_stage 各 1 个）
  ⇒ 既有 `decoder_unnamed_output_N` / `id_stage_unnamed_output_N` 的**编号不位移**
  ⇒ ⇒ 独立 Verilog TB 的端口表不错位。
- **零新增 `HazardUnit` 端口** ⇒ `include/cpu/pipeline/hazard_unit.h` **未出现在**本次改动面
  （`git diff --name-only 6b0bd23 HEAD` 核对）✔

### AC11 · 全量回归零失败 + 锚点无变化 ✅ PASS（轮 1 因 **B1** FAIL · 轮 2 闭合）

- **全量回归**：✅ **PASS** —— 全量重建（`cmake --build build -j$(nproc)`）**0 error**
  ⇒ `ctest --test-dir build -LE perf --output-on-failure -j$(nproc)`
  ⇒ **`100% tests passed, 0 tests failed out of 273`**（`Total Test time (real) = 1031.06 sec`）。
- **warning 口径**：全量重建的 468 条 warning **全部**来自**本次未改动**的既有头
  （`include/core/lnodeimpl.h` 的 `-Wswitch-enum` ×240 · `include/utils/logger.h` 的
  `-Wnonnull-compare` ×213 · `include/cpu/cache/d_cache_sync.h` 的 `-Wparentheses` ×12 ·
  `-Wcomment` ×1）⇒ ⇒ **本次改动面（5 个头 + 5 个测试源）零 warning** ✔
- **计数锚点**：✅ `verify_test_counts.sh` PASS（`CTEST_TOTAL` 284 = `ctest -N` 284 ·
  `VERILATOR_E2E_TOTAL` 81 = 81）· `verify_dual_backend_parity.sh` PASS（**67** 文件 / **297** pair 双锚点一致）·
  `EXAMPLES_TEST_SOURCES` 26 一致。
- ✅ **「行数锚点」**：轮 1 破了（自述 7600 ≠ 实测 **7601** ⇒ 护栏 **C5 红** ⇒ exit 1）⇒ **B1**
  ⇒ 轮 2 闭合后 `verify_no_duplicate_rv32i_headers.sh` ⇒ **exit 0**（C5 无 VIOLATION）。

### AC12 · 每条新判据配一条变异（须仍能编译）+ 逐条登记在 `design.md` §8 ✅ PASS

- **登记完整性**：`design.md` §8 登记 **M1–M12**（12 条），逐条含「变异 / 目标判据 / 期望 / 实测」
  ⇒ **覆盖映射完整**（AC1 → M1/M2 · AC2 → M3 · AC3 → M4 · AC4 → M5 · AC5 → M11 · AC6 → M12 ·
  AC7 → M7 · AC8 → M8 · AC9 → M10 · 触发集 → M6 · §2.3 气泡路径 → **M9**）⇒ **无判据漏配变异** ✔
- **独立复核**：本报告 §4 另施加 **6 条 builder 未用过的变异（V1–V6）** ⇒ **6/6 转红**
  （且每条都**编译通过**后才跑判据）⇒ ⇒ 判别力**不是**只靠 builder 自证。

---

## 2. ❌→✅ 失败项（BLOCKER · 轮 1）· 路由回 build · **轮 2 已闭合**

### **B1** · `include/cpu/AGENTS.md` 的**行数锚点** off-by-one ⇒ 护栏红 ⇒ T5.2 的声明不成立

- **现象**：`bash scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ **exit 1**，输出：

  ```
  --- C5: include/cpu/AGENTS.md MUST 与磁盘一致 ---
  VIOLATION include/cpu/AGENTS.md 自述行数 7600 ≠ 实测 7601（声明：30 headers, 7600 lines）
  VIOLATION include/cpu/AGENTS.md 自述行数 7600 ≠ 实测 7601（声明：30 headers, 7600 lines）
  ❌ verify_no_duplicate_rv32i_headers: 2 处违规
  ```

- **复现命令**：
  ```bash
  # 仓根执行
  bash scripts/verify_no_duplicate_rv32i_headers.sh ; echo "EXIT=$?"
  find include/cpu -name '*.h' -exec cat {} + | wc -l   # ⇒ 7601
  ```
- **根因**（实测定位）：本次改动把 `include/cpu/**` 的头文件总行数从 **7507** 推到 **7601**，
  但 `include/cpu/AGENTS.md` 两处自述写成 **7600**（差 1 行 ⇒ 在最后一次头文件改动之后测得旧值 / off-by-one）：
  - `include/cpu/AGENTS.md:7`（结构说明里的「**30 headers, 7600 lines**」）
  - `include/cpu/AGENTS.md:19`（目录树里的「`include/cpu/`  # 30 headers, 7600 lines」）
  - 同一数字也写进了 `.rddf/improvements/rv32i-privileged-csr-2026-11.md` §9.2 的
    「**锚点**：`include/cpu/AGENTS.md` 行数 **7507 → 7600**」。
- **对照（证明是本项引入、不是历史遗留）**：在**本项之前**的提交 `6b0bd23` 上实测
  `include/cpu` = **30 headers / 7507 lines**，且 `AGENTS.md` 自述同为 **7507** ⇒ 护栏**绿**
  ⇒ ⇒ **红是本项引入的**。
- **为什么是 BLOCKER（不是 SHOULD_FIX）**：
  ① `tasks.md` **T5.2** 明写「相关护栏 PASS：… `verify_no_duplicate_rv32i_headers` …」⇒ ⇒ **该声明在 HEAD 上不成立**；
  ② 护栏红 ⇒ **CI 红** ⇒ 不满足仓内 ZERO-DEBT 的「Green is the only acceptable state」；
  ③ 它就是「**改动面与磁盘不自洽**」的最后一处残留。
- **修法（机械，builder 侧执行）**：`include/cpu/AGENTS.md` 两处 `7600` → **`7601`**；
  `.rddf/improvements/rv32i-privileged-csr-2026-11.md` §9.2 的 `7507 → 7600` → `7507 → **7601**`；
  随后 `bash scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ 期望 **exit 0**。

### 路由

- **分类** = **`implementation_gap`**（实现/文档面缺口，不是提案漂移）⇒ ⇒ **回 build** 修 B1。
- **重试上限**：本 change 的验收已用 **1 / 3** 轮 ⇒ 还有 2 轮。
- **归档闸门**：用户指令「**验收通过后再 openspec archive**」⇒ ⇒ **本轮不执行 `openspec archive`**。

### ✅ 闭合证据（轮 2）

- **builder 实际修法**（逐路径 `git add`）：
  - `include/cpu/AGENTS.md`：**3 处** `7600` → `7601`
    （结构说明的「**30 headers, 7601 lines**」· 目录树的「`# 30 headers, 7601 lines`」·
    变更流水里的「**7507 → 7601 · 头文件数不变**」）；
  - `.rddf/roadmap/phase-6-next-steps.md:964` 的锚点行：**`7507 → 7600`** → **`7507 → 7601`**
    （⚠️ 该锚点行在 **roadmap** 文件的 §9.2 闭合记录里，**不在** improvement 文件
    ⇒ ⇒ **轮 1 的 S3 定位有误，此处订正**）；
  - `examples/riscv-mini/tests/test_rv32i_trap_entry.cpp` / `…test_rv32i_soc_dual_parity.cpp`：
    两处过期 `mtvec` 注释（**S2**）同步订正（见 §3）。
- **复核**：`bash scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ **exit 0**
  （C5 **无 VIOLATION** · C6 = 51 端口快照仍 OK）⇒ ⇒ **B1 闭合** ✔
- **旁证**：全量重建后 `ctest --test-dir build -LE perf -j$(nproc)`
  ⇒ **`100% tests passed, 0 tests failed out of 273`**（831.68 sec）· **16 个护栏全 PASS**（§5.2）。

### 路由（轮 2 已执行）

- **分类** = **`implementation_gap`**（实现/文档面缺口，不是提案漂移）⇒ ⇒ **回 build** 修 B1 ⇒ 已修。
- **重试额度**：本 change 的验收已用 **2 / 3** 轮（余 1 轮）。
- **归档闸门**：轮 1 ⇒ **不归档**；轮 2 ⇒ **PASS** ⇒ ⇒ 闸门**已开** ⇒ ⇒ T5.4 归档执行。

---

## 3. ⚠️ SHOULD_FIX（如实登记 · 不静默留尾 · 交 build 在归档步一并处理）

| # | 位置 | 事实 | 处置建议 |
|---|---|---|---|
| **S1** | `openspec/specs/rv32i-isa-semantics/spec.md`（**主 spec**） | P6-4 落地后，主 spec 仍有 **5 处**把 `ECALL`/`EBREAK`/`WFI` 列为 `illegal_reason = 1`（冻结）：L359-360（操作码分类说明）· L385-386（行为策略）· L396-400（Scenario「OP_SYSTEM ⇒ 分类为 **1**」）· L411-416（Scenario「已知未实现（reason=1）—— 仍是冻结」）· L497-499（Scenario「冻结是终态 —— 仅对 reason=1 成立」）⇒ ⇒ 与 P6-4 的实现（`ECALL`/`EBREAK`/`WFI` ⇒ `reason = 0`）**直接矛盾** | 本 change 的 delta **只**动 `rv32i-privilege`（`design.md` §7 只声明「**非法指令条款**不由本项修订」，**未**覆盖「`reason = 1` 的具体指令清单」）⇒ ⇒ 这 5 处属**「因语义变更而过时的旧文」**（不是合并残留）⇒ ⇒ 按 **P6-3 的先例**（`.rddf/reviews/rv32i-trap-entry-2026-11-verify.md` §7 的第 6/7/8 项）在归档步**直接订正主 spec**。⚠️ 同一 spec L357 自己写着「**禁只改实现不改 spec**」⇒ 本项**必须**在归档前收口 ⇒ ⇒ ✅ **轮 2 结论：在 T5.4 归档步闭合**（归档后**手工订正主 spec** · 逐条见 §9） |
| **S2** | `examples/riscv-mini/tests/test_rv32i_trap_entry.cpp:210-211` · `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:2537` | 注释仍写 trap 落点 `mtvec` = `0x2000_0100`（旧值）⇒ 事实已改为 `0x8000_0100` | **不影响断言**（字索引仍是 `0x040` ⇒ 落点不变）⇒ ⇒ 仅订正注释文字（两个文件都在本次改动面内 ⇒ 属本项残留）⇒ ✅ **轮 2 已闭合**（两处注释均改为 `0x8000_0100`，并注明「原 `0x2000_0100` 靠偶然别名落在同一 word」） |
| **S3** | `.rddf/roadmap/phase-6-next-steps.md:964`（§9.2 闭合记录的锚点行 · **不在** improvement 文件 ⇒ 轮 1 定位有误，此处订正） | 锚点行数写成 `7507 → 7600`（与 B1 同源 off-by-one） | 与 B1 同批订正 ⇒ ✅ **轮 2 已闭合**（`7507 → **7601**`） |
---

## 4. 变异登记

### 4.1 builder 的 M1–M12（登记于 `design.md` §8）

> builder 报告 **12/12 转红** 且每条**编译通过**。本轮**未重复施加**（预算：verifier 的独立复核优先用在
> builder 未覆盖的格上，见 §4.2）。

### 4.2 **独立变异 V1–V6**（verifier 自选 · **6/6 转红** · 每条**编译通过**后才跑判据）

> **选点原则**：刻意挑**builder 未用过的变异点 / 不同的错误取值**，避免「用 builder 的假设验 builder 的实现」。
> ⚠️ **纪律**：① 每条变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假绿 ——
> 本轮实测踩到一次：V4 的首版注入产生了语法错误 ⇒ 编译失败 ⇒ 旧二进制跑出「1196 断言全过」的**假绿** ⇒ 已改正后重测）；
> ② **还原源码 ≠ 复原**（二进制也是状态）⇒ ⇒ 每轮还原后**全量重建**并复跑判据。

| # | 变异（还原错误实现） | 站点 | 目标判据 | 实测结果 |
|---|---|---|---|---|
| **V1** | `mepc` 写掩码 `0xFFFFFFFC` → **`0xFFFFFFFE`**（只掩 bit0） | `rv32i_csr.h` | AC8（design.md §2.4「MUST NOT 用 `~0x1`」） | ✅ **红**（读回 `0x1236` / `0x123E` ≠ `0x1234` / `0x123C`）· **补上 M8 覆盖不到的格** |
| **V2** | `ECALL` 原因码 `11_d` → **`8_d`**（U-mode 码 · design.md §6 明列的错法） | `rv32i_pipeline.h` | AC1 | ✅ **红**（`mcause` 读回 8 ≠ 11）· **与 M1（改 2）不同** |
| **V3** | `is_wfi` 从 `sys_f3z_legal` 摘掉 ⇒ `WFI` 落 `reason = 3` | `rv32i_decoder.h` | AC4 | ✅ **红**（`WFI` 变成陷入 ⇒ `mepc`/`mcause` 不再是 0）· **与 M5（改成 reason 1）不同** |
| **V4** | `0x102`（SRET）从 `sys_f3z_illegal` 摘掉 ⇒ 落 `reason = 2` | `rv32i_decoder.h` | AC3 | ✅ **红**（`saw_reason3` 断言失败）· **与 M4（改成 reason 1）不同** |
| **V5** | `illegal_active` **不含** `sys_trap_kind` ⇒ 合法但陷入的指令照常退休 | `rv32i_pipeline.h` | **§2.3 气泡路径**（AC1/AC2 的「不退休」面） | ✅ **红**：SoC 级 `retire(20)` 实测 **16 ≠ 13** ⇒ **独立复现 design.md §2.3 的真缺陷形态**；⚠️ 同时**核级 11 用例 / 1196 断言仍全绿** ⇒ ⇒ **证实**「该缺陷在核级/组件级**结构性不可观测** · 只有 SoC 级退休计数能抓」这条 design 判断 |
| **V6** | `EBREAK` 的 `sys_trap_kind` `2` → **`1`** | `rv32i_decoder.h`（**decoder 侧**） | AC2 | ✅ **红**（`mcause` 变 11 ≠ 3）· **与 M3（pipeline 侧 mux）不同站点** |

> **还原复核**：6 条全部还原后 `git status --short` **为空**（工作树 = HEAD）·
> 全量重建 ⇒ `test_rv32i_trap_entry` **1196 断言 / 11 用例全过** ·
> `test_csr_unit` **60/15 全过** · `test_rv32i_csr_instructions` **31/8 全过** ·
> `test_rv32i_soc_dual_parity` **23213 断言 / 15 用例全过** ✔

---

## 5. 全量回归 + 守卫

### 5.1 全量回归（AC11）

| 项 | 结果 |
|---|---|
| 全量重建 | **0 error**；改动面**零 warning**（468 条 warning 全部来自**未改动**的既有头） |
| `ctest --test-dir build -LE perf --output-on-failure -j$(nproc)` | ✅ **100% tests passed, 0 tests failed out of 273**（1031.06 sec） |
| 关键条目 | `test_rv32i_trap_entry`（**11 用例 / 1196 断言**）· `test_csr_unit`（**15 / 60**）· `test_rv32i_csr_instructions`（**8 / 31**）· `test_rv32i_soc_dual_parity`（**15 / 23213**）· `test_rv32i_hier_rtl`（`module_count == 8`）· `test_rv32i_arch_dual`（双后端 WARL 拍表）· `test_rv32i_official_suite_g1..g4`（官方 `rv32ui-p-*` 40 个） |

### 5.2 守卫（`scripts/verify_*.sh`）

| 守卫 | 结果 |
|---|---|
| `verify_no_duplicate_rv32i_headers.sh` | ✅ **PASS**（轮 1 为 **RED**：C5 行数锚点 7600 ≠ 7601 ⇒ **B1** ⇒ 轮 2 闭合；**C6** = 51 端口快照 ✅）|
| `verify_riscv_soc_core_ports.sh` | ✅ PASS（53 / 51 / 无微架构副本） |
| `verify_hierarchical_rtl.sh` | ✅ PASS（R1–R9，含 R8′ = 53 根） |
| `verify_hierarchical_port_naming.sh` | ✅ PASS（N0–N3） |
| `verify_openspec_delta.sh rv32i-ecall-ebreak-wfi-2026-11` | ✅ **4/4 绿**（R1–R4） |
| `openspec validate rv32i-ecall-ebreak-wfi-2026-11` | ✅ **valid**（CLI 1.14.3 · 零 WARNING） |
| `verify_openspec_purpose.sh` | ✅ PASS（占位符 0 · 登记债 26 条完整） |
| `verify_test_counts.sh` | ✅ PASS（`CTEST_TOTAL` 284 · `VERILATOR_E2E_TOTAL` 81） |
| `verify_dual_backend_parity.sh` | ✅ PASS（67 文件 / 297 pair · 双锚点一致） |
| `verify_example_test_wiring.sh` | ✅ PASS（C1/C2/C3/C3b · 26 个 target 逐条 exit 0） |
| `verify_example_assertion_quality.sh` | ✅ PASS |
| `verify_public_api_visibility.sh` | ✅ PASS（R1–R6） |
| `verify_roadmap_fragments.sh` | ✅ PASS（AUTO 块守恒） |
| `verify_docs_links.sh` | ✅ PASS（L1 47 篇 / 192 链 / 177 引用） |
| `verify_input_port_additions.sh` | ✅ PASS（新增 `ch_in` = 0） |
| `verify_input_port_required_registry.sh` | ✅ PASS（17 条登记 · R6 rst_n 护栏仍绿） |
| `verify_agents_size.sh` | ✅ PASS（52100 B ≤ 72 KiB） |
| `scripts/rv32i_rtl_sim_env.sh` | ✅ PASS（flat / hier 各 `checks=24 fails=0` · hier 8 module / 53 端口） |

---

## 6. 遗留清扫（本次改动面）

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX / HACK | **无** ⇒ `grep -nE "TODO\|FIXME\|XXX\|HACK"` 在 5 个改动头 + 5 个测试源上**零命中** |
| 注释掉的代码 | **无**（`rv32i_decoder.h` / `rv32i_pipeline.h` 的注释块是**取代记录**，不是注释掉的实现） |
| 临时调试日志 / `printf` | **本项零新增** ⇒ `git show b9a933e -- <两个测试源> \| grep -E "^\+" \| grep -E "cout\|printf\|DEBUG\|SKIP"` **零命中**；存量 `std::cout "[PASS]"`（`test_csr_unit.cpp`）与 `RV32I_SOC_DEBUG` 门控 dump（`test_rv32i_soc_dual_parity.cpp`）**均为既有**，非本项引入 |
| 被 `SKIP` 的测试 | **无新增** |
| 未替换的 mock / 临时 fallback | **无**（核级 DUT 用**真** I-TCM `sread`） |
| 死代码 | **无新增**：`sys_trap_kind` 在 decoder → id_stage → pipeline **逐级被消费**（无冒泡）· `ExceptionUnit` 的 8 个 `ch_out` 仍全部被消费 · 本项**删除**了 `rv32i_exception.h` 的死常量 `MTVEC_MODE_MASK`（零消费者）⇒ 符合 ZERO-DEBT |
| 注释事实一致性 | ⚠️ **S2**（两处过期 `mtvec` 旧值注释）⇒ 登记，未静默留尾 |
| 过期注释的**语义影响** | **无**（两处注释里的**字索引**仍是 `0x040` ⇒ 落点不变 ⇒ 断言不受影响） |

---

## 7. 语义变更的爆炸半径（受影响的**既有**判据 · 全部已订正）

P6-4 把 `funct3 = 0` 的处置从「**整桶冻结**」改为「按 `imm[11:0]` 细分」⇒ ⇒ **4 处既有**判据的期望值过时
（**判据变更，不是行为回归**）：

| # | 文件 / 用例 | 旧期望 | 新期望 |
|---|---|---|---|
| 1 | `test_rv32i_csr_instructions.cpp` 的「funct3=0（ECALL）」 | `reason == 1` 且冻结（`retired == 0`） | 只保留**译码面**判据（`illegal_reasons` **空** · `retired > 0`）⇒ 「陷入」的完整语义归 `test_rv32i_trap_entry.cpp` |
| 2 | `test_csr_unit.cpp` 的「CSR reset values」 | `mtvec == 0x20000100` | `mtvec == 0x80000100`（**同一字索引 `0x040`** ⇒ 取指行为零变化） |
| 3 | `test_rv32i_soc_dual_parity.cpp` 的 `IllegalOpcodeObservableNotRetired + FenceIsNop` | ECALL 行 = `reason 1` + `retire = 8`（冻结） | ECALL 行 = `reason 0` + `retire = 13`（陷入）· **新增** MRET 行（`reason 1` · `retire = 8` · 冻结） |
| 4 | `test_rv32i_trap_entry.cpp` 的 AC12 用例 | 「reason=1（ECALL）仍冻结」 | 改为「ECALL ⇒ 陷入（`mcause = 11`）」+ **+6** 个 P6-4 用例 |

> ⚠️ **第 3 项的 `retire(20) = 13` 已被本轮独立复核**（§4.2 的 **V5** ⇒ 实测 **16**）
> ⇒ ⇒ 「合法但陷入的指令 MUST 装气泡」这条**承重**约束有**独立**判别力证据。

---

## 8. 轮次记录

| 轮 | 时间 | 结论 | 说明 |
|---|---|---|---|
| **1** | 2026-11-26 | ❌ **FAIL** | **B1**（行数锚点 off-by-one ⇒ 护栏红 ⇒ T5.2 声明不成立）+ S1/S2/S3；**归档未执行**（用户指令的闸门未开）；已用 **1/3** 轮 |
| **2** | 2026-11-26 | ✅ **PASS** | builder 修完 B1（+ S2/S3）⇒ verifier 复验：`verify_no_duplicate_rv32i_headers.sh` **exit 0** · 全量重建 + `ctest -LE perf` **273/273 全绿** · **16 个护栏全 PASS** ⇒ ⇒ **归档闸门已开** ⇒ ⇒ T5.4 归档执行（§9 / §10）；已用 **2/3** 轮 |

---

## 9. T5.4 归档（2026-11-26 · verifier 复核）

> 归档在**轮 2 PASS 之后**执行（用户指令「**验收通过后再 openspec archive**」的闸门）。

### 9.1 归档动作

- **命令**：`openspec archive rv32i-ecall-ebreak-wfi-2026-11 -y`
- **实测输出**：
  ```
  Applying changes to openspec/specs/rv32i-privilege/spec.md:
    + 1 added
    ~ 2 modified
  Totals: + 1, ~ 2, - 0, → 0
  Change 'rv32i-ecall-ebreak-wfi-2026-11' archived as '2026-11-26-rv32i-ecall-ebreak-wfi-2026-11'.
  ```
- ⚠️ **非阻塞告警（2 条，如实登记）**：
  ① `Proposal warnings in proposal.md (non-blocking): Change must have a Why section`
  ⇒ ⇒ 本仓的 proposal 用的是 `## Why（为什么现在做）` ⇒ CLI 1.14.3 的**严格标题匹配器**
  不认（⇒ ⇒ **非**缺内容 · **非**违规）⇒ ⇒ 只登记，**不改**（改标题会破坏全仓既有
  `## Why（……）` 风格，且本项的 AC 判据不受影响）；
  ② `Task status: 24/26 tasks / Warning: 2 incomplete task(s) found`
  ⇒ ⇒ 那 2 条正是 **T5.4 / T5.5**（归档 + 验收报告本身）⇒ ⇒ 归档后已在归档件的 `tasks.md`
  里勾上（见 §9.3）。

### 9.2 ⚠️ **事实订正（重要 · 与 P6-3 的记录不同）**

| 项 | P6-3 的记录 | **P6-4 的实测** |
|---|---|---|
| `openspec archive` 对 **MODIFIED Requirement** 的语义 | 「**追加**语义，不删旧文」⇒ 合并后有 **5 处残留** | ⇒ **原地替换** ⇒ ⇒ **零合并残留** |
| 残留订正工作量 | 手工删 5 处合并残留 + 3 处过时旧文 | ⇒ **0 处合并残留** + **6 处过时旧文**（另 capability 的 `## Purpose`） |

- **根因（机制）**：CLI 1.14.3 的 MODIFIED 合并 = **按 Requirement 标题定位 + 整段替换**。
  ⇒ ⇒ **只要 delta 对该 Requirement 做了「完整重述」（含**全部** Scenario）**，
  合并即**无残留**；P6-3 那次之所以出现追加，是因为它的 delta **未完整重述**
  （⇒ `scripts/verify_openspec_delta.sh` 的 **R3** 正是这条判据 ⇒ 本仓的护栏已经在管这件事）。
- ⇒ ⇒ **对本仓的可复用结论**：`verify_openspec_delta.sh` 的 R3「MODIFIED 标题与主 spec
  逐字一致」**不足以**保证「无残留」⇒ ⇒ **真正承重的是「完整重述（含全部 Scenario）」**
  ⇒ ⇒ 本仓 delta 的写法已满足 ⇒ ⇒ **零残留是可复现的，不是运气**。

### 9.3 手工订正（归档后 · 逐条）

| # | 文件 | 订正内容 | 依据 |
|---|---|---|---|
| **1** | `openspec/specs/rv32i-isa-semantics/spec.md` | **操作码分类定义**：`1` = 已知未实现（**P6-4 起只剩 `MRET`** · `imm[11:0] = 0x302`）· 并补注 `ECALL`/`EBREAK`/`WFI` 与 CSR 指令**都是合法指令**（分类 **0**）· `SRET`（`0x102`）M-only 下**不可编码** ⇒ 分类 **3** | 实现（`rv32i_decoder.h`）+ 规范 Priv §2.1.3 |
| **2** | 同上 | **优先级**：`1`（**`MRET`**）> `3`（保留组合 / 不可编码）> `2`（未知）> `0`（合法） | 同上 |
| **3** | 同上 | **行为策略**：`reason = 1` ⇒ **只剩 `MRET`**（仍冻结）· 并补 `ECALL`/`EBREAK` 的处置（合法 + 陷入 · 触发判据用 `sys_trap_kind ≠ 0`）与 `WFI`（合法 NOP） | 同上 |
| **4** | 同上 | **Scenario「已知但未实现的 opcode 与未知 opcode MUST 可区分」**：判据**按 `imm[11:0]` 分类**（`WHEN` 收窄到 `MRET`） | 同上 |
| **5** | 同上 | **Scenario「已知未实现（`reason = 1`）—— 仍是冻结」**：`WHEN` 收窄到 `MRET` | 同上 |
| **6** | 同上 | **Scenario「正确路径的『冻结是终态』—— 仅对 `reason = 1` 成立」**：`reason = 1` **只剩 `MRET`** | 同上 |
| **7** | `openspec/specs/rv32i-privilege/spec.md` | **`## Purpose` 手工更新**（「归档后手工写」通道 · 指导 §「`## Purpose` 的修改通道」）：三段 → **四段**（新增 ④ `funct3 = 0` 细分 + WARL）· ① 的触发集与 `mcause` 改为动态 · **覆盖**清单补新覆盖 · **⚠️ 边界**清单删掉已兑现的 `ECALL`/`EBREAK`/`WFI`（并注明「此前那句已作废」）· **接缝**段补 P6-4 对「`reason = 1` 具体指哪些指令」的修订 | 归档后手工写通道（delta 不含 `## Purpose`） |

### 9.4 归档后复核（verifier 实测）

| 检查 | 结果 |
|---|---|
| `openspec validate --specs` | ✅ **54 passed / 0 failed**（warning 均为**既有**的「Requirement text is very long」，与本项无关） |
| `verify_openspec_purpose.sh` | ✅ **PASS**（占位符 0 · 登记债 26 条完整 · 54 个 spec 已扫） |
| `verify_openspec_delta.sh` | ✅ exit 0（**零信号**：归档后无 active change ⇒ ⚠️ 「零信号 ≠ 合格」· 已在归档**前**跑过 4/4 绿） |
| `rv32i-privilege` 的 Scenario 结构 | ✅ **无重复标题** · 唯一「孤立块」= `#### Scenario: 6 种 funct3 的 csr_wdata 语义` ⇒ ⚠️ **既有问题**（P6-2 的**纯表格式** Scenario · 无 `THEN` 行）⇒ ⇒ 已在 P6-3 的验收报告 §7 末段登记 ⇒ ⇒ **不由本项引入** ⇒ ⇒ 本次**不修**（属 P6-2 的形状选择 · 须独立立项） |
| `rv32i-isa-semantics` 的 Scenario 结构 | ✅ **无重复标题 · 无孤立块** |
| 归档件的 `tasks.md` | ✅ **T5.3**（7600 → **7601**）· **T5.4** · **T5.5** 已勾上并附**实测事实**（与 P6-3 的归档件同款做法） |
| 仓内「active change 路径」残留引用 | ✅ **零残留**（决策单 / 母 improvement / `phase-6.md` / `phase-6-next-steps.md` 的 4 处路径全部指向归档件） |
| 归档后护栏复跑（9 个） | ✅ 全 PASS：`verify_no_duplicate_rv32i_headers` · `verify_riscv_soc_core_ports` · `verify_openspec_purpose` · `verify_docs_links` · `verify_roadmap_fragments` · `verify_public_api_visibility` · `verify_test_counts` · `verify_dual_backend_parity` · `verify_agents_size` |

## 10. 最终结论

- **验收结论**：✅ **PASS**（轮 2）· 全部 **AC1–AC12** 满足（每条有工具证据）·
  **6 条独立变异** V1–V6 全部转红 ⇒ 判别力**独立**于 builder 自证。
- **本项遗留**：轮 1 的 **B1** 已闭合；**S1**（isa-semantics 过时旧文）已在归档步闭合；
  **S2 / S3** 已闭合；**零 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock**。
- **未纳入本项的既有形状问题**（**如实登记 · 不静默留尾**）：`rv32i-privilege` 的
  `#### Scenario: 6 种 funct3 的 csr_wdata 语义`（P6-2 的**纯表格式** Scenario · 无 `THEN` 行）
  ⇒ ⇒ 已在 P6-3 的验收报告 §7 末段登记 ⇒ ⇒ **须独立立项**（不在本项范围）。
- **下游影响**：**下游若把 `ECALL` / `EBREAK` / `WFI` 当作「已知未实现 / 冻结」处理，MUST 更新其期望**
  （新行为已由规范锁定 · 契约已写进 capability `rv32i-privilege`）。
