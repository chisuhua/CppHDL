# critic 审稿记录 · riscv-tests-gate-2026-11 · 轮次 1

> **Change**：`riscv-tests-gate-2026-11`
> **review_profile**：`plan, build`（critic 自报 `mixed`）
> **日期**：2026-11-25 · build-agent（builder 角色）
> **预算**：每 change 2 轮 ⇒ 本轮为**第 1 轮**

---

## 1. critic 结论

```
# 审查结论：REJECT
mergeable: no
scope_exceeded: false
```

⇒ ⇒ **1 BLOCKER + 4 SHOULD_FIX + 2 NIT**。

### critic 对三个 focus 的结论（**均无 BLOCKER**）

| focus | critic 结论 |
|---|---|
| **1 · 冻结修法的逐拍对齐** | **无缝隙**：start-edge（`fwd_*_d1` 在拍 T 已持有 E 的档位）· hold（`mem_stall_fwd` 与 `id_stage.freeze` / `exmem_en` / `m2_*` / `memwb_*` **同源同值** ⇒ 锁步）· release-edge（解除拍装入按后继 S 算出的档位，S **下一拍**进 EX 消费；E 在该拍仍持旧档位 ⇒ 正确）。与 ADR-047 D5（打拍结构不动）、ADR-069 §7.1.2（D11 的 ID 读点旁路未被冻结）**均不冲突**；`hazard.io().forward_a/b` 的消费者只有 `fwd_*_d1`（grep 确认）⇒ 冻结完备。两个「错法」的排除论证成立。 |
| **2 · 门禁判定是否可信** | (a) 观测面可信：`obs_data_write_en = data_write_en & data_ready_calc` 是总线侧单拍脉冲，地址/数据同拍采样一致；负向对照证明非恒 1；镜像缺失 ⇒ `saw_tohost=false` ⇒ fail-closed。(b) 4 二进制拆分与护栏 C3「条目名 = 二进制名、无参运行」**等价**（组由编译期宏划定）⇒ TIMEOUT 300 相容。(c)「40/40 全过」有 PROVENANCE §3 的完整口径声明 ⇒ **不构成**过度声称（但 spec 措辞需消歧，见 SF-2）。 |
| **3 · spec delta 契约面** | `rv32i-data-forwarding` 的 MODIFIED 与主 spec 逐字比对：既有内容**零删除**、只追加 1 条 Scenario，与 ADR-047/069 **无冲突、无重复定义**。 |

---

## 2. 逐条处置

### 🔴 BLOCKER · 计数锚点未同步（plan 档）

- **critic**：`CMakeLists.txt` 注册了 **5 个**新 CTest 条目、两个新测试源均含 `TEST_CASE`，
  但根 `AGENTS.md` 锚点仍是 `CTEST_TOTAL: 277` / `EXAMPLES_TEST_SOURCES: 22`
  ⇒ `verify_test_counts.sh` / `verify_example_test_wiring.sh` C2 /
  `update_test_count_anchors.sh --check` 全部转红；且 `tasks.md` T5.1/T5.3 已勾 `[x]` 却未发生。
