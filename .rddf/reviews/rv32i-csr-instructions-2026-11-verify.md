# 验收报告 · `rv32i-csr-instructions-2026-11`（RV32I CSR 指令 · P6-2）

> **判据来源**（按优先级）：
> 1. `openspec/changes/rv32i-csr-instructions-2026-11/proposal.md` 的 `## 验收标准`（**AC1..AC13**）
> 2. `design.md`（§1–§8）+ `tasks.md`（T0–T5）
> 3. capability `rv32i-privilege` 的 delta（`specs/rv32i-privilege/spec.md` · **ADDED ×2**）
> 4. 根 `AGENTS.md` 的 RED 线（`HazardUnit` 端口面冻结 · ZERO-DEBT · 计数锚点）
>
> **判定纪律**：每条判据 MUST 有**工具证据**；期望值**手推**（禁抄仿真输出 ——
> 解释器与 codegen 同源 ⇒ 双后端一致地错 ⇒ parity 对该族缺陷结构性失明）。
> **已用轮次**：1（首次验收 · 无返工轮）。

---

## 1. 逐条判定

### AC1 · 6 种 `funct3` 的 `csr_wdata` 语义 ✅ PASS

- **证据**：`examples/riscv-mini/tests/test_rv32i_csr_instructions.cpp` 的
  `TEST_CASE("CSR - 6 种 funct3 的 wdata 语义与读回")` ⇒ 写回序列逐条 == 手推：
  ```
  x1=0x12345000 (lui) · x1=0x12345678 (addi)
  x2=0x0        (csrrw 读到旧值)   · mscratch=0x12345678
  x3=0x12345678 (csrrs, rs1=x0 ⇒ 不写)
  x4=0x12345678 (csrrc 读到旧值)   · wdata = 0x12345678 & ~0x12345678 = 0
  x5=0x0        (csrrwi 读到旧值)  · mscratch=0x1f
  x6=0x1f       (csrrsi)          · wdata = 0x1f | 0x10 = 0x1f
  x7=0x1f       (csrrci)          · wdata = 0x1f & ~0x0f = 0x10
  x8=0x10       (csrrs 读回)
  ```
- **工具**：`./build/examples/riscv-mini/test_rv32i_csr_instructions` ⇒
  **8 用例 / 30 断言全过**（exit 0）。

### AC2 · 读回（`csrrs rd, csr, x0`）✅ PASS

- 同一用例的 `x3` / `x8` 行；`mscratch` 复位值 0（`CsrBank` 的 `ch_reg` init）为手推前提。

### AC3 · 相邻两条 MUST 有序（读到新值）✅ PASS

- **证据**：`TEST_CASE("CSR - 相邻两条 MUST 有序（读到新值）")` ⇒ `x2=0x0`（旧）·
  `mscratch=0x2A` · `x3=0x2A`（**新值**）。
- **机制**：`csr_stall`（顶层 · 1 拍）⇒ ⇒ 写者的写落沿后读者才锁存读值。
- **判别力**：变异 M1（`csr_stall = ch_bool(false)`）⇒ 该用例**转红**（`x3=0x0`）✔

### AC4 · `csrrs` / `csrrc` 的读-改-写语义 ✅ PASS

- **证据**：`TEST_CASE("CSR - CSRRC 掩码是 &~x 而不是 ^x")`
  —— `old=0x0F` · `x=0xF0` ⇒ `& ~x = 0x0F`（`^ x` 会是 `0xFF`）· `x4=0x0F`；
  `csrrci 0x10` 同理（`x6=0x0F` vs 变异 `0x1F`）。
- **判别力**：变异 M2（`& ~` → `^`）⇒ **转红** ✔

### AC5 · 立即数型用 `zimm`（不是 rs1 值）✅ PASS

- **证据**：`TEST_CASE("CSR - 立即数型用 zimm 而不是 rs1 值")`
  —— 诱饵 `x5=0x3F`、`zimm=0x05` ⇒ `csrrs` 的 wdata = `0x10|0x05=0x15`；
  `x3=0x15` · `x4=0x15` · `x6=0x05`（若误用 rs1 值 ⇒ `0x3F`）。
- **判别力**：变异 M3（源恒取 `fwd_mux` 的 rs1）⇒ **转红** ✔

