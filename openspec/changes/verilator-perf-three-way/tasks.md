# Tasks: verilator-perf-three-way

## 1. 分析与准备

- [x] 1.1 调研 `get_literal_str` 所有 4 个调用点（精确决定需要修的位置）
      ✅ 通过 grep + read 确认：仅 3 处需要迁移（`print_binary_op` line 709、`print_concat` line 845/849）；4 处 `static_cast<uint64_t>` 直接输出整数的代码（bit-select 索引 / bits-extract range / rotate_l amount / bitsupdate range）不在改造范围
      ✅ 已在 design.md Decision 2 中明确列出
- [x] 1.2 确认 CMake `CPPHDL_VERILATOR_WRAPPER` 注入路径
      ✅ `CMakeLists.txt:333` 设置 `CPPHDL_VERILATOR_WRAPPER` cache 变量
      ✅ `tests/benchmark/CMakeLists.txt:73-89` 已在 `add_test(NAME perf_tests ...)` 中注入
      ✅ 缺口：没有独立的 `perf_three_way` ctest 入口
- [x] 1.3 实测 TC-07 depth=1000 XOR lit 宽度 bug
      ✅ `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=$(which verilator)` 输出：`%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`
      ✅ 根因确认：`get_literal_str` 用 `val.bv_.size()` (字面量存储宽度，常 32 位) 而非 `rhs_node->size()` (目标 wire 宽度，常 8 位)

## 2. 实现 `get_literal_str` 重载 + 修复 codegen 字面量宽度

- [ ] 2.1 在 `include/codegen_verilog.h` 添加新重载声明 + 旧 API `[[deprecated]]` 标注
      📝 第 47 行附近：
      ```cpp
      // 旧 API（内部保留，标 deprecated 防止新调用方）
      [[deprecated("Use the 2-arg overload that takes target_width")]]
      std::string get_literal_str(const ch::core::sdata_type &val) const;
      // 新 API（推荐）
      std::string get_literal_str(uint32_t target_width,
                                  const ch::core::sdata_type &val) const;
      ```
- [ ] 2.2 在 `src/codegen_verilog.cpp` 实现新重载（接收 `target_width`）
      📝 在原 `get_literal_str`（line 141-159）之后新增：
      ```cpp
      std::string verilogwriter::get_literal_str(
          uint32_t target_width, const ch::core::sdata_type &val) const {
          try {
              uint64_t value = static_cast<uint64_t>(val);
              if (target_width == 1) {
                  return (value ? "1'b1" : "1'b0");
              }
              std::stringstream ss;
              ss << target_width << "'h" << std::hex << value;
              return ss.str();
          } catch (...) {
              return "1'b0";
          }
      }
      ```
      📝 旧 1-arg 版本保持不变（继续走 `val.bv_.size()`，保证现有调用方兼容）
- [ ] 2.3 迁移 `print_binary_op` 调用点（line 709）到新重载
      📝 修改：
      ```cpp
      // 旧：rhs_name = get_literal_str(lit_node->value());
      // 新：
      rhs_name = get_literal_str(rhs_node->size(), lit_node->value());
      ```
      📝 **关键决策**：用 `rhs_node->size()` 而非 `node->size()` —— RHS 字面量上下文必须匹配 RHS wire 宽度，而非结果节点（结果节点在 ADD 等饱和场景可能更宽）
- [ ] 2.4 迁移 `print_concat` 调用点（line 845, 849）到新重载
      📝 两处都改为 `node->size()`：
      ```cpp
      rhs_name = get_literal_str(node->size(), lit_node->value());
      // ...
      lhs_name = get_literal_str(node->size(), lit_node->value());
      ```
      📝 Concat 中 LHS/RHS 字面量宽度 = 整个 concat 输出宽度（assign 截断由 verilog 规则处理）
