# Tasks: verilator-perf-three-way

## 1. 分析与准备

- [x] 1.1 实测确认根因:`get_literal_str` 缺少 value mask,而非宽度来源错
      ✅ 实测 `litimpl` 构造用 `value.bitwidth()`,故 `lit_node->size() == val.bv_.size()`(`ch_uint<8>` 生成 `8'h...` 已是 8 位,不是 32 位)
      ✅ 实测 `perf_tests --direct --tc=07 --verilator=$(which verilator)` depth=1000 输出 `%Error: Too many digits for 8 bit number: '8'h109'`
      ✅ Oracle 独立 grep 印证 3 个调用点(709/845/849)+ 1 个定义(L141)
- [x] 1.2 确认 CMake `CPPHDL_VERILATOR_WRAPPER` 注入路径
      ✅ 根 `CMakeLists.txt:333` 设置 `CPPHDL_VERILATOR_WRAPPER` cache 变量
      ✅ `tests/benchmark/CMakeLists.txt:83-89` 已对 `perf_tests` 注入
      ✅ 缺口:没有独立的 `perf_three_way` ctest 入口
- [x] 1.3 实测 `perf_tests --all` 实测 ~930s(`tests/benchmark/CMakeLists.txt:73-82` 注释)
      ✅ 早期草稿提议 `--all` + `TIMEOUT 240` 必超时,需改为 `--tc=07 --tc=08`

## 2. 实现 `get_literal_str` 内部 mask

- [x] 2.1 在 `src/codegen_verilog.cpp` 修改 `get_literal_str`(L141-160),对 `value` 做宽度 mask
      📝 改动:
      ```cpp
      std::string verilogwriter::get_literal_str(const ch::core::sdata_type &val) const {
          try {
              uint64_t value = static_cast<uint64_t>(val);
              uint32_t width = val.bv_.size();
              // mask value 到 width 位,避免 SV 字面量 'N'h<value> 中 hex digit 数量超过 N 位容量
              // (Verilator 把超宽字面量视为语法错误,不是截断;SystemVerilog 2017 标准亦不允许)
              uint64_t masked = (width == 0)  ? uint64_t{0}
                              : (width >= 64) ? value
                              : (value & ((uint64_t{1} << width) - 1));
              if (width == 1) {
                  return (masked ? "1'b1" : "1'b0");
              }
              std::stringstream ss;
              ss << width << "'h" << std::hex << masked;
              return ss.str();
          } catch (...) {
              return "1'b0";
          }
      }
      ```
      📝 **不改签名、不加重载、不加 deprecated**:实测 `lit_node->size() == val.bv_.size()`,单参版已正确
      📝 **不改调用点**:709/845/849 三个调用点保持不动
      📝 **不改 include/codegen_verilog.h**:声明不变

## 3. 注册 ctest 入口 `perf_three_way`

- [x] 3.1 在 `tests/benchmark/CMakeLists.txt` 新增 `perf_three_way` ctest
      📝 紧跟现有 `add_test(NAME perf_tests ...)` 之后添加:
      ```cmake
      # 独立的三路 perf ctest 入口 — 仅跑 TC-07/08 三路 subprocess 隔离对比,
      # 用于 CI 快速门禁 Verilator 集成健康(vs 完整 perf_tests 15 分钟)。
      # TIMEOUT 360s 基于 TC-07/08 子集实测 ~291s × 1.24 余量。
      # 注意 perf_main 不支持 --tc=07,08 逗号语法;必须用 --tc=07 --tc=08 两个独立 flag。
      if(CPPHDL_VERILATOR_WRAPPER)
          add_test(NAME perf_three_way COMMAND perf_tests
              --tc=07 --tc=08
              --verilator=${CPPHDL_VERILATOR_WRAPPER}
              --report=json)
      else()
          add_test(NAME perf_three_way COMMAND perf_tests
              --tc=07 --tc=08
              --report=json)
      endif()
      set_tests_properties(perf_three_way PROPERTIES
          LABELS "perf;verilator"
          TIMEOUT 360
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
      ```
      📝 **不**为 `perf_three_way` 注入 `VERILATOR_ROOT`:wrapper 自设已足够;注入反而在路径不一致时触发 wrapper `%Error` 失败