### AC6 · `rd=0` / 源为 0 的两个独立维度 ✅ PASS

- **证据**：`TEST_CASE("CSR - rd=0 不读但写 · CSRRW 源为 0 仍写")`
  —— `csrrw x0, mscratch, x1` ⇒ 无 x0 写回、`mscratch=0x11`；
  `csrrw x0, mscratch, x0` ⇒ `mscratch=0`（**源 0 仍写**）⇒ `x3=0x0`。
- **判别力**：变异 M4（`CSRRW` 也按「源 ≠ 0」门控）⇒ **转红**（`x3=0x11`）✔
- ⚠️ **如实登记的边界**：set/clear 型「源为 0 ⇒ 不写」在**值面不可判别**
  （写 `old|0` 与 `old&~0` 都等于 `old`）⇒ ⇒ 该分支的判别落在**写使能表达式**上
  （代码评审 + 规范要求），本报告不声称有值级判别力。

### AC7 · `funct3 = 4` ⇒ `reason == 3` 且冻结 ✅ PASS

- **证据**：`TEST_CASE("CSR - funct3=4（保留组合）⇒ reason 3 且冻结")`
  ⇒ `dbg_illegal_reason` 观测到的**非零值集合 == {3}** · `retired == 0`。
- **判别力**：变异 M7（`sys_reserved` 仍归 1）⇒ **转红**（实得 1）✔
  ⚠️ 变异 **M6（单改 `legal_opcode`）= 无效变异**：`sys_known_unimpl` 在 select 链
  **更前** ⇒ 值不变（**不构成判别力缺口**）⇒ 组合变异 **M6b** 转红 ✔

### AC8 · `funct3 = 0` ⇒ `reason == 1` 且冻结 ✅ PASS

- **证据**：`TEST_CASE("CSR - funct3=0（ECALL）⇒ reason 1 且冻结")`
  ⇒ 非零集合 == {1} · `retired == 0`（**MUST NOT** 变「合法但无功能」）。
- **判别力**：变异 M6b（整体归合法 + 去 `sys_known_unimpl`）⇒ **转红** ✔
- **负向对照**：合法 CSR 程序的 `illegal_reasons` **为空**（AC1 用例断言）✔

### AC9 · `mtip` / `meip` 显式 tie 0 ✅ PASS（双载体）

- **功能面**：`TEST_CASE("CSR - mip 读 0（mtip/meip tie 0 的功能面）")` ⇒ `csrrs x1, mip, x0` ⇒ `x1=0`。
  ⇒ **判别力**：变异 M8（`mtip = 1`）⇒ **转红**（`mip` 读到 `0x80`）✔
- **端口表面**：未驱动的 `ch_in` 会成为产物顶层 `input` ⇒ ⇒ 「漏 tie」由
  `scripts/verify_riscv_soc_core_ports.sh` 的 **53 端口逐字比对**兜住（本次 **PASS**）。
- ⚠️ 如实登记：解释器里「未驱动」也读 0 ⇒ **功能面对「漏驱动」结构性失明**（故需端口表面）。

### AC10 · 零端口面变更 ✅ PASS（**实测** · 非推断）

| 载体 | 结论 | 证据 |
|---|---|---|
| 核 IP 顶层 **53** 端口契约表 | **逐字逐序不变** | `verify_riscv_soc_core_ports.sh` ⇒ V1/V2/V3 全绿（53 / 51 + C6 快照闭环） |
| C6 快照（核自有 51） | 不变 | 同上 V3 |
| 扁平产物顶层端口表 | **53**（与改前同） | `tb_rv32i_soc --emit-rtl` 的 `manifest.json::port_table` |
| 层次化产物顶层端口表 | **53**（与扁平同源） | `--emit-rtl --hierarchical` 的 `port_table` |
| 层次化 module 数 | **8**（`CsrBank` 为 inline ⇒ **不新增层**） | `top.v` 的 `^module` 枚举 |
| tsv / 期望值表 / 独立 Verilog TB | 不动 | 端口表不变 ⇒ 无需改（上方四项） |

⇒ ⇒ ⚠️ **实施期发现并修复的真缺陷**：父层读**子组件输入端口**会产生**不可映射的跨模块边**
（ADR-060 §2.1）⇒ 改为读**子组件输出 / 本层落地网**（`if_stage.io().instruction` /
`fwd_mux.io().alu_input_a` / `hazard.io().flush` / `mem_stall`）后发射通过。
（细节 = `design.md` §8.5 ②）

