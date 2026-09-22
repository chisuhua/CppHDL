# Design: verilator-perf-three-way

## Context

`docs/AGENTS.md` 三路 perf 对比 (interpreter / JIT / Verilator) 是 CppHDL 性能契约的核心 CI 门禁。当前实现完整存在于 `tests/benchmark/perf_main.cpp` + `tests/benchmark/verilator_runner.h`,但三路对比**实际未生效**,导致 Verilator 后端被永久标记为"实验性",CI 无法捕获 verilator 集成回归。

**问题清单**(proposal.md 详细列出 3 项):

1. **Verilator 二进制查找默认依赖 PATH**(`perf_main.cpp:623` `std::string verilator_bin = "verilator";`)。CI / 本地用户在第三方安装路径 (`build/verilator-install/bin/verilator`) 下运行 `perf_tests --all` 时,runner 调 `popen("verilator --version")` 失败,最终所有 9 个 verilator 行 status=`UNSUPPORTED`。
2. **TC-07 depth=1000 XOR chain 触发 Verilator 编译错误**:`%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`。根因在 `src/codegen_verilog.cpp:141` `get_literal_str()` **未对 `value` 做宽度 mask**;实际生成字面量宽度 (`val.bv_.size()`) 已是目标宽度(实测 `litimpl` 用 `value.bitwidth()` 构造,`lit_node->size() == val.bv_.size()`),但当 `ch_uint<8>(i)` 中 `i` 可达 999 时,`8'h3e7` 因 hex digit 数量超 8-bit 容量被 Verilator 语法拒绝。
3. **`perf_three_way` 入口范围矛盾**:`tests/benchmark/CMakeLists.txt` 的 `add_test(NAME perf_tests ...)` 调用 `perf_tests --all`(实测 ~930s),没有只跑 TC-07/08 子集的快速门禁;早期草稿同时提议 `--all` + `TIMEOUT 240`,必超时。

## Goals / Non-Goals

**Goals:**
- 让 `perf_tests` 在 `BUILD_VERILATOR=ON` 编译时**自动**接收 `--verilator=${CPPHDL_VERILATOR_WRAPPER}`,消除 PATH 依赖
- 修复 `get_literal_str()` 字面量 mask 缺失:`ch_uint<N>(literal)` 产生的 Verilog 字面量值 MUST 在 `N` 位内(避免触发 Verilator "Too many digits" 语法错误)
- 新增独立 ctest 测试 `perf_three_way`,**仅跑 TC-07/08** 的三路 subprocess 隔离对比,`TIMEOUT 240`,可在 CI 快速门禁验证 Verilator 集成健康
- 提供 Catch2 单元测试 `test_verilator_three_way.cpp`,**不依赖 verilator 二进制**,纯 codegen 端到端断言(生成 verilog 字符串后检查字面量形态)
- 不破坏任何现有合法构造路径(不引入 width-trunc 回归)

**Non-Goals:**
- 不修改 `verilator_runner.h` 的 SHA-1 cache 机制(已稳定)
- 不修改 `perf_main.cpp` 的 F2 subprocess 隔离架构(已通过 `fix-perf-subprocess-isolation` 落地)
- 不修改 Verilator 后端运行时接口(`include/core/verilator_backend.h` 已稳定)
- 不修改 `docs/AGENTS.md` 三路 perf 规范(契约不变,只是落实执行链路)
- 不修复 `get_literal_str` 在 `target_width > 64` 的场景(受 ADR-035 Phase 3 wide-signal rejection 限制;64-bit 上限已强制)
- **不改 `get_literal_str` 签名**(实测 `lit_node->size() == val.bv_.size()`,单参版已正确)

## Decisions

### Decision 1: `get_literal_str` 单参版内部 mask,不引入新重载

**实现**(`src/codegen_verilog.cpp:141`):

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

**关键设计权衡**:
- **不改签名,不加重载**:实测 `litimpl` 构造用 `value.bitwidth()`,故 `lit_node->size() == val.bv_.size()`。调用方传 `rhs_node->size()` 或 `node->size()` 与传 `lit_node->size()` 在常规 codegen 路径下都 == `val.bv_.size()`,新重载等价于 no-op。Oracle 与 Metis 独立实测印证。
- **不标 `[[deprecated]]`**:旧 1-arg 版无内部调用方迁移需求(签名不变),`[[deprecated]]` 不适用。
- **不引入 `set_source_files_properties(... COMPILE_OPTIONS "-Wno-...")`**:本项目 CMake 零使用该模式;零内部 deprecated 调用方 = 零警告。
- **mask 在 width >= 64 时退化为原行为**:与 ADR-035 64-bit IO 端口上限一致;codegen 路径中 `target_width > 64` 不会出现。
- **mask 在 width == 0 时返回 0**:防御性处理,避免 `(1<<0)-1` 未定义。

