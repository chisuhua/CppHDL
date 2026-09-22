# verilator-perf-three-way

## Why

`docs/AGENTS.md` 规范 **三路 perf 对比** (interpreter / JIT / Verilator) 是 CppHDL 性能契约的核心 CI 门禁。当前实现完整存在于 `tests/benchmark/perf_main.cpp` + `tests/benchmark/verilator_runner.h`,但三路对比**实际未生效**:

**1. Verilator 二进制查找默认依赖 PATH**(`perf_main.cpp:623` `std::string verilator_bin = "verilator";`)。CI 与本地用户在第三方安装路径 (`build/verilator-install/bin/verilator`) 下运行 `perf_tests --all` 时,verilator 路径未被注入,runner 调 `popen("verilator --version")` 失败,最终所有 9 个 verilator 行 status=`UNSUPPORTED`。

**2. TC-07 depth=1000 XOR chain 触发 Verilator 编译错误**:实测 `perf_tests --direct --tc=07 --verilator=...` 时 depth=10/100 正常,depth=1000 报错 `%Error: top.v:3542:49: Too many digits for 8 bit number: '8'h109'`。

   - **根因**:`src/codegen_verilog.cpp:141` `get_literal_str()` 在 8 位字面量上下文中输出 `8'h<value>`,**但未对 value 做宽度 mask**。当 `ch_uint<8>(i)` 中 `i` 在 depth=1000 时可达 999 (`0x3E7`,hex 写 12 位/SV 字面量需 10 bits),SystemVerilog 字面量 `8'h3e7` 因 hex digit 数量超过 8-bit 容量而被 Verilator 拒绝为语法错误(`-Wno-fatal` 救不了)。
   - **修法**:在 `get_literal_str` 内部对 `value` 按 `width = val.bv_.size()` 做 mask (`value & ((1ULL << width) - 1)`),使所有生成的字面量天然落在目标宽度范围内。例如:
     - `ch_uint<8>(109)` → 原 `8'h6d`(注: 109 < 256 不越界,但 mask 后同值)
     - `ch_uint<8>(256)` → mask 后 `8'h0`(0x100 & 0xFF = 0)
     - `ch_uint<8>(999)` → mask 后 `8'he7`(0x3E7 & 0xFF = 0xE7)
   - **本 change 不引入新重载**:实测 `litimpl` 构造函数用 `value.bitwidth()` 作为节点宽度,故 `lit_node->size() == val.bv_.size()`;`get_literal_str` 单参版内部 `val.bv_.size()` 已是目标宽度,无需 API 变更。

**3. `perf_three_way` ctest 入口范围矛盾且 TIMEOUT 不匹配**:`tests/benchmark/CMakeLists.txt` 的 `add_test(NAME perf_tests ...)` 调用 `perf_tests --all`,实测 ~930s (`TIMEOUT 1800`)。"仅跑 TC-07/08 三路"的快速门禁入口尚未独立。现有早期草稿提议的 `perf_three_way` 同时使用 `--all` 与 `TIMEOUT 240`,两者冲突,会在 CI 必超时红灯。

三件事共同导致 Verilator 后端被永久标记为"实验性"、CI 无法捕获 verilator 集成回归。本次 change 闭环。

## What Changes

- **`src/codegen_verilog.cpp` `get_literal_str()`**:在 1-arg 版本内部对 `value` 做宽度 mask,确保任意 `ch_uint<N>(literal)` 生成的 Verilog 字面量总是落在 `N` 位内,不再触发 Verilator 的 "Too many digits for N bit number" 语法错误。**不改签名、不加重载、不加 deprecation、不改调用点**。`lit_node->size() == val.bv_.size()` 已通过实测确认。

  ```cpp
  // 关键改动:在函数体内对 value 做 mask(单参版签名不变)
  uint32_t width = val.bv_.size();
  uint64_t masked = (width == 0) ? 0
                  : (width >= 64) ? value
                  : (value & ((1ULL << width) - 1));
  ```

- **`tests/benchmark/CMakeLists.txt`** 新增独立 ctest 测试 `perf_three_way`(区别于 `perf_tests`),**仅跑 TC-07/08 三路 subprocess 隔离对比**,`TIMEOUT 360`(基于实测 291s 留 1.24× 余量,非 `--all` 的 930s)。在 `BUILD_VERILATOR=ON` 时通过 `--verilator=${CPPHDL_VERILATOR_WRAPPER}` 注入 verilator 路径;**不注入** `VERILATOR_ROOT`(wrapper 自设已足够,避免路径不一致时触发 wrapper 一致性 `%Error`)。

