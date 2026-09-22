# verilator-issue-25-fix

修复 VerilatorBackend `eval/sync` 路径 — 让 Vtop 的组合 wire 输出与 sequential reg 更新正确传播到 Simulator 的 `data_map_`,解锁 AXI4-Lite e2e 数据完整性、Counter 50-cycle REQUIRE、所有顺序逻辑验证。