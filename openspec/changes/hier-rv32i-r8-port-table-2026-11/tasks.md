# Tasks: RV32I 产品级层次化语料的顶层端口表 == 手推期望集（R8′）

## 1. 护栏（R8′）

- [x] 1.1 `scripts/verify_hierarchical_rtl.sh`：
      新前置（fail-closed）= `build/examples/riscv-mini/emit_rtl_rv32i_soc_hier`
      （+ 文件头「前置」段与「R8′」判据说明同步）
- [x] 1.2 新块 `r8p_rv32i_port_set()`：发射 ⇒ `verilator --json-only` ⇒
      `scripts/lib/verilog_tree_extract.py --ports` ⇒ 与**手推期望集**（**34** 根）逐项比对
      （名字 + 方向 + 宽度 + 顺序）
- [x] 1.3 手推期望集**推导源** = `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 `__io`
      （4 in + 28 out ⇒ 声明顺序即端口顺序）+ 框架 2 ⇒ 34 根
      ⇒ ⇒ **禁**从产物输出抄（抄了 = 判据自证 ⇒ 恒绿）
- [x] 1.4 `run_all` 接线（发射 + 判据）· `self_test` 编号 `/10` → `/11`

## 2. 判别力自证

- [x] 2.1 `--self-test` 新增**投毒 11**：改产品顶层端口宽度（`top_dbg_id_pc` 32 → 16）
      ⇒ ⇒ R8′ 必红 ⇒ ⇒ 证明「期望集**真的在比**」（否则可能恒绿）

## 3. 期望源登记

- [x] 3.1 `.rddf/checks/examples-rtl-expectations.md` 的 RV32I 段：新增「顶层端口表的
      **独立手推判据**（R8′）」小节（写明既有两条判据**与产物同源** + R8′ 的推导源与投毒）

## 4. 验证

- [x] 4.1 `bash scripts/verify_hierarchical_rtl.sh` ⇒ PASS（R1–R9 + **R8′**）
- [x] 4.2 `bash scripts/verify_hierarchical_rtl.sh --self-test` ⇒ PASS（13/13 对照全生效）
- [x] 4.3 `bash scripts/verify_examples_rtl_sim.sh` ⇒ PASS（RV32I 产品面不回归）
- [x] 4.4 `bash scripts/verify_examples_rtl_sim_coverage.sh` ⇒ PASS
- [x] 4.5 `openspec validate hier-rv32i-r8-port-table-2026-11 --strict` +
      `scripts/verify_openspec_delta.sh` ⇒ PASS

## 5. 归档 + 残留两侧同批

- [x] 5.1 `openspec archive hier-rv32i-r8-port-table-2026-11 --yes --skip-specs`
- [x] 5.2 残留登记表 id=25 → **已收口**（闭口证据 = 本归档 change）+ 台账 §7 行 25 两侧同批
      ⇒ ⇒ `scripts/verify_arch_ledger_residuals.sh` ⇒ PASS
