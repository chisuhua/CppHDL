# Tasks: dual-backend-rv32i-integration-2026-10

> TDD 5 步结构（红 → 绿 → 精化 → 变异 → 回归）。每步 ≤ 2-5 分钟可执行。
> **无 spec 变更** ⇒ 归档用 `--skip-specs`（`.openspec.yaml` 已声明）。

## 0. 准备

- [x] 0.1 读码确认「clk-spike 不成立」：`IfIdPipelineReg::describe()`
      **从不读** `io().clk`（`rv32i_pipeline_regs.h:203-227`）⇒ 死端口
- [x] 0.2 读码确认 `BranchPredictor` 纯组合（无 clk/rst、无 `ch_reg`）
- [x] 0.3 确认隐式时钟的 dual 能力已有先例（`tests/test_dual_backend_clock.cpp`）
- [x] 0.4 确认宿主文件 `tests/test_rv32i_pipeline_behavior.cpp` 的用法风格
      （`DUAL_BACKEND_TEST_PAIR(Dut, "标题", "[tags]", [&]() { … })`）
- [x] 0.5 确认 `tests/CMakeLists.txt:565` 已用 `add_dual_backend_test` 注册该 target
      ⇒ **不新建 target ⇒ 计数不变**
- [x] 0.6 手推 3 个用例的金标（design.md §4）⇒ 期望值**不来自**仿真输出

## 1. 红（先写断言，让它失败或暴露问题）

- [x] 1.1 用例 1：`IfIdPipelineReg` 装载 PC 与指令
      —— 断言 `id_pc==0x100` / `id_instruction==0x00100093` / `id_instr_valid==1`
- [x] 1.2 用例 2：`IfIdPipelineReg` 在 flush 时清 valid（**只清 valid**）
      —— 断言 `id_instr_valid==0` **且** `id_pc==0x100`（未变）
- [x] 1.3 用例 3：`BranchPredictor` BNT
      —— 断言 `is_branch==1` / `branch_taken==1` / `predict_error==1` / `branch_target==0x100`
- [x] 1.4 三个用例的**反向期望**写进代码注释（`X_old != X_new`）

## 2. 绿（实现到位）

- [x] 2.1 3 个 `DUAL_BACKEND_TEST_PAIR` 追加到 `tests/test_rv32i_pipeline_behavior.cpp` 末尾
      （**既有 13 个 Batch 1 用例零改动**）
- [x] 2.2 `IfIdPipelineReg` 的 **7 个输入端口逐个显式驱动**（含死端口 `clk`）
      ⇒ `rst=0`（该组件 `rst` **高有效**）、`clk=1`（**只置 1，不做 toggle**）
- [x] 2.3 `BranchPredictor` 的 4 个输入端口逐个显式驱动
- [x] 2.4 `sim.tick()` 一次 = 一拍（**MUST NOT** 照抄原用例的两次手动 clk toggle）
- [x] 2.5 构建：`cmake --build build -j$(nproc) --target test_rv32i_pipeline_behavior`
- [x] 2.6 跑：`ctest -R test_rv32i_pipeline_behavior --output-on-failure` ⇒ 绿
      （**既有 13 个 Batch 1 用例仍绿** ⇒ 追加未破坏基线）

## 3. 精化（判别力自检）

- [x] 3.1 确认每个断言**有判别力**（不是恒真 / 不是初始值巧合）
- [x] 3.2 确认用例 2 的「`id_pc` MUST 仍是 0x100」能抓到「flush 清一切」的误实现
- [x] 3.3 确认用例 3 的 `predict_error` 不是恒 0 的巧合（BNT 语义推导在注释里留痕）

## 4. 变异验证（每条**先确认编译成功**）

