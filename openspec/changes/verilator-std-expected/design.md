# Design: verilator-std-expected

## Context

`VerilatorBackend` 当前的错误流是 1990s 风格的 `bool + log`：
- 6 个 `bool` 返回（1 public `initialize()` + 5 private）
- 12 个 `CHERROR` / `CHWARN` 站点（`src/core/verilator_backend.cpp`）
- 错误细节通过宏展开为 `printf` 风格字符串，丢失 type-level 信息
- 调用方 4 个 test 文件 + 2 个 example + 1 个 Simulator 站点无法区分错误类别

C++23 `std::expected<T, E>`（P0323R12）由 C++23 标准化，让 `T` 为成功值类型、`E` 为错误类型，类型系统强制处理。本 change 是 `cpp23-standard-upgrade` (f5e3479) 确立 C++23 baseline 后的**首个 C++23 特性采用 PR**，目的是为后续 `std::format` / `std::flat_map` / deducing this 等特性提供 spec + 实现模板。

## Goals / Non-Goals

详见 `proposal.md`。本设计文档补充技术决策。

## Decisions

### D1：仅改造 VerilatorBackend，不动 IEvalBackend

- 决策：`IEvalBackend::initialize()` 保持 `bool` 签名不变
- 理由：
  - `IEvalBackend` 由 3 个后端实现（Interpreter / JIT / Verilator），改它需要 3 处同步改造 + 全部测试更新，scope 蔓延
  - `std::expected` 是**增值**而非**替换**：Verilator 后端能给出更细的错误，其他后端维持现状
  - 现有 `bool initialize()` 调用（4 test + 2 example + 1 Simulator）0 改动即可编译
- 实现：在 `VerilatorBackend` 内部新增 `try_initialize()` 公共方法 + 把 `bool initialize()` 改写为薄 wrapper

### D2：VerilatorError 类型设计

- 决策：8 个 `enum class` 错误码 + 消息 + `std::source_location`
- 草图：
  ```cpp
  // include/core/verilator_backend.h
  enum class VerilatorErrorCode {
      VerilogGeneration,     // generate_verilog() failed (cannot open top.v)
      VerilatorCompile,      // invoke_verilator() failed (verilator --cc returned non-zero)
      Dlopen,                // dlopen_top() failed (libVtop.so not loadable)
      DlsymMissing,          // dlsym returned nullptr for required symbol
      PortBitwidthOverflow,  // build_port_access_table() found port > 64 bits
      PortAccessTable,       // generic build_port_access_table() failure
      FileIO,                // generic file I/O failure
      CacheWriteback,        // writeback_to_cache() failed (best-effort, not abort)
  };

  struct VerilatorError {
      VerilatorErrorCode code;
      std::string message;            // human-readable, from CHERROR format string
      std::source_location location;  // C++20 feature, captured at error site
  };

  // 类型别名
  using VerilatorResult = std::expected<void, VerilatorError>;
  ```
- 理由：
  - 8 个变体映射 ADR-035 §5 R1 / R5 / R8 / R10 风险
  - `std::source_location` 自动捕获 file:line:function，无需手工 `__FILE__` 宏
  - 单个 `VerilatorError` struct 便于日志输出（`fmt::format("{}\n  at {}", e.message, e.location)`）和 pattern matching

### D3：backward compat 策略

- 决策：保留 `bool initialize(...)` 为 wrapper
- 草图：
  ```cpp
  // verilator_backend.h
  class VerilatorBackend : public IEvalBackend {
  public:
      // 新增：富错误返回
      std::expected<void, VerilatorError>
      try_initialize(ch::core::context *ctx, ch::data_map_t &data_map);

      // 保留：向后兼容 wrapper
      bool initialize(ch::core::context *ctx, ch::data_map_t &data_map) override {
          return try_initialize(ctx, data_map).has_value();
      }
      // ...
  };
  ```
- 理由：
  - `IEvalBackend::initialize()` 是 pure virtual `bool` 签名，必须 override
  - 现有 test `REQUIRE(backend.initialize(...))` 0 改动
  - 新 test 可用 `auto r = backend.try_initialize(...); REQUIRE(r);` 获得详细错误
- 注意：失败时原始错误信息**丢失**（因为 wrapper 只返回 bool）；未来 test 想看错误细节必须升级到 `try_initialize()`

### D4：private 方法返回类型迁移

- 决策：5 个 private `bool` 方法全改为 `std::expected<void, VerilatorError>`
- 草图：
  ```cpp
  // verilator_backend.h
  private:
      VerilatorResult generate_verilog(ch::core::context *ctx);
      VerilatorResult invoke_verilator(const std::string &verilog_path);
      VerilatorResult dlopen_top(const std::string &so_path);
      VerilatorResult build_port_access_table();
      VerilatorResult writeback_to_cache(const std::string &cache_key);
  ```