- **`tests/test_verilator_three_way.cpp`**(新建 ~120 行):Catch2 测试覆盖以下场景(**不依赖 verilator 二进制**,纯 codegen 端到端断言):
  - `ch_uint<8>(256)` 生成的 verilog 字面量 MUST 落在 8 位内 (`8'h0`,而非 `8'h100`)
  - `ch_uint<8>(999)` 生成的 verilog 字面量 MUST 落在 8 位内 (`8'he7`,而非 `8'h3e7`)
  - `ch_uint<8>(0)` / `ch_uint<8>(1)` 仍然正常输出
  - TC-07 depth=1000 XOR chain 生成的 verilog **不应** 包含 `8'h` 后跟 3+ hex digits 的字面量(`H` 结尾除外)

- **不动**:
  - Verilator 后端运行时接口(`include/core/verilator_backend.h` 已稳定)
  - `verilator_runner.h` 的 SHA-1 cache 机制
  - `perf_main.cpp` 的 F2 subprocess 隔离架构(已通过 `fix-perf-subprocess-isolation` 落地)
  - `docs/AGENTS.md` 三路 perf 规范(契约不变,只是落实执行链路)
  - `get_literal_str` 的签名(实测 `lit_node->size() == val.bv_.size()`,无需外部传参)

## Capabilities

### New Capabilities

无。本 change 不引入新 capability。

### Modified Capabilities

- `perf-test-isolation`(已存在,archive 时合并):新增 1 条 Requirement 把"`perf_main` 在 BUILD_VERILATOR=ON 时 MUST 自动接收 `--verilator=${CPPHDL_VERILATOR_WRAPPER}`"契约形式化(取代手动 PATH 查找);新增 1 条 Requirement 把"`perf_three_way` ctest 测试入口存在且仅跑 TC-07/08 三路 subprocess 隔离对比"契约形式化(明确命令为 `--tc=07 --tc=08`,非 `--all`)。

- `verilator-backend-e2e`(已存在,archive 时合并):新增 1 条 Requirement 把"codegen 对 `ch_uint<N>(literal)` 产生的 Verilog 字面量 MUST mask 到 `N` 位内(避免触发 Verilator 'Too many digits' 语法错误)"契约形式化,覆盖 `get_literal_str` 在 XOR/ADD/SUB/MUL/SHL 等所有二元运算 RHS 字面量路径及 `print_concat` LHS/RHS 路径。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响源 | `src/codegen_verilog.cpp` (1 行 mask 实现 + `get_literal_str` 内部) |
| 受影响测试 | `tests/benchmark/CMakeLists.txt` (新增 `perf_three_way` ctest) + `tests/test_verilator_three_way.cpp` (新建) + `tests/CMakeLists.txt` (注册) |
| 受影响 spec | `openspec/specs/perf-test-isolation/spec.md`(追加 2 条 REQUIREMENT)+ `openspec/specs/verilator-backend-e2e/spec.md`(追加 1 条 REQUIREMENT) |
| **API 行为变更** | 否。`get_literal_str` 签名不变;仅内部对值做 mask,等价于 SpinalHDL verilog backend 行为 |
| 迁移影响 | 无。下游用户无直接 API 调用 |
| 性能 | 无变化(单 bitand 运算,常数时间) |
| 风险 | **LOW-MEDIUM**。`get_literal_str` 是 codegen 核心热路径;mask 行为对所有 codegen 字面量生效,需 `tests/test_verilog_gen` 46 个 TEST_CASE 全部仍通过 |
| 回滚 | `git revert` 单一 commit;mask 是 1 行删除,独立可回滚 |
| 关联 commit | `a120069 fix(codegen): emit synchronous reset in always_ff for Verilator compat`(前置);`fix-perf-subprocess-isolation`(subprocess 隔离基础) |
| CI 验证 | `openspec validate verilator-perf-three-way --strict` pre-archive;`ctest -R verilator_three_way`;`perf_tests --direct --tc=07 --verilator=${CPPHDL_VERILATOR_WRAPPER}` depth=1000 不再失败 |