### Decision 2: 调用点不动

实测 `src/codegen_verilog.cpp:709` `print_binary_op` / L845/849 `print_concat` 中 `get_literal_str` 调用全部使用 `lit_node->value()`,单参版签名不变 → **零调用方需要修改**。

### Decision 3: CMake 自动注入 verilator 路径 + 新增 `perf_three_way` 子集 ctest

**当前实现**(`tests/benchmark/CMakeLists.txt:73-99`):

```cmake
if(CPPHDL_VERILATOR_WRAPPER)
    add_test(NAME perf_tests COMMAND perf_tests --all
        --verilator=${CPPHDL_VERILATOR_WRAPPER}
        --report=json --report=csv --report=md)
    set_tests_properties(perf_tests PROPERTIES
        LABELS "perf" TIMEOUT 1800
        ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    add_test(NAME perf_tests COMMAND perf_tests --all ...)
    set_tests_properties(perf_tests PROPERTIES LABELS "perf" TIMEOUT 1800 ...)
endif()
```

**新增**(`perf_tests` 之后):

```cmake
# 独立的三路 perf ctest 入口 — 仅跑 TC-07/08 + subprocess 隔离,
# 用于 CI 快速门禁 Verilator 集成健康(vs 完整 perf_tests 15 分钟)。
# TIMEOUT 360s 基于 TC-07/08 子集实测 ~291s × 1.24 余量。
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
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

**关键设计权衡**:
- **不复用现有 `perf_tests`**:`perf_tests --all` 走 ~930s,`perf_three_way` 用 `--tc=07 --tc=08` 子集(perf_main 支持多次独立 `--tc=` flag,`perf_main.cpp:644-652`)。**注意**:`--tc=07,08` 逗号语法不存在,会导致 `unknown argument` 退出。
- **`TIMEOUT 360`**:基于 TC-07/08 子集(三路 × 三 sizes × subprocess 隔离 + verilator 缓存冷启动)实测 ~291s,留 1.24× 余量。**不**用 `--all` + `TIMEOUT 1800`,后者会失去"快速门禁"意义。
- **`ENVIRONMENT "VERILATOR_ROOT=..."` 在 `perf_three_way` 中不强制注入**:wrapper 在 `VERILATOR_ROOT` 未设置时会自设(`third_party/verilator/bin/verilator:101`),注入反而在 `${CPPHDL_VERILATOR_DIR}` 与 realpath 不一致时触发 wrapper `%Error` 失败模式。注入对 `perf_tests` 已存在(`belt-and-suspenders`),`perf_three_way` 复用 wrapper 自设。
- **`BUILD_VERILATOR=OFF` 仍注册 `perf_three_way`**:verilator 行自然 `status=UNSUPPORTED`(与现有 K2 行为一致),保持 ctest 列表稳定。

### Decision 4: 测试覆盖范围(unit + integration)

**新增 `tests/test_verilator_three_way.cpp`**(约 120 行,~5 个 TEST_CASE):

1. `TEST_CASE("ch_uint<8>(256) generates 8'h0 literal (mask)", "[verilog][verilator]")` — RED 旧版:`8'h100`;NEW:`8'h0`。
2. `TEST_CASE("ch_uint<8>(999) generates 8'he7 literal (mask)", "[verilog][verilator]")` — RED 旧版:`8'h3e7`;NEW:`8'he7`。这是 TC-07 depth=1000 的关键 case。
3. `TEST_CASE("ch_uint<8>(0) and ch_uint<8>(1) still emit cleanly", "[verilog][verilator]")` — 小值不应被 mask 破坏。
4. `TEST_CASE("TC-07 depth=1000 XOR chain produces no 8'h<N+> literal", "[verilog][verilator]")` — 端到端:`ch::toVerilog()` 生成 1000 个 XOR 后,断言不含 `8'h` 后跟 3+ hex digits(末尾 H 可允许)。
5. `TEST_CASE("perf_three_way emits non-UNSUPPORTED verilator rows when CPPHDL_VERILATOR_WRAPPER is set", "[perf][verilator][slow]")` — 端到端:`./build/tests/benchmark/perf_tests --tc=07 --tc=08 --verilator=...` 跑完检查 `perf_results.json` 中 verilator 行至少 1 个 PASS。