- 内部调用链改造：`generate_verilog() → invoke_verilator() → dlopen_top() → build_port_access_table()` 用 `std::move(...)` / `return std::unexpected(...)` 传递错误
- 理由：
  - 内部一致性：5 个失败点统一类型
  - 调用链错误自动冒泡，无需在 `try_initialize()` 里手工 `if (!gen) { ... }` 重构
  - C++23 `std::expected` 支持 `auto operator->()` 与 `operator*()` 链式调用，写法自然

### D5：错误码到 ADR-035 风险的映射

| VerilatorErrorCode | ADR-035 风险 | 触发场景 |
|--------------------|--------------|----------|
| `VerilogGeneration` | — | 写 `top.v` / `sim_main.cpp` 失败（disk full / permission） |
| `VerilatorCompile` | R8 | `verilator --cc --build` 返回非零（codegen bug） |
| `Dlopen` | R1 / R10 | `dlopen(libVtop.so)` 返回 nullptr（ABI 不兼容 / 多 .so 符号冲突） |
| `DlsymMissing` | R5 | dlsym 7 个符号中任一返回 nullptr（codegen 字段名错） |
| `PortBitwidthOverflow` | — | 端口位宽 > 64（Verilator QData 限制） |
| `PortAccessTable` | — | port table 构建失败（兜底） |
| `FileIO` | — | 其他文件 I/O 失败（fallback） |
| `CacheWriteback` | — | `~/.cache/cpphdl/verilator/<key>/` 写失败（best-effort） |

R7（内存端口 Verilator 不识别）不在本 change scope（独立 workstream）。

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| IEvalBackend 同步修改遗漏（其他后端编译错） | 低 | D1 明确不动 IEvalBackend；现有 `bool initialize()` wrapper 保证 0 改动 |
| `std::source_location` 在 GCC 13.3 表现 | 低 | C++20 特性，GCC 10+ 已 stable；本项目已升 C++23 验证 |
| wrapper `bool initialize()` 丢失错误信息 | 中 | D3 注释：未来 test 升级到 `try_initialize()`；当前 4 个 test 文件只需 `REQUIRE(bool)` 即可 |
| test 改造漏改导致编译错 | 低 | 仅 2 个 example 文件改造为示范，4 个 test 文件保持 0 改动（向后兼容） |
| `std::expected` 在 ABI 层的 size 影响 | 极低 | 16-24 bytes（T + E），不影响 `VerilatorBackend` 公共布局（成员不暴露 T/E） |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| 现有 ctest | `ctest -LE perf` | 142/143 仍通过（pre-existing 1 失败不归本 change） |
| 新增 test | `ctest -R test_verilator_error_flow` | 全通过 |
| 例程 0 改动 | `cmake --build build --target 01_minimal_counter 02_counter_with_input` | exit 0 |
| 例程升级 | `ctest -R verilator` 包含 example run | 仍通过 |
| openspec validate | `openspec validate verilator-std-expected --strict` | `Change 'verilator-std-expected' is valid` |
| spec 一致性 | `openspec validate --specs --strict` | 8/8 passed（+1 新 capability） |

## TDD 5 步骨架（详细在 tasks.md）

1. **RED**：写 `tests/test_verilator_error_flow.cpp` 8 个 TEST_CASE 对应 8 个 `VerilatorErrorCode`，先 assert std::expected 返回
2. **GREEN**：在 `include/core/verilator_backend.h` 加 `VerilatorError` struct + `VerilatorErrorCode` enum + `VerilatorResult` alias + `try_initialize()` 公共方法
3. **GREEN**：5 个 private 方法改返回类型 + `src/core/verilator_backend.cpp` 内 12 个 `CHERROR/CHWARN/return false` 站点改 `std::unexpected(VerilatorError{...})`
4. **REFACTOR**：把 `bool initialize()` 改写为 `try_initialize().has_value()` wrapper；更新 2 个 example 文件示范 `or_else()` 错误处理
5. **VERIFY**：全 ctest 142/143 + 新 test 全通过 + `openspec validate` 8/8

## 关联 ADR 引用

- **ADR-035 §3.5 加载模式**：dlopen + SHA-1 缓存 — 解释 `dlopen_top()` 为何是 `expected`-able
- **ADR-035 §5 风险登记 R1/R5/R8/R10**：4 类失败模式直接映射到 4 个 `VerilatorErrorCode` 变体
- **build-cpp-standard spec (f5e3479)**：C++23 baseline 已确立，本 change 兑现"引入 C++23 特性"的承诺