- [x] 4.1 **M1**：`valid_reg->next` 的 flush 分支反向 ⇒ 用例 2 转红 ✅（2 failed，`REQUIRE(id_valid==0)`）
- [x] 4.2 **M2**：`predict_error` 改成恒 0 ⇒ 用例 3 转红 ✅（2 failed）
- [x] 4.3 **M3**：漏驱动 `if_instr_valid` ⇒ ✅ **转红**（2 failed ⇒ 未驱动端口两后端都读到 0 ⇒ 断言有判别力）
      （实测**抓到了** ⇒ 不需要补判别断言；见 design.md §5.1）
- [x] 4.4 每条变异后 MUST **重建**（`cmake --build`）再跑 ⇒ 否则跑的是旧二进制 ⇒ 假绿
      ✅ M3'（第 1 轮审稿 BLOCKER 的变异：删掉 Batch 2-b 第 1 拍前的 `flush=false`）
      ⇒ 实测**零判别力**（仍全绿）⇒ 如实记录「纪律 ≠ 可判别性」（design.md §5.2）
- [x] 4.5 变异全部还原 ⇒ **重建** ⇒ 复跑全绿（确认 build 目录里没有变异二进制）

## 5. 回归与工程

- [x] 5.1 `ctest -LE perf` **191/191 全绿**（2026-10-27 实测，1008.8 s）
- [ ] 5.2 `ctest -L perf` 全绿（11/11）
- [x] 5.3 `run_all_ported_tests.sh` **28/28**
- [x] 5.4 `run_all_samples.sh` **35/35**
- [x] 5.5 8 个 `scripts/verify_*.sh` 全 exit 0（含 `verify_test_counts.sh`）
- [x] 5.6 三个计数**不变**（实测 202 / 37）：`ctest -N` = 202 / `ctest -N -L verilator_e2e` = 37 /
      `EXAMPLES_TEST_SOURCES` = 17
- [x] 5.7 `openspec validate --specs --strict` 不因本 change **新增**红（本 change 无 spec 变更 ⇒ 无新增）
      （⚠️ CLI 1.14.3 比仓库钉的 1.4.1 多两条 warning 规则 ⇒ 以「前后无新增」为准）

## 6. 文档与簿记（与实现同批）

- [x] 6.1 `tests/test_rv32i_pipeline_behavior.cpp` 的**文件头** Batch 清单更新
      （Batch 2/3 由「待做」改为「已做」；**文件头描述 MUST 与实现一致**）
- [x] 6.2 `.rddf/roadmap/features/feat-dual-backend-coverage.md`：Phase 4 🟡 → ✅
- [x] 6.3 `.rddf/roadmap/features/feat-rv32i-integration.md`：最后一条验收勾选
- [x] 6.4 `.rddf/roadmap/objectives/objective-rv32i-e2e.md`：Go 判据第 5 条勾选
      ⇒ 6/6 ⇒ **objective 转 `completed`**（⚠️ 逐条复核 §9.2 的 6 条后再转）
- [x] 6.5 `.rddf/roadmap.md`：sprint 表 / Unmapped 计数
- [x] 6.6 根 `AGENTS.md §NOTES`：本 change 的记录（计数不变的原因 + 变异结果）
- [x] 6.7 根 `AGENTS.md` 的 `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` **不动**
      （本 change 三个计数均不变 ⇒ 锚点行**零改动**）

## 7. 审稿与归档

- [x] 7.1 对抗审稿（critic，`review_profile: build`）⇒ **2 轮**（第 1 轮 REJECT：1 BLOCKER + 1 SF + 1 NIT；第 2 轮 **APPROVE**）
- [x] 7.2 审稿处置：BLOCKER（Batch 2-b 第 1 拍漏驱动 `flush`）**已修**；1 SF + 3 NIT 全 CLOSED
- [x] 7.3 `openspec archive dual-backend-rv32i-integration-2026-10 --yes --skip-specs` ✅
      （⚠️ `skip_specs` 合法：本 change **无** spec 变更，`.openspec.yaml` 已显式声明）
- [x] 7.4 归档后：`.rddf/roadmap` / 根 `AGENTS.md` 的最终状态核对
      （`openspec list` ⇒ 无活跃 change）