- **处置**：✅ **已闭合**。
  - 跑 `scripts/update_test_count_anchors.sh`（**唯一更新入口**）⇒
    `CTEST_TOTAL: 277 → 282` · `EXAMPLES_TEST_SOURCES: 22 → 24`
    （其余三个锚点 `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `_PAIRS` **不变** ⇒ 脚本报 ✓）。
  - `tasks.md` T5.3 **订正**：根 `AGENTS.md` 只承载**计数锚点**（见 T5.1）；
    「riscv-mini 完成」的定义落在 `examples/riscv-mini/AGENTS.md` 的 OVERVIEW（**新增**）。
- **回归证据**：`scripts/update_test_count_anchors.sh --check` 绿 · `verify_example_test_wiring.sh` C2 = 24。

### 🟡 SHOULD_FIX 1 · `fwd_b_d1` 半边无判别性用例（build 档）

- **critic**：A1/A2/A3 的判别操作数**全走 rs1（A 口）** —— A1/A2 的冻结指令是 `addi`
  （不消费 B 口）；A3 的 `beq x5,x0` 的 rs2 = x0（污染不改变比较结果）
  ⇒ ⇒ **只回退 `fwd_b_d1` 的变异在三用例下恒绿**。
- **处置**：✅ **已闭合**（选「补形态」而非「登记缺口」）。
  - A2 的冻结指令由 `addi x6, x5, 1`（`0x00128313`）改为 **`add x6, x0, x5`（`0x00500333`）**
    ⇒ 被污染的操作数落在 **rs2**（rs1 = x0 ⇒ 其档位本来恒 0）⇒ 期望值 `x6 = 42`（原 43）。
    指令字由 `riscv32-unknown-elf-as` 汇编 + `objdump` 核对。
  - **判别力自证（实测三档）**：
    | 变异 | A1 | A2 | A3 |
    |---|---|---|---|
    | 只回退 **A 口**（`fwd_a_d1`） | 🔴 | ✅ | 🔴 |
    | 只回退 **B 口**（`fwd_b_d1`） | ✅ | 🔴（`0 == 42`） | ✅ |
    | **两个都回退** | 🔴 | 🔴 | 🔴 |
  ⇒ ⇒ A 口由 A1 / A3 覆盖，B 口由 A2 覆盖 ⇒ **两口各自有判别力**。

### 🟡 SHOULD_FIX 2 · spec 的「全部通过」与排除项 Requirement 有字面张力（plan 档）

- **处置**：✅ **已闭合**。Requirement 1 的 THEN 改为「MUST 全部通过（**依下方「排除项」
  Requirement 登记者除外**）」。

### 🟡 SHOULD_FIX 3 · `ma_data` 的排除理由疑似事实错误（plan 档）

- **critic**：`ma_data` 的通行认知是 **misaligned data**，不是「主动式多 hart」。
- **处置**：✅ **已核对上游并订正**：`isa/rv32ui/ma_data.S` 只是 `#include "../rv64ui/ma_data.S"`，
  而后者首行注释 = 「**Test misaligned ld/st data**」（`lh`/`lw` 在偏移 **1/2/3**）。
  ⇒ ⇒ 三处措辞改正（`PROVENANCE.md` §3 · `env/build.sh` · 门禁源文件注释）：
  「本核 load/store 的车道选择**忽略低地址位**（`LH` 只看 `addr[1]` · `LW` 忽略 `addr[1:0]`）
  ⇒ 非对齐访问被**静默对齐**（既不处理也不 trap）⇒ ⇒ **能力对齐**」
  ⇒ ⇒ 该口径的既有载体 = `include/cpu/AGENTS.md` 的「load 侧的字节道契约」段。

### 🟡 SHOULD_FIX 4 · 缺「判定 MUST 有负向对照」Requirement（plan 档）

- **处置**：✅ **已闭合**。新 capability 追加 Requirement
  「**门禁的判定 MUST 有负向对照（防恒绿）**」+ 1 条 Scenario（两种恒绿形态 MUST 被堵住）。
  代码侧的负向对照（`test_rv32i_official_suite.cpp` 的手写镜像写 3 ⇒ 读到 3）**已在位**。

### 🔵 NIT 1 · `examples/riscv-mini/AGENTS.md` 的 target 计数 off-by-one

- **处置**：✅ **已订正** `23 → 24`（实数 = 20 个顶格 `riscv_mini_catch_test` 调用
  + `foreach` 展开的 4 个 = 24 个 CTest 条目，另 1 个 noctest 只构建）。

### 🔵 NIT 2 · AC3 与 tasks T2.5 的变异范围表述不一致

- **处置**：✅ **已对齐**：两处都改为「只回退 A 口 / 只回退 B 口 / 两个都回退」三档，
  与实测结果逐条对应。

---

## 3. 复审

- **BLOCKER 已闭合** ⇒ 按预算（2 轮）**不另开复审轮**；
  修复项的判别力由**实测变异三档**（见 SF-1 的表）承担 ⇒ 该表即闭合证据。

## 4. backlog（本 change 范围外 · 只记录不清理）

- 官方套件只能跑**解释器后端**（该核设计含 128 bit 的 mem 节点 ⇒ JIT 整体跳过 ⇒ ≈ 36 拍/秒）
  ⇒ ⇒ **性能债**：若日后 JIT 支持该宽度，可把门禁耗时压下来。
- 4 组拆分后单组 95–122 s / `TIMEOUT 300` ⇒ 慢 ~2.5× 的机器会触顶
  ⇒ ⇒ `kMaxCycles = 4000` 是 fail-closed（超时判红，不挂死）⇒ **已知且接受**。