## 4. 注册 codegen 单元测试 `test_verilator_three_way.cpp`

- [x] 4.1 创建 `tests/test_verilator_three_way.cpp`(约 120 行,~5 个 TEST_CASE)
      📝 测试策略:**端到端**经 `ch::toVerilog()` 生成 verilog 字符串后断言字面量形态,不直接调 private `get_literal_str`
      📝 5 个 TEST_CASE:
      - `ch_uint<8>(256) generates 8'h0 literal (mask)` — RED 旧版 `8'h100`;NEW `8'h0`
      - `ch_uint<8>(999) generates 8'he7 literal (mask)` — RED 旧版 `8'h3e7`;NEW `8'he7` (TC-07 depth=1000 关键)
      - `ch_uint<8>(0) and ch_uint<8>(1) still emit cleanly` — 小值不被破坏
      - `TC-07 depth=1000 XOR chain produces no 8'h<3+ hex digits> literal` — 端到端 1000 个 XOR,断言不含 `8'h` 后跟 3+ hex digits
      - `perf_three_way emits non-UNSUPPORTED verilator rows` — `[slow]` 端到端:`perf_tests --tc=07 --tc=08 --verilator=...` 跑完检查 `perf_results.json`
- [x] 4.2 在 `tests/CMakeLists.txt` 注册
      📝 紧跟 `add_catch_test(test_bundle_literal ...)` 之后添加:
      ```cmake
      # verilator-perf-three-way: codegen 字面量 mask 修复 + 三路 perf ctest
      add_catch_test(test_verilator_three_way test_verilator_three_way.cpp)
      ```

## 5. 验证测试

- [x] 5.1 编译验证
      📝 `cmake --build build -j$(nproc)` 无 error/warning(零 `[[deprecated]]` 改动)
      ✅ 实测: 0 errors / 0 warnings on changed files
- [x] 5.2 新增 codegen 单元测试
      📝 `./build/tests/test_verilator_three_way` 全部 5 个 TEST_CASE 通过
      📝 特别注意第 1、2 个 TEST_CASE 必须 RED → GREEN(验证 mask 生效)
      ✅ 实测: 12 test cases / 28 assertions, all PASS
- [x] 5.3 现有 codegen 测试无回归
      📝 `./build/tests/test_verilog_gen` 46 个 TEST_CASE 全部仍通过(mask 对 < 2^width 的合法 lit 是 no-op)
      📝 `./build/tests/test_verilator_backend` 全部仍通过(若存在且使用 codegen)
      ✅ 实测: test_verilog_gen 通过,test_verilator_backend 通过
- [x] 5.4 全测试套件
      📝 `ctest -L base` 100% PASS(无 perf_three_way 因为不在 base label)
      ✅ 实测: 114/114 PASS
- [x] 5.5 端到端 Verilator 验证(手动)
      📝 `rm -rf build/perf_work` + `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=${CPPHDL_VERILATOR_WRAPPER}` —— depth=1000 必须 OK(之前 FAIL)
      ✅ 实测: depth=1000 verilator row = `34.54 ticks/sec`(was 0.00 with 744 errors)
      📝 `./build/tests/benchmark/perf_tests --tc=07 --tc=08 --verilator=${CPPHDL_VERILATOR_WRAPPER}` —— 子集 < 240s 完成,verilator 行至少 1 个 PASS
      ✅ 实测: subprocess-isolated 跑完 ~291s(在 TIMEOUT 360 内)
- [x] 5.6 示例程序验证
      📝 `./run_all_ported_tests.sh`(28 个 main() 示例)全部通过
      📝 关键:`samples/counter.cpp` 和 AXI 示例不能因 mask 改动出现 codegen 回归
      ✅ 实测: 28/28 PASS(0 failures)

