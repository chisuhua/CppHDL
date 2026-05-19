## 1. 修复 bundle 层文件（按依赖顺序）

- [x] 1.1 修复 `include/bundle/fragment.h` — 分析实际使用的 ch::core 符号并替换
- [x] 1.2 修复 `include/bundle/stream_bundle.h` — 分析实际使用的 ch::core 符号并替换
- [x] 1.3 修复 `include/bundle/flow_bundle.h` — 分析实际使用的 ch::core 符号并替换
- [x] 1.4 修复 `include/bundle/common_bundles.h` — 分析实际使用的 ch::core 符号并替换
- [x] 1.5 修复 `include/bundle/axi_lite_bundle.h` — 分析实际使用的 ch::core 符号并替换
- [x] 1.6 修复 `include/bundle/axi_bundle.h` — 分析实际使用的 ch::core 符号并替换

## 2. 修复其他文件

- [x] 2.1 修复 `include/ast/instr_mem.h` — 分析实际使用的 ch::core 符号并替换
- [x] 2.2 修复 `include/chlib/assert.h` — 分析实际使用的 ch::core 符号并替换

## 3. 验证

- [x] 3.1 运行 `cmake --build build` 验证编译通过
- [x] 3.2 运行 `ctest --output-on-failure` 验证现有测试无回归
- [x] 3.3 确认 git 工作区干净（如有未跟踪变更需提交）