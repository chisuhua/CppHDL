# Tasks — Issue #25 VerilatorBackend eval/sync 路径修复

TDD 5-step 结构。每个 task 2-5 分钟。

## 0. 调查与准备

- [ ] 0.1 读 `src/core/verilator_backend.cpp:696-711` `eval_sequential()` 当前实现并确认 Oracle 假设
- [ ] 0.2 读 `src/core/verilator_backend.cpp:349-372` `sync_*_to_vtop` 当前实现
- [ ] 0.3 读 `src/core/verilator_backend.cpp:534-550` `build_port_access_table` 当前实现
- [ ] 0.4 grep `clock_field_ptr_` / `reset_field_ptr_` 在 verilator_backend.cpp/.h 中的引用,确认是否已存在字段
- [ ] 0.5 跑当前 `test_verilator_e2e_harness` + `test_axi_lite_verilator_e2e` 记录 baseline(issue #25 复现证据)
- [ ] 0.6 读 `src/core/verilator_backend.cpp:351-356` `clear()` 方法,确认新增 `clock_field_ptr_`/`reset_field_ptr_` 需要在 `clear()` 中重置为 nullptr

## 1. RED: 强化测试要求

- [ ] 1.1 `tests/test_verilator_e2e_harness.cpp:166` `CHECK` → `REQUIRE(actual == 50)`
- [ ] 1.2 `tests/test_verilator_e2e_harness.cpp:206` `CHECK` → `REQUIRE(actual == 50 % 16)`
- [ ] 1.3 `tests/test_axi_lite_verilator_e2e.cpp:240-249` `INFO` → `REQUIRE(awready_v == 1)` / `REQUIRE(wready_v == 1)` / `REQUIRE(bvalid_v == 1)`
- [ ] 1.4 `tests/test_axi_lite_verilator_e2e.cpp` 新增 `REQUIRE(rdata_val == 0xDEADBEEF)`
- [ ] 1.5 跑测试确认 RED(issue #25 阻断,3-4 个 REQUIRE 失败)

## 2. GREEN: 实现 3 步时钟翻转

- [ ] 2.1 `src/core/verilator_backend.h` 添加 `clock_field_ptr_` / `reset_field_ptr_` 字段(若不存在)
- [ ] 2.2 `src/core/verilator_backend.cpp:534-550` `build_port_access_table` 中识别 `type_clock`/`type_reset`,保存 `clock_field_ptr_` / `reset_field_ptr_` 直接指针(不经过 data_map_)
- [ ] 2.3 `src/core/verilator_backend.cpp:696-711` `eval_sequential` 重写为 3 步时钟序列:clk=0→eval→clk=1→eval→clk=0→eval
- [ ] 2.4 `src/core/verilator_backend.cpp:349-361` `sync_inputs_to_vtop` 添加 `exclude_clock` 参数,默认 true(不覆盖 Vtop 时钟字段)
- [ ] 2.5 `src/core/verilator_backend.cpp:363-372` `sync_outputs_from_vtop` 排除时钟/复位字段
- [ ] 2.6 重新编译,跑 1.5 的测试确认 GREEN

## 3. 回归验证

- [ ] 3.1 `cmake --build build -j$(nproc)` 无 error/warning
- [ ] 3.2 `ctest -L base` 100% PASS(无 regression)
- [ ] 3.3 `./run_all_ported_tests.sh` 28/28 PASS
- [ ] 3.4 `tests/test_verilator_three_way` 12 cases 全部 PASS(无 regression)
- [ ] 3.5 `tests/test_verilator_e2e_harness` 全部 REQUIRE PASS
- [ ] 3.6 `tests/test_axi_lite_verilator_e2e` 全部 REQUIRE PASS
- [ ] 3.7 `perf_three_way` ctest PASS(TC-07/08 verilator 行 ≥ 34 ticks/sec baseline)

## 4. 文档同步

- [ ] 4.1 根 `AGENTS.md` "Verilator 三路 perf 对比"段补充 issue #25 修复说明
- [ ] 4.2 `docs/developer_guide/verilator-integration.md` "Quick perf regression check"段补充 3 步时钟翻转说明
- [ ] 4.3 `docs/CHANGELOG.md` 新增 `2026-09-23 — verilator-issue-25-fix` 条目

## 5. OpenSpec 校验与归档

- [ ] 5.1 `openspec validate verilator-issue-25-fix --strict` PASS
- [ ] 5.2 `openspec validate --specs --strict` PASS(5→6 specs)
- [ ] 5.3 拆分 atomic commits:
  - commit 1: `fix(verilator): 3-step clock toggle in eval_sequential`
  - commit 2: `test(verilator): tighten e2e REQUIRE assertions (issue #25 closure)`
  - commit 3: `docs(changelog): verilator-issue-25-fix entry`
- [ ] 5.4 `openspec archive verilator-issue-25-fix`(第 4 commit)
- [ ] 5.5 `git push origin main`

## 6. 端到端 Verilator 验证(手动)

- [ ] 6.1 `rm -rf build/perf_work` + `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=${CPPHDL_VERILATOR_WRAPPER}` — TC-07 depth=1000 仍 ≥ 34 ticks/sec
- [ ] 6.2 `./build/tests/benchmark/perf_tests --tc=07 --tc=08 --verilator=${CPPHDL_VERILATOR_WRAPPER}` — 子集 < 360s 完成
- [ ] 6.3 跑 `./run_all_ported_tests.sh` 28/28 PASS

## 7. 跟进项(out of scope)

- [ ] 7.1 ADR-035 Phase 4: 64-bit port cap 提升 + TC-09/TC-11 harness(独立 ADR)
- [ ] 7.2 K1 ORC JIT cross-DUT state pollution 真正根因定位(独立调研)
- [ ] 7.3 评估 ChipForge Phase 6d.5 E8(vendored RISC-V ELF)是否可在 issue #25 fix 后启用