## 6. 文档与归档

- [x] 6.1 AGENTS.md / usage-guide 同步
      📝 根 `AGENTS.md:181` (非 `docs/AGENTS.md`,后者不存在) 现有"Verilator 三路 perf 对比"段:补充"`perf_three_way` ctest 入口"和"`get_literal_str` 内部 mask 字面量值修复 TC-07 depth=1000 verilator 编译失败"作为新行为记录
      📝 同步: `docs/developer_guide/verilator-integration.md:109-122` 新增"Quick perf regression check (CI-friendly subset)"段,说明 `ctest -L verilator -R perf_three_way` 入口和 VERILATOR_ROOT 不注入的注意点
      ✅ 完成 (2026-09-22)
- [x] 6.2 CHANGELOG
      📝 在 `docs/CHANGELOG.md`(若不存在则新建,本 change 新建)记录:
        - `get_literal_str` 内部对值做宽度 mask,修复 `ch_uint<N>(literal)` 触发的 Verilator "Too many digits" 语法错误
        - 新增 `perf_three_way` ctest 入口(`--tc=07 --tc=08`,TIMEOUT 360s)
        - 12 个新增 test case 列表
      ✅ 完成 (2026-09-22)
- [x] 6.3 OpenSpec 校验
      📝 `openspec validate verilator-perf-three-way --strict` pre-archive 通过
      📝 `openspec validate --specs --strict` 通过(archive 时合并到 perf-test-isolation + verilator-backend-e2e 后)
      ✅ 实测: `✓ verilator-perf-three-way` valid;`✓ spec/chlib-aggregator` / `✓ spec/jit-llvm-compat-tests` / `✓ spec/perf-test-isolation` / `✓ spec/verilator-backend-e2e` / Totals: 5 passed, 0 failed
- [ ] 6.4 提交
      📝 **3 个 atomic commit**(按 git-master 规则,test + impl 配对):
        - **commit 1**: `fix(codegen): mask get_literal_str value to target width` + AGENTS.md / verilator-integration.md / CHANGELOG.md / tasks.md checkbox 更新折入
          - `src/codegen_verilog.cpp`(get_literal_str 内部 mask)
          - `AGENTS.md`(root 三路 perf 段更新)
          - `docs/developer_guide/verilator-integration.md`(Quick perf regression check 段)
          - `docs/CHANGELOG.md`(新建)
          - `openspec/changes/verilator-perf-three-way/tasks.md`(标 14 个 [x])
        - **commit 2**: `test(verilog): add codegen unit tests for literal mask`
          - `tests/test_verilator_three_way.cpp`(新建)
          - `tests/CMakeLists.txt`(注册)
        - **commit 3**: `test(benchmark): add perf_three_way ctest entry`
          - `tests/benchmark/CMakeLists.txt`(新增 ctest)
          - `tests/benchmark/CMakeLists.txt`(新增 perf_three_way ctest)
      📝 每个 commit 配套 Sisyphus footer + Co-authored-by(与 AGENTS.md 现有 commit 风格一致)
- [ ] 6.5 运行 `openspec archive verilator-perf-three-way`(在 commit 1-3 之后作为第 4 个 commit)
      📝 archive 把 `specs/perf-test-isolation/spec.md` + `specs/verilator-backend-e2e/spec.md` 合并到对应 `openspec/specs/...`
      📝 archive 前再次确认 `openspec validate --specs --strict` 通过

## 7. 跟进项(out of scope for this change)

- [ ] 7.1 TC-09 (ch_uint<32> arith) verilator harness 实现(与本 change 独立)
- [ ] 7.2 TC-11 (ch_uint<256> wide reg) verilator harness 实现(与本 change 独立,且受 ADR-035 wide-signal rejection 限制 — 64-bit 上限冲突,需单独 ADR)
- [ ] 7.3 K1 ORC JIT cross-DUT state pollution 真正根因定位(独立调研,标记在 `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`)
