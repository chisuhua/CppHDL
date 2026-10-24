## Tasks

> 归档前状态：**8/8 完成**（9 轮对抗审稿的整改已并入下列条目；详见 proposal 的
> 「各轮审稿整改」小节与本 change 的 proposal）。

- [x] ### Task 1: 建护栏脚本（先立尺子，再改代码）
  - `scripts/verify_example_test_wiring.sh`（归属守恒：C0/C1/C2/C3/C3b/C3c）
  - `scripts/verify_example_assertion_quality.sh`（断言质量：C4/C4b —— 2026-10-24
    从 Task 1 拆出，归属后继 change `example-assertion-quality-2026-10`，
    其**规格**（fixture 集 + 跨平台实测）由后继 change 承接）
- [x] ### Task 2: 修 `test_pipeline_minimal` + `test_pipeline_debug`（ch_module → fixture）
- [x] ### Task 3: 修 `test_riscv_tests_soc.cpp`（eval → tick）
- [x] ### Task 4: 修 `test_riscv_tests.cpp`（编码 + 条件 SKIP + 路径可配）
- [x] ### Task 5: 删测试源文件 + target（**8** 条留痕：6 个曾声明过的 CMake target
  + 2 个从未进 CMakeLists 的孤儿源文件 ⇒ 全部登记在 `examples/removed_examples.txt`）
- [x] ### Task 6: 14 个绿 target 注册进 CTest（12 个原 target + 2 个修好的高价值
  orphan：`test_interrupt_flow` / example 版 `test_rv32i_pipeline`；另有 1 个
  `riscv_mini_catch_test_noctest` 只构建不注册 ⇒ 登记在 `examples/disabled_examples.txt`）
- [x] ### Task 7: CI 接入护栏 + `CTEST_TOTAL` 同步（185 → 199）
- [x] ### Task 8: 全量回归 + 归档前 gate
  - `ctest -LE perf` **188/188**；`ctest -L perf` **11/11**（1338 s）
  - 5 个护栏脚本 + `run_all_ported_tests.sh`（28/28）+ `run_all_samples.sh`（35/35）
  - `openspec validate` + `verify_openspec_delta.sh` 均绿
  - 9 轮对抗审稿（第 9 轮 **无 BLOCKER**）
