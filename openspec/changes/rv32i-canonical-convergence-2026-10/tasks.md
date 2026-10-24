## Tasks

> 每个任务对应一个**可验证**的结果；「RED → GREEN → REFACTOR」按 tasks 顺序执行。
> ⚠️ 本 change 的核心约束：**契约先行**（ADR-046 D12）—— delta 与代码改动**同批落地**。

### Task 1: 建立单源化护栏（先立尺子，再改代码）

- [ ] T1.1 新建 `scripts/verify_no_duplicate_rv32i_headers.sh`，含 4 条检查：
      C1 同 basename ≤ 1 / C2 根 tests 无反向 `-I` / C3 根 tests 无 example include /
      C4 零消费者头 ≤ 允许清单（**带期望条数**）
- [ ] T1.2 允许清单初始内容 = `examples/riscv-mini/src/rv32i_pipeline_top.h`（1 条，
      理由：C3 的预定 DUT；失效条件：C3 落地）
- [ ] T1.3 **RED 验证**：故意造一个同名副本 ⇒ 护栏 MUST exit 1；
      故意加一处反向 `-I` ⇒ MUST exit 1；
      故意删掉允许清单里那条的「零消费者」状态 ⇒ MUST exit 1（stale 检查）
- [ ] T1.4 护栏挂进 CI（与既有 5 个护栏同一 job）

### Task 2: 删死头（17 个）

- [ ] T2.1 `git rm` 17 个零消费者头（清单见 proposal §「删死头」）
- [ ] T2.2 验证：全仓 `#include` 反查 + 编译 ⇒ 无任何 TU 引用它们
- [ ] T2.3 保留 `rv32i_pipeline_top.h`（C3 的 DUT，零消费者已登记在 root `AGENTS.md`）

### Task 3: 提升 10 个头到 `include/cpu/`

- [ ] T3.1 `git mv` 9 个到 `include/cpu/pipeline/`（`rv32i_alu` / `rv32i_regfile` /
      `rv32i_pc` / `rv32i_forwarding` / `rv32i_branch_predict` /
      `rv32i_hazard_complete` / `branch_predictor_v2`）
- [ ] T3.2 `git mv` `rv32i_core.h` → `include/cpu/pipeline/hazard_detection_unit.h`
      （**改名**，ADR-046 D8；类名 `HazardDetectionUnit` 与文件名对齐）
- [ ] T3.3 `git mv` `i_cache_prefetch.h` / `d_cache_write_buffer.h` → `include/cpu/cache/`
- [ ] T3.4 **namespace 保持 `riscv`**（ADR-046 D8）⇒ 迁移后逐文件核对
- [ ] T3.5 更新所有消费者的 `#include`（example 侧 3 个 unit test + 5 个 Catch2 test；
      根 `tests/` 侧 4 个 TU）

### Task 4: 收敛 4 组同名双副本

- [ ] T4.1 `rv32i_decoder.h` / `rv32i_pipeline_regs.h` / `rv32i_tcm.h`：
      example 副本删，消费者重指 `include/cpu/pipeline/`
- [ ] T4.2 ⚠️ 这 3 组的差异**仅**是 `using namespace ch::core;` 与 `namespace riscv {`
      的位置 ⇒ lib 版**收窄**了名字可见性 ⇒ 逐 target 编译验证；
      若某个 TU 依赖了全局可见性 ⇒ 在该 TU 内**显式**补 `using namespace ch::core;`
      （MUST NOT 回退到「保留 example 副本」）
- [ ] T4.3 `hazard_unit.h`：example 副本删；3 个消费者改装配为
      「`HazardUnit`（检测）+ `chlib::ForwardingMux`（数据 mux）」
- [ ] T4.4 ⚠️ mux 拆解 MUST **保持断言语义**：原 `REQUIRE(rs1_data == 42)`（前推后的数据）
      ⇒ 改为读 `ForwardingMux` 的 `alu_input_a`，**期望值不变**
- [ ] T4.5 验证：`chlib::ForwardingMux` 的档位语义与原 mux 逐档等价
      （0→REG / 1→EX / 2→MEM）

### Task 5: 消除反向依赖

- [ ] T5.1 删根 `tests/CMakeLists.txt` 的 6 处 `target_include_directories(... examples/riscv-mini/src)`
- [ ] T5.2 改写 6 个根测试 TU 的 `#include`（`test_rv32i_pipeline` /
      `test_rv32i_pipeline_behavior` / `test_blt_debug` /
      `tests/chlib/test_branch_predictor_v2` / `tests/chlib/test_hazard_complete` /
      `tests/chlib/test_phase4_integration`）
- [ ] T5.3 `tests/test_rv32i_pipeline_behavior.cpp` 的 `hazard_unit.h` include 是**多余**的
      （该文件测的是 `ForwardingUnit` / `ControlHazardUnit` / `HazardDetectionUnit`）⇒ 直接删
- [ ] T5.4 example 侧 `CMakeLists.txt:19-23` 的「include 顺序敏感」注释**作废** ⇒ 删

### Task 6: 全量回归

- [ ] T6.1 `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] T6.2 `ctest -LE perf` **188/188** 全绿（2026-10-24 基线）
- [ ] T6.3 `ctest -L verilator_e2e` 全绿（基线 34）
- [ ] T6.4 `ctest -L perf` 全绿（11/11）
- [ ] T6.5 5 个既有护栏脚本 + `run_all_ported_tests.sh`（28/28）+ `run_all_samples.sh`（35/35）
- [ ] T6.6 新护栏 `scripts/verify_no_duplicate_rv32i_headers.sh` 绿

### Task 7: 文档同步

- [ ] T7.1 `include/cpu/AGENTS.md`：STRUCTURE / KEY FILES / NAMESPACE MAP 与磁盘一致；
      删幽灵条目；修正自述计数
- [ ] T7.2 root `AGENTS.md`：STRUCTURE / NOTES 的 `cpu/` 条目同步
- [ ] T7.3 `examples/riscv-mini/AGENTS.md`：同步 example 侧收敛后的事实
- [ ] T7.4 ADR-046 §5 回填 `executed`；D3 #14 补「mux 拆到 `chlib::ForwardingMux`」的落地形态
- [ ] T7.5 root `AGENTS.md` 的 KNOWN LIMITATIONS：`rv32i_pipeline_top.h` 零消费者的
      条目 MUST 保留并指向 C3

### Task 8: 归档前 gate

- [ ] T8.1 `openspec validate rv32i-canonical-convergence-2026-10` 绿
- [ ] T8.2 `bash scripts/verify_openspec_delta.sh rv32i-canonical-convergence-2026-10` 绿
- [ ] T8.3 对抗式审稿（critic）通过
- [ ] T8.4 `openspec archive` 合并主 spec + 手写补写 `## Purpose`（归档不会改它）
