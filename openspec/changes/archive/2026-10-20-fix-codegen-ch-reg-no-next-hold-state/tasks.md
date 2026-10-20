## Tasks

- [x] ### Task 1: 修改 `src/codegen_verilog.cpp` else 分支
  - **action**: `else` 分支从 warning comment 改为 emit hold-state always_ff
  - **verify**: 编译通过 + 全量 ctest 不退化
- [x] ### Task 2: CHWARN 增强诊断字段
  - **action**: `CHWARN("[print_reg] ch_reg init_node is not type_lit ...")` 加 size + name
  - **verify**: grep src/codegen_verilog.cpp
- [x] ### Task 3: 验证修复（临时探测试 test_axi_uart_behavior.cpp）
  - **action**: 写 1 个 dual backend 探测试验证 `tx_empty(ch_bool(true))` 后 uart_tx=1
  - **verify**: interpreter half PASS (uart_tx=1) + verilator half PASS (uart_tx=1)
  - **note**: 探测试已撤回（`rm tests/test_axi_uart_behavior.cpp`），fix 验证记录在本 task
- [x] ### Task 4: 全量回归
  - **verify**: `ctest -LE perf --output-on-failure` → 157/157 PASS（baseline 156 + 探测试 1，撤回后 156/156 PASS）
- [x] ### Task 5: openspec validate + change archive
  - **verify**: `openspec validate fix-codegen-ch-reg-no-next-hold-state --strict` → valid
  - **action**: `openspec archive fix-codegen-ch-reg-no-next-hold-state --yes --skip-specs`
  - **verify**: `ls openspec/changes/archive/ | grep ch-reg-no-next-hold-state`