- [ ] 2.5 抑制内部 `[[deprecated]]` 警告
      📝 在 `src/codegen_verilog.cpp` 顶部（或仅 codegen_verilog.cpp 这个 TU）添加编译选项：
      ```cmake
      # CMakeLists.txt:src/codegen_verilog.cpp 部分添加
      set_source_files_properties(src/codegen_verilog.cpp PROPERTIES
          COMPILE_OPTIONS "-Wno-deprecated-declarations")
      ```
      📝 避免内部 4 个调用点产生 -Wdeprecated-declarations 警告污染 CI

## 3. 注册 ctest 入口 `perf_three_way`

- [ ] 3.1 在 `tests/benchmark/CMakeLists.txt` 新增 `perf_three_way` ctest
      📝 紧跟现有 `add_test(NAME perf_tests ...)` 之后（约 line 92 之后）添加：
      ```cmake
      # 独立的三路 perf ctest 入口 — 仅跑 TC-07/08 + subprocess 隔离，
      # 用于 CI 快速门禁 Verilator 集成健康（vs 完整 perf_tests 15 分钟）。
      # TIMEOUT 240s = 实测 ~150s (含 verilator build cache 首次) × 1.6 余量。
      if(CPPHDL_VERILATOR_WRAPPER)
          add_test(NAME perf_three_way COMMAND perf_tests --all
              --verilator=${CPPHDL_VERILATOR_WRAPPER}
              --report=json)
      else()
          add_test(NAME perf_three_way COMMAND perf_tests --all
              --report=json)
      endif()
      set_tests_properties(perf_three_way PROPERTIES
          LABELS "perf;verilator"
          TIMEOUT 240
          ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
      ```
- [ ] 3.2 在 `tests/CMakeLists.txt` 中确保 root ctest 可发现 `perf_three_way`
      📝 现有 `perf_tests` 是 `tests/benchmark/` 子目录的 ctest，root `tests/CMakeLists.txt` 不需要新增条目（CTest 会自动枚举子目录）
      📝 但需要在 `tests/CMakeLists.txt` 添加一个**纯 codegen 单元测试**注册（见 Task 4）

## 4. 注册 codegen 单元测试 `test_verilator_three_way.cpp`

- [ ] 4.1 创建 `tests/test_verilator_three_way.cpp`
      📝 **5 个 TEST_CASE**（参考 design.md Decision 4）：
      - `get_literal_str: 8-bit target uses 8'h prefix` — RED：旧版返回 `32'h109`；NEW：返回 `8'h109`
      - `get_literal_str: 8-bit target truncates value` — `ch_uint<8>(256)` → `8'h100`
      - `get_literal_str: 1-arg deprecated overload still works` — 兼容性证明
      - `TC-07 depth=1000 XOR chain compiles via verilator --lint-only` — `[slow]` 集成测试，verilator 缺失时 SKIP
      - `perf_main --all emits non-UNSUPPORTED verilator rows` — `[slow]` 端到端验证
- [ ] 4.2 在 `tests/CMakeLists.txt` 注册
      📝 紧跟 `add_catch_test(test_bundle_literal ...)` 之后添加：
      ```cmake
      # verilator-perf-three-way: codegen 字面量宽度修复 + 三路 perf ctest
      add_catch_test(test_verilator_three_way test_verilator_three_way.cpp)
      ```

## 5. 验证测试

- [ ] 5.1 编译验证
      📝 `cmake --build build -j$(nproc)` 无 error/warning
      📝 `[[deprecated]]` 警告已在 codegen_verilog.cpp TU 内禁言（不影响其他文件）
- [ ] 5.2 新增 codegen 单元测试
      📝 `./build/tests/test_verilator_three_way` 全部 5 个 TEST_CASE 通过
      📝 特别注意第 1 个 TEST_CASE 必须 RED → GREEN（验证 get_literal_str 新重载生效）
- [ ] 5.3 现有 codegen 测试无回归
      📝 `./build/tests/test_verilog_gen` 46 个 TEST_CASE 全部仍通过
      📝 `./build/tests/test_verilator_backend` 全部仍通过（如果存在且使用 codegen）
- [ ] 5.4 全测试套件
      📝 `ctest -E perf_tests --output-on-failure` 所有测试通过（排除 perf_tests 全套，单独跑 perf_three_way）
      📝 `ctest -R perf_three_way --output-on-failure` 通过
- [ ] 5.5 端到端 Verilator 验证（手动）
      📝 `rm -rf build/perf_work` + `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=$(which verilator)` —— depth=1000 必须 OK（之前 FAIL）
      📝 `./build/tests/benchmark/perf_tests --all --verilator=$(which verilator)` —— 至少 6 个 verilator 行 status=PASS（TC-09/TC-11 仍未配 harness 保持 SKIPPED 是已知约束）
- [ ] 5.6 示例程序验证
      📝 `./run_all_ported_tests.sh`（28 个 main() 示例）全部通过
      📝 关键：`samples/counter.cpp` 和 AXI 示例不能因 get_literal_str 改动出现 codegen 回归

## 6. 文档与归档

- [ ] 6.1 AGENTS.md / usage-guide 同步
      📝 `docs/AGENTS.md` 现有"Verilator 三路 perf 对比"段：把"`perf_three_way` ctest 入口（`ctest -L verilator`）"和"perf_tests 自动检测 verilator 二进制路径（无需手动 PATH）"作为新行为记录
- [ ] 6.2 CHANGELOG / release notes
      📝 记录 3 项行为变更：
        - perf_tests 自动接收 `--verilator=${CPPHDL_VERILATOR_WRAPPER}`（无 PATH 依赖）
        - 新增 `perf_three_way` ctest 入口（`ctest -L verilator`）
        - 修复 `ch_uint<N>(literal)` 生成的 Verilog 字面量宽度（之前误用 bv 存储宽度，导致 TC-07 depth=1000 verilator 编译失败）
- [ ] 6.3 OpenSpec 校验
      📝 `openspec validate verilator-perf-three-way --strict` pre-archive 通过
      📝 `openspec validate --specs --strict` 通过（archive 时合并到 perf-test-isolation + verilator-backend-e2e 后）
- [ ] 6.4 提交
      📝 **3 个 atomic commit**（按 git-master 规则，test + impl 配对）：
        - **commit 1**: `fix(codegen): use target_width for lit emission in binary ops + concat`
          - `include/codegen_verilog.h`（新重载声明 + deprecation）
          - `src/codegen_verilog.cpp`（新重载实现 + 3 处调用迁移）
          - `CMakeLists.txt`（codegen_verilog.cpp TU 内 -Wno-deprecated-declarations）
        - **commit 2**: `test(verilog): add codegen unit tests for literal width fix`
          - `tests/test_verilator_three_way.cpp`（新建）
          - `tests/CMakeLists.txt`（注册）
        - **commit 3**: `test(benchmark): add perf_three_way ctest entry`
          - `tests/benchmark/CMakeLists.txt`（新增 perf_three_way ctest）
      📝 每个 commit 配套 Sisyphus footer + Co-authored-by（与 AGENTS.md 现有 commit 风格一致）
- [ ] 6.5 运行 `openspec archive verilator-perf-three-way`
      📝 archive 把 `specs/perf-test-isolation/spec.md` + `specs/verilator-backend-e2e/spec.md` 合并到对应 `openspec/specs/...`
      📝 archive 前再次确认 `openspec validate --specs --strict` 通过

## 7. 跟进项（out of scope for this change）

- [ ] 7.1 TC-09 (ch_uint<32> arith) verilator harness 实现（与本 change 独立）
- [ ] 7.2 TC-11 (ch_uint<256> wide reg) verilator harness 实现（与本 change 独立，且受 ADR-035 wide-signal rejection 限制 — 64-bit 上限冲突，需单独 ADR）
- [ ] 7.3 K1 ORC JIT cross-DUT state pollution 真正根因定位（独立调研，标记在 `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`）
