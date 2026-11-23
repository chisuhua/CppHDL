# tasks · 提交事件端口脉冲化

> **Change**：`rv32i-commit-event-pulse-2026-11`
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> **零行为变化**（观测面语义订正）⇒ ⇒ **核端口面 34 不变** ⇒ ⇒ C6 快照**不动**

## P0 · 判据先行（先见红）

- [ ] **T0.1** 新建 `examples/riscv-mini/tests/test_commit_event_pulse.cpp`
  （Catch2）⇒ 程序 = `addi x1,x0,42` · `lw x2,0(x0)`（**冷 D-TCM ⇒ D-L1 miss ⇒ MEM 级多拍冻结**）
  · `addi x3,x0,1` · `jal x0,0`
  ⇒ ⇒ 编码**手推**（禁抄仿真输出）⇒ ⇒ 在 `examples/riscv-mini/AGENTS.md` 的
  「核端口契约表」旁登记期望
- [ ] **T0.2** 断言 **AC1**：MEM 级被冻结的拍 ⇒ `dbg_wb_write_en` **逐拍为 0**
  ⇒ ⇒ **旧实现下必红**（电平持续为 1）⇒ ⇒ 先见红
- [ ] **T0.3** 断言 **AC2**：**写回事件次数 == 3**（x1/x2/x3）· **store 事件次数 == 0**
  ⇒ ⇒ **旧实现下必红**（= 3 + 停顿拍数）⇒ ⇒ 先见红
- [ ] **T0.4** 断言 **AC3**：停顿期间 `obs_perf_instructions` **逐拍不增长**
  ⇒ ⇒ 与两根事件端口**同时**为 0（口径一致性）
- [ ] **T0.5** 注册 target 进 `examples/riscv-mini/CMakeLists.txt`
  （`riscv_mini_catch_test` helper）⇒ ⇒ **CTEST_TOTAL 275 → 276**
  ⇒ ⇒ 同步 `AGENTS.md` 的 `CTEST_TOTAL` 锚点 + `scripts/verify_test_counts.sh` 的基线
- [ ] **T0.6** 断言 **AC5**（在 `test_rv32i_soc_dual_parity.cpp` 里**追加**，不动既有 golden）：
  **不折**的原始逐拍写回序列里**不存在相邻重复项**⇒ ⇒ **旧实现下必红**

## P1 · 实现（改 2 行）

- [ ] **T1.1** `include/cpu/pipeline/rv32i_pipeline.h:693`
  ⇒ `io().data_write_en <<= exmem_is_store & !mem_stall;`（附口径注释）
- [ ] **T1.2** `include/cpu/pipeline/rv32i_pipeline.h:891`
  ⇒ `io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;`（附口径注释）
- [ ] **T1.3** ⚠️ **禁改**：`dbg_exmem_is_store` / `dbg_id_is_store`（**级属性**端口 ⇒ 电平正确）
  ⇒ ⇒ `reg_file.write` 的使能口径**不动**（幂等 ⇒ 零可观测影响 ⇒ 见 design §4 R1）

## P2 · 验证（先窄后宽）

- [ ] **T2.1** **全量重建**（`cmake --build build -j$(nproc)`）⇒ ⇒ ⚠️ **禁**只重建被点名的 target
  （旧二进制 ⇒ 假绿）
- [ ] **T2.2** 新 target **全绿** ⇒ ⇒ AC1 / AC2 / AC3 / AC5 过
- [ ] **T2.3** 核级 `tb_rv32i_soc` ⇒ ⇒ `RESULT: checks=24 fails=0`（**AC4** · 零变化）
- [ ] **T2.4** `test_rv32i_soc_dual_parity` ⇒ ⇒ **写回序列 golden 逐项不变**
  ⇒ ⇒ 「折连续相同」workaround **保留**（脉冲化后**幂等**）
- [ ] **T2.5** `ctest` 全绿（**276**）· `run_all_ported_tests` PASS
- [ ] **T2.6** 全部 `scripts/verify_*.sh` 全绿（**含 C6 快照** —— 端口面零变更 ⇒ C6 不动）

## P3 · 判别力自证（变异 · MUST 实测转红）

- [ ] **T3.1** **M1** = 去掉 `dbg_wb_write_en` 的 `& !mem_stall`
  ⇒ ⇒ **AC1** 转红 + **AC5** 转红 ⇒ ⇒ **重建后**复跑（还原源码 ≠ 复原）
- [ ] **T3.2** **M2** = 去掉 `data_write_en` 的 `& !mem_stall`
  ⇒ ⇒ store 事件次数的断言转红 ⇒ ⇒ **重建后**复跑
- [ ] **T3.3** **M3** = 两根端口**对调** ⇒ ⇒ **AC2** 转红（写回 3 ↔ store 0 互换）
- [ ] **T3.4** 每个变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）
- [ ] **T3.5** 全部还原 ⇒ ⇒ 复跑 ⇒ ⇒ 全绿 ⇒ ⇒ 记录进期望台账

## P4 · 登记面（文档同步 · 与代码同批）

- [ ] **T4.1** `.rddf/checks/public-api-surface.md` ⇒ 「**语义订正**」节登记两根端口的语义变更
  （端口名 / 方向 / 位宽**未变** ⇒ 不属「新增 / 移除公开符号」⇒ 登记为语义订正）
- [ ] **T4.2** `docs/CHANGELOG.md` ⇒ 一行
- [ ] **T4.3** `include/cpu/AGENTS.md` ⇒ 流水线观测面约定补一节
  （「**事件端口** = 单拍脉冲」vs「**级属性端口** = 电平」）
- [ ] **T4.4** `examples/riscv-mini/AGENTS.md` ⇒ 核端口契约表加**语义脚注**
  （`top_core_obs_data_write_en` / `top_core_dbg_wb_write_en` 现为**事件脉冲** ⇒ **表本身不动**）
- [ ] **T4.5** 母 improvement `rv32i-soc-platform-2026-11` ⇒ 批次表加一行
  （**S4a-0** = 本 change）⇒ ⇒ 进度行同步

## P5 · 收口

- [ ] **T5.1** `openspec validate --all` ⇒ ⇒ 绿
- [ ] **T5.2** `scripts/verify_openspec_delta.sh` ⇒ ⇒ 绿（**实际门**）
- [ ] **T5.3** 过 **critic**（`review_profile: build`）—— ⚠️ 本 change 触及
  **公开观测面语义** + **已签批 capability 的 MODIFIED** ⇒ ⇒ MUST 审
- [ ] **T5.4** 归档（`openspec archive`）⇒ ⇒ **S4a 的硬前置解除**
