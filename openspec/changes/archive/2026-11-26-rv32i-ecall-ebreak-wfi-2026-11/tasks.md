# tasks · RV32I **`ECALL` / `EBREAK` / `WFI` + `mtvec`/`mepc` WARL**（P6-4）

> **Change**：`rv32i-ecall-ebreak-wfi-2026-11` · **母 improvement**：`rv32i-privileged-csr-2026-11`（P6-4）
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（核 IP 顶层 53 端口契约表 + C6 快照 **不动**）

---

## T0 · 侦察与裁决（**只读 + 决策单**）

- [x] **T0.1** `OP_SYSTEM` 的分派现状：`funct3 = 0` **整桶归 `reason = 1`**
      （`ECALL`/`EBREAK`/`MRET`/`WFI` 一起冻结）⇒ ⇒ 缺口在 `rv32i_decoder.h`
- [x] **T0.2** `CsrBank` 的 `mtvec` / `mepc` 写路径**均无 coercion**；
      `mtvec` 初值 `0x2000_0100` 不在任何映射区（`rv32i_mem_map.h`）
- [x] **T0.3** `ExceptionUnit` 在 `start_trap` 拍**锁存** `exception_cause`
      ⇒ ⇒ 原因码可以是**组合** mux（无需额外寄存器）
- [x] **T0.4** **规范原文核对**（Priv v20260120）：
      `mtvec` WARL / `mcause` 编码表（异常 vs 中断分表）/ `mepc[1:0]` 恒 0 /
      `WFI` 可实现为 NOP / `ECALL`/`EBREAK`/`MRET`/`SRET` 的 `imm` 编码
- [x] **T0.5** 决策单落盘并对 D-1 / D-2 取得裁决：
      `.rddf/checks/decision-request-rv32i-privileged-d1-d2-2026-11.md`
      （D-1 = A · D-2 = 全部按推荐）

## T1 · 立项

- [x] **T1.1** `proposal.md`（Why / IN·OUT / **AC1–AC12** / Impact）
- [x] **T1.2** `design.md`（裁决表 / 实现 / 判据设计 / 错法 / **变异矩阵**）
- [x] **T1.3** `tasks.md`（本文件）
- [x] **T1.4** spec delta：`rv32i-privilege` **MODIFIED ×2 + ADDED ×1**

## T2 · 实现（**先见红**）

- [x] **T2.1** 先改既有判据的**期望值**（语义变更）⇒ 旧断言**转红**：
      `test_csr_unit`（`mtvec` 复位值）· `test_rv32i_csr_instructions`（ECALL ⇒ 冻结）·
      `test_rv32i_soc_dual_parity`（ECALL 场景）· `test_rv32i_trap_entry`（AC12）
- [x] **T2.2** `rv32i_decoder.h`：`funct3=0` 按 `imm[11:0]` 细分 + `sys_trap_kind` 输出
- [x] **T2.3** `id_stage.h`：`sys_trap_kind` 透传（`valid` 门控）
- [x] **T2.4** `rv32i_pipeline.h`：触发集扩展 + 原因码 mux
- [x] **T2.5** `rv32i_pipeline.h`：`illegal_active` 纳入 `sys_trap_kind`
      （⚠️ **实测缺陷**：不加 ⇒ `retire(20) = 16`，应为 **13**）
- [x] **T2.6** `rv32i_csr.h`：`mtvec` 写掩码 / `mtvec` 初值 `0x8000_0100` / `mepc` 写掩码
- [x] **T2.7** `rv32i_exception.h`：原因码注释**分表**（异常 / 中断）

## T3 · 判据

- [x] **T3.1** `test_rv32i_trap_entry.cpp` +6 用例（ECALL / EBREAK / SRET / 未定义 imm / WFI / MRET）
- [x] **T3.2** `test_csr_unit.cpp` +1 用例（WARL coercion：`mtvec.MODE` + `mepc[1:0]`）
- [x] **T3.3** 4 处既有判据的语义订正（见 T2.1）
- [x] **T3.4** 目标二进制全绿：
      `test_rv32i_trap_entry`（11 用例 / 1196 断言）· `test_csr_unit`（15 用例 / 60 断言）·
      `test_rv32i_csr_instructions`（8 用例 / 31 断言）· SoC 用例（237 断言）

