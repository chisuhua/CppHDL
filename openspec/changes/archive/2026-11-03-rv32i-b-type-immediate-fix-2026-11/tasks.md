# Tasks：RV32I B 型立即数 bit 11 修复（ADR-063 实施）

## T1 —— 实现修复（D1 / D3）

- [x] `include/cpu/pipeline/rv32i_decoder.h:113` 补 `<< ch_uint<4>(11_d)`（带注释说明）
- [x] `include/cpu/pipeline/stages/id_stage.h:145` 同上（**生产路径**）
- [x] 两处**各修一行**，**不**收敛两份重复实现（ADR-063 D2 / 选项 F 推迟）

## T2 —— golden 翻转（D5）

- [x] `tests/test_rv32i_stage_dual.cpp:204`：`0xFFFFF7F9` → `0xFFFFFFF8`
- [x] `tests/test_rv32i_stage_dual.cpp:205`：`0x00000001` → `0x00000800`（`imm[11]=1` 边界）
- [x] `tests/test_rv32i_timing_dual.cpp:118`：`0xFFFFF7F9` → `0xFFFFFFF8`
- [x] 验证「不变」清单：SoC parity golden / `pipeline_behavior` / 3 个 example / 227 个 ctest

## T3 —— D4 等价性判据

- [x] `tests/test_rv32i_stage_dual.cpp` 新增 `ImmConsistencyDut` + pair 4
      （并排实例化 `Rv32iDecoder` + `IdStage`，10 条指令，断言 `dec_imm == id_imm`）
- [x] `tests/CMakeLists.txt` 给该 target 补 `include/cpu/pipeline` 路径
- [x] 如实登记判别力边界：**两份一起错**时该判据恒绿（M-c 实测）

## T4 —— 变异验证（D6；每条先确认仍可编译；还原用 `cp` 后重建）

- [x] M-a 只回退 decoder 的 `<<11` ⇒ 🔴 C1 4 failed
- [x] M-b 只回退 id_stage 的 `<<11` ⇒ 🔴 C1 2 + C2 2 failed
- [x] M-c 两处都回退 ⇒ 🔴 C1 2 + C2 2（D4 pair 绿 = 已登记的边界）
- [x] M-d `<<11` → `<<12` ⇒ 🔴 C1 2 + C2 2（⚠️ 首版注入器吞掉分号 ⇒ 编译失败 ⇒ 修正后重跑）
- [x] M-e `sext<32>` → 零扩展 ⇒ 🔴 C1 2 + C2 2

## T5 —— 契约与验收

- [x] capability `rv32i-isa-semantics` ADDED ×1（1 Requirement + 4 Scenario）
- [x] `bash scripts/verify_openspec_delta.sh` 4 条全绿
- [x] `openspec validate rv32i-b-type-immediate-fix-2026-11 --strict` 通过
- [x] 全量重建后 `ctest -LE perf` **227/227** 绿
- [x] 计数锚点同步：`DUAL_BACKEND_PARITY_PAIRS` **271 → 272**（其余不变）
- [x] 归档**未**用 `--skip-specs`

## 不做（ADR-063 D8）

- [ ] 立即数提取「两份 → 单一实现」收敛（选项 F）⇒ 独立议题
- [ ] 移位量容器宽度统一 · `id_stage.h` 非自包含 · `BRANCH_*` 常量 ⇒ 各自已立项
