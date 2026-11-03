# Tasks：路线 C 第 2 批 —— RV32I 的「时序侧」组件进双后端 parity

## C0 —— 可转性 4 问

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（三个符号在 60 个 macro dual 文件里各 0 次；
      唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ 判别形态不同 ⇒ 逐案裁决通过）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 11/12 抓（+ 1 条等价变异）
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ **零 DUT 复制**（三个组件本身带顶层端口 ⇒ 直接当 DUT）
- [x] ⚠️ **刻意不做 `InstrTCM`**：`sread` 时序在「地址是输入端口」时**不可判别**（陷阱 T2）

## C1 —— 新文件

- [x] `tests/test_rv32i_timing_dual.cpp`
      ⇒ pair 1 `riscv::IdStage`（10 组：气泡契约 ×3 + 立即数 ×5 + mux 语义）
      ⇒ pair 2 `riscv::Rv32iRegFile`（16 组：x0 硬连线 + 写使能 + 地址分派）
      ⇒ pair 3 `riscv::DataTCM<8,32>`（10 组：读-改-写字节合并 + valid 门控）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test` + 1 条 `target_include_directories`
      （`id_stage.h` 非自包含 ⇒ 沿用既有 `test_rv32i_pipeline_behavior` 的同款绕过）

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**464 == 2 × 232**
- [x] 变异 **11/12 抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 期望值**手推**（独立 Python 模型 + **规范编码器** ⇒ 一次通过）
- [x] ⚠️ 如实登记 **1 条等价变异**（M7：写 x0 的三重冗余防护 ⇒ 不可判别）

## C3 —— 缺陷登记（本批的副产品）

- [x] 🔴 `include/cpu/pipeline/stages/id_stage.h` **非自包含** ⇒
      `.rddf/improvements/id-stage-header-not-self-contained-2026-11.md`
- [x] ⚠️ **本批刻意不修**（零行为影响 ⇒ 不需 ADR ⇒ ⇒ 但**不**在本批顺手改头文件）

## C4 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **238** · `-L verilator_e2e` **70** · `-LE perf` **227** ·
      `verify_dual_backend_parity.sh` **61 / 271** · `tests/test_*.cpp` **132**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目）
- [x] 全量重建后 `ctest -LE perf` **227/227** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0（含命名诚实性自检）
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