## T4 · 变异（**每条须仍能编译**）

- [x] **T4.1** M1–M12 施加 / 构建 / 跑目标判据 / 还原（结果登记 `design.md` §8）

## T5 · 收口

- [x] **T5.1** 全量重建 + `ctest -LE perf --output-on-failure` ⇒ **273/273 全绿**（EXIT=0）
- [x] **T5.2** 相关护栏 PASS：`verify_no_duplicate_rv32i_headers` · `verify_hierarchical_rtl` ·
      `verify_riscv_soc_core_ports` · `verify_hierarchical_port_naming` · `verify_openspec_delta` ·
      `verify_docs_links` · `verify_test_counts` · `verify_dual_backend_parity` ·
      `verify_example_test_wiring` · `verify_example_assertion_quality` · `verify_public_api_visibility` ·
      `verify_roadmap_fragments` · `verify_input_port_additions` · `verify_input_port_required_registry` ·
      `scripts/rv32i_rtl_sim_env.sh`（flat/hier 各 `checks=24 fails=0`）
- [x] **T5.3** `include/cpu/AGENTS.md` 的事实 / 计数同步（30 headers · **7601** lines · 1196 断言）
      ⇒ ⇒ ⚠️ **验收轮 1 发现 off-by-one**（自述 7600 ≠ 实测 7601 ⇒ 护栏 `verify_no_duplicate_rv32i_headers`
      的 **C5 红**）⇒ ⇒ **轮 2 已订正为 7601** ⇒ ⇒ 护栏复跑 **exit 0**
- [x] **T5.4** ✅ **归档**（`openspec archive rv32i-ecall-ebreak-wfi-2026-11 -y` ⇒
      `rv32i-privilege: + 1 added · ~ 2 modified` ⇒ 归档为
      `openspec/changes/archive/2026-11-26-rv32i-ecall-ebreak-wfi-2026-11/`）
      ⇒ ⇒ ⚠️ **事实订正（CLI 1.14.3 实测）**：本仓 delta 对 MODIFIED Requirement 做了
      **完整重述**（含**全部** Scenario）⇒ ⇒ **合并 = 原地替换** ⇒ ⇒ **零合并残留**
      （**不是** P6-3 记录的「追加语义」—— 那次 delta 未完整重述 ⇒ ⇒ 追加）
      ⇒ ⇒ **手工订正 6 处「因语义变更而过时的旧文」**（`rv32i-isa-semantics` 主 spec 的
      `reason = 1` 清单：分类定义 / 优先级 / 行为策略 / 3 个 Scenario）
      ⇒ ⇒ **手工更新 `rv32i-privilege` 的 `## Purpose`**（三段 → **四段** · 边界清单同步）
      ⇒ ⇒ `openspec validate --specs` **54 passed / 0 failed** · `verify_openspec_purpose.sh` PASS
      ⇒ ⇒ 逐条见验收报告 §9
- [x] **T5.5** ✅ **验收报告**：`.rddf/reviews/rv32i-ecall-ebreak-wfi-2026-11-verify.md`
      （**verifier 角色**写入 · **2 轮**：轮 1 **FAIL**（1 BLOCKER = 行数锚点 off-by-one）
      ⇒ 轮 2 **PASS** ⇒ ⇒ 另含 **6 条独立变异** V1–V6，全部转红）

---

## 复现命令

```bash
cd /workspace/project/CppHDL
cmake --build build --target test_rv32i_trap_entry test_csr_unit \
    test_rv32i_csr_instructions test_rv32i_soc_dual_parity -j"$(nproc)"
./build/examples/riscv-mini/test_rv32i_trap_entry
./build/examples/riscv-mini/test_csr_unit
./build/examples/riscv-mini/test_rv32i_csr_instructions
./build/examples/riscv-mini/test_rv32i_soc_dual_parity \
    "Rv32iSoc - IllegalOpcodeObservableNotRetired + FenceIsNop"
```

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest --test-dir build -LE perf --output-on-failure
```

```bash
cd /workspace/project/CppHDL
bash scripts/verify_openspec_delta.sh rv32i-ecall-ebreak-wfi-2026-11
bash scripts/verify_no_duplicate_rv32i_headers.sh
```