### AC11 · 每条新判据附一条变异（且变异能编译）✅ PASS

| # | 变异 | 结果 |
|---|---|---|
| M1 | 去掉 `csr_stall` | **RED** |
| M2 | `CSRRC` 用 `^` 代替 `& ~` | **RED** |
| M3 | 立即数型改用 rs1 值 | **RED** |
| M4 | `CSRRW` 源 0 时不写 | **RED** |
| M5 | `CSRRW/CSRRWI` 写成旧值 | **RED** |
| M6 | 单改 `legal_opcode`（整体归合法） | **GREEN = 无效变异**（select 优先级覆盖 · 登记为无效，非缺口） |
| M6b | 组合变异（去 `sys_known_unimpl` + 整体归合法） | **RED** |
| M7 | 保留组合仍归 1 | **RED** |
| M8 | `mtip` 不 tie 0 | **RED** |

⇒ ⇒ 每条变异都**通过编译**（编译失败会显式报告「变异无效」）；每条 RED 都指出**哪条判据**转红。

### AC12 · spec delta 进 `rv32i-privilege`（ADDED）✅ PASS

- `openspec validate rv32i-csr-instructions-2026-11` ⇒ **valid（零 WARNING）**
- `scripts/verify_openspec_delta.sh` ⇒ **4/4 绿**（R1–R4；**未用** `--skip-specs`）
- delta 只有 `## ADDED Requirements`（**既有 Requirement 不动**）

### AC13 · 计数锚点同步 ✅ PASS

- `CTEST_TOTAL` **282 → 283** · `ctest -N` ⇒ **283**（`verify_test_counts.sh` ✓）
- `EXAMPLES_TEST_SOURCES` **24 → 25**（新增 `examples/riscv-mini/tests/*.cpp` ⇒ 命中口径 (c)）
- `VERILATOR_E2E_TOTAL` **81 不变**（未新增双后端标签）· `DUAL_BACKEND_*` **不变**（未新增 parity pair）

---

## 2. 全量回归

| 命令 | 结果 |
|---|---|
| `cmake --build build -j$(nproc)`（**全量重建** —— `ctest` 不会自动重建 ⇒ 只重建部分 target = 假绿） | 0 error |
| `ctest --test-dir build -LE perf --output-on-failure` | **272 / 272 通过**（1122 s · base 247 · chlib 1 · spinalhdl-ported 24 · verilator_e2e 81） |

⚠️ **1 处非预期失败已修**：`tests/test_component_ownership.cpp` 的
`REQUIRE(pipeline.child_count() == 8)` —— 新增第 9 个成员子组件 `csr_bank`
⇒ 期望值陈旧 ⇒ 改为 **9**（附原因注释）⇒ 该目标复跑 **83 断言 / 9 用例全过**。
⚠️ `verify_example_test_wiring` 首次运行报 `test_rv32i_official_suite_g4` **exit 143**
= 与全量 ctest **并发争用**导致超时被杀（输出本身为 “All tests passed”）
⇒ ⇒ 单独复跑 **PASS**（不是回归）。

---

## 3. 遗留清扫复核

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX | **0**（`grep` 四个改动头 + 新测试文件 ⇒ 无命中） |
| 半截实现 | 无（6 种 `funct3` 全部实现 + 5 条边界全部有判据） |
| 注释掉的代码 | 无 |
| 临时调试日志 | 无（变异脚本在 `/tmp/opencode/csr-mut/`，**未入库**） |
| 被 skip 的测试 | 无（新 8 用例全在跑） |
| 未替换的 mock | 不适用（核级 DUT + 预装 TCM 镜像，无 stub） |
| 临时 fallback 分支 | 无 |
| 文档同步 | `include/cpu/AGENTS.md`（CSR 通路段 + `illegal_reason` 语义订正 + 行数锚点 **7169 → 7384**）· 根 `AGENTS.md`（两个计数锚点） |

---

## 4. 路由结论

- 判定：**PASS**（AC1–AC13 全部满足 · 全量回归零失败）
- 失败项：无（`test_component_ownership` 的期望值已按新形态更新，非产品缺陷）
- 返工轮次：**0**（首次验收通过）