**Catch2 标签策略**:核心测试 `[verilator]`,slow 集成测试 `[slow]`,允许 `ctest -LE slow` 跑快速子集。

## File Impact

| 文件 | 改动 |
|------|------|
| `src/codegen_verilog.cpp` | +3 行(mask 计算,`get_literal_str` 内部) |
| `include/codegen_verilog.h` | 0 改动 |
| `tests/benchmark/CMakeLists.txt` | +18 行(新增 `perf_three_way` ctest + LABELS + TIMEOUT) |
| `tests/test_verilator_three_way.cpp` | **新建** ~120 行(5 个 TEST_CASE) |
| `tests/CMakeLists.txt` | +2 行(注册 `test_verilator_three_way`) |
| `openspec/specs/perf-test-isolation/spec.md` | +2 REQUIREMENT(archive 时合并) |
| `openspec/specs/verilator-backend-e2e/spec.md` | +1 REQUIREMENT(archive 时合并) |
| 根 `CMakeLists.txt` | **0 改动**(无 `[[deprecated]]` 警告,无需 `set_source_files_properties`) |

## Migration / Compatibility

- **下游用户 API**:无影响(`get_literal_str` 是 private,外部不可见)
- **测试套件**:所有现有 `tests/test_verilog_gen.cpp` 46 个 TEST_CASE **应当继续通过**(mask 对值 < 2^width 的合法 lit 是 no-op;现有测试用例都是 0/1/小整数,值 < 256,不在 mask 范围)
- **CHANGELOG**:在 `docs/CHANGELOG.md`(若不存在则新建)记录:"`get_literal_str` 内部对值做宽度 mask,修复 TC-07 depth=1000 Verilator 'Too many digits' 编译失败;新增 `perf_three_way` ctest 入口(`--tc=07 --tc=08`)"

## Risk Analysis

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| width-trunc 回归(其他 codegen 路径因 mask 行为变化失败) | LOW | HIGH | 现有 `test_verilog_gen.cpp` 46 个 TEST_CASE + `test_verilator_three_way.cpp` 5 个新增 TEST_CASE 全覆盖 |
| `perf_three_way` ctest wall-clock 超时(实测偏差) | LOW | MEDIUM | TIMEOUT 240s = 子集实测 1.6× 余量;实测偏差时单 commit 增量调整 |
| verilator 二进制缺失导致 SKIPPED(不是 PASS) | MEDIUM | LOW | 已设计:verilator 行 status=UNSUPPORTED 是预期(K2 已知行为),不影响其他后端 PASS |
| `VERILATOR_ROOT` 注入路径不一致导致 wrapper exit 1 | LOW | LOW | `perf_three_way` 不注入 `VERILATOR_ROOT`,复用 wrapper 自设;`perf_tests` 注入仍保留(belt-and-suspenders) |

## Validation Plan

1. **Pre-archive validation**:
   - `openspec validate verilator-perf-three-way --strict` 必须通过
   - `cmake --build build -j$(nproc)` 0 error / 0 warning
2. **Post-implementation validation**:
   - `./build/tests/test_verilator_three_way` 全部 5 个 TEST_CASE 通过
   - `./build/tests/test_verilog_gen` 46 个 TEST_CASE 全部仍通过(无回归)
   - `ctest -E "perf_tests|perf_three_way" --output-on-failure` 全部通过
   - `./run_all_ported_tests.sh`(28 个 main() 示例)全部通过
3. **End-to-end verification**:
   - `./build/tests/benchmark/perf_tests --direct --tc=07 --verilator=${CPPHDL_VERILATOR_WRAPPER}`:depth=10/100/1000 全部 OK,verilog/harness 路径存在,verilator 行 status=PASS(不再是 SKIPPED)
   - `./build/tests/benchmark/perf_tests --tc=07 --tc=08 --verilator=${CPPHDL_VERILATOR_WRAPPER}`:子集 < 240s 完成,verilator 行至少 1 个 PASS
   - `ctest -R perf_three_way --output-on-failure` 通过

## Open Questions

无。本次 change 设计基于已观测的根因(mask 缺失) + 已存在的 OpenSpec `perf-test-isolation` capability,无需额外调研。
