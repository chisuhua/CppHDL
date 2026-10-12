## ADDED Requirements

### Requirement: VerilatorError 类型契约
`VerilatorBackend` 失败时 **MUST** 返回 `std::expected<void, VerilatorError>`，其中 `VerilatorError` **SHALL** 包含三个字段：`VerilatorErrorCode code`（枚举 8 个变体）、`std::string message`（人类可读，从原 `CHERROR` 格式字符串迁移）、`std::source_location location`（C++20 特性，自动捕获 file:line:function）。

#### Scenario: VerilatorErrorCode 8 个变体完整
- **WHEN** 阅读 `include/core/verilator_backend.h` 的 `enum class VerilatorErrorCode`
- **THEN** 变体集合为 `{VerilogGeneration, VerilatorCompile, Dlopen, DlsymMissing, PortBitwidthOverflow, PortAccessTable, FileIO, CacheWriteback}`，无遗漏无冗余

#### Scenario: 错误消息从 CHERROR 迁移
- **WHEN** 阅读 `src/core/verilator_backend.cpp` 中的每个 `std::unexpected(VerilatorError{...})` 站点
- **THEN** `VerilatorError::message` 包含原 `CHERROR` 宏的相同诊断信息（例：`"port id 30 has bitwidth 65 > 64; ADR-035 §M2 rejects wide signals"`）

#### Scenario: source_location 自动捕获
- **WHEN** 任意 `std::unexpected(VerilatorError{.location = std::source_location::current()})` 触发
- **THEN** `VerilatorError::location.file_name()` 返回 `"src/core/verilator_backend.cpp"`，`location.line()` 返回触发行的 1-based 行号

### Requirement: 6 个失败点全部用 std::expected 表达
`VerilatorBackend` 的 6 个失败点 **MUST** 全部以 `std::expected<T, VerilatorError>` 返回：`initialize()` 的等价方法 `try_initialize()`（公共，1 个），加上 5 个 private 方法 `generate_verilog()` / `invoke_verilator()` / `dlopen_top()` / `build_port_access_table()` / `writeback_to_cache()`。

#### Scenario: public try_initialize 签名
- **WHEN** 阅读 `include/core/verilator_backend.h` 的 `VerilatorBackend` 类
- **THEN** 存在方法 `std::expected<void, VerilatorError> try_initialize(ch::core::context*, ch::data_map_t&)`，参数与 override 的 `bool initialize()` 完全一致

#### Scenario: 5 private 方法签名
- **WHEN** 阅读 `include/core/verilator_backend.h` 的 `VerilatorBackend` private 段
- **THEN** `generate_verilog()` 返回 `VerilatorResult`，`invoke_verilator(string)` 返回 `VerilatorResult`，`dlopen_top(string)` 返回 `VerilatorResult`，`build_port_access_table()` 返回 `VerilatorResult`，`writeback_to_cache(string)` 返回 `VerilatorResult`（无 `bool` 返回残留）

#### Scenario: bool initialize wrapper 保留
- **WHEN** 阅读 `include/core/verilator_backend.h` 的 `bool initialize(...)` override
- **THEN** 函数体为 `{ return try_initialize(ctx, data_map).has_value(); }`（或等价单行实现），不再有任何 `CHERROR` / `return false` 站点

### Requirement: 向后兼容 IEvalBackend 接口
`IEvalBackend::initialize()` 虚函数签名 **MUST** 保持 `bool` 返回不变。`VerilatorBackend::bool initialize()` 通过内部 `try_initialize()` wrapper 实现，**MUST NOT** 修改 `IEvalBackend` 抽象层。

#### Scenario: IEvalBackend 0 改动
- **WHEN** `git diff` 本 change 涉及 `include/core/eval_backend.h`
- **THEN** 0 行变更（diff 为空）

#### Scenario: InterpreterBackend / JITBackend 0 改动
- **WHEN** `git diff` 本 change 涉及 `src/core/interpreter_backend.cpp` 或 `src/jit/jit_compiler.cpp`
- **THEN** 0 行变更

#### Scenario: 现有 REQUIRE(backend.initialize(...)) 0 改动
- **WHEN** 阅读 `tests/test_verilator_e2e_harness.cpp` 与 `tests/test_axi_lite_verilator_e2e.cpp` 中所有 `REQUIRE(backend.initialize(...))` 站点
- **THEN** 0 行变更（向后兼容验证）

### Requirement: 错误码到 ADR-035 风险的可追溯映射
每个 `VerilatorErrorCode` 变体 **SHALL** 在 `design.md` 中标注其对应的 ADR-035 §5 风险登记编号。`VerilatorCompile` → R8（codegen 回归），`Dlopen` → R1 + R10（ABI 不兼容 + 符号冲突），`DlsymMissing` → R5（IO 字段名错）。

#### Scenario: 风险追溯注释
- **WHEN** 阅读 `include/core/verilator_backend.h` 的 `enum class VerilatorErrorCode` 定义
- **THEN** 每个变体上方有 `/// @adr ADR-035 §5 R<n>` 注释，n ∈ {1, 5, 8, 10}

#### Scenario: design.md D5 表格完整
- **WHEN** 阅读 `openspec/changes/verilator-std-expected/design.md` 的 D5 表格
- **THEN** 8 个变体全部列出对应的 ADR-035 风险或标注"无 ADR 风险"

### Requirement: 错误携带 std::source_location 用于日志
`VerilatorError::location` **MUST** 反映错误触发的源代码位置（file:line:function），让 `CHWARN` / `CHERROR` 日志输出能直接指向失败源，无需用户手工 grep。

#### Scenario: source_location 字段类型
- **WHEN** 阅读 `struct VerilatorError` 定义
- **THEN** `location` 字段类型为 `std::source_location`（C++20 标准库类型）

#### Scenario: 日志输出含 location
- **WHEN** 任何 `VerilatorError` 通过 `try_initialize()` 返回到调用方，且调用方选择打印
- **THEN** 输出格式至少含 `at <file>:<line>:<function>` 三元组

### Requirement: 单元测试覆盖 8 个错误码
`tests/test_verilator_error_flow.cpp` **MUST** 包含 8 个 `TEST_CASE`，每个对应一个 `VerilatorErrorCode` 变体，使用 mock context 或假路径触发对应失败路径并断言 `std::expected` 返回值的 `error().code` 等于预期变体。

#### Scenario: 8 个 TEST_CASE 一一对应
- **WHEN** `ctest -R test_verilator_error_flow --output-on-failure`
- **THEN** 列出 8 个测试名，每个含错误码变体名（例：`"Dlopen returns VerilatorErrorCode::Dlopen on missing libVtop.so"`）

#### Scenario: location 字段断言
- **WHEN** 任意错误返回的 `VerilatorError::location.line()` 在测试中
- **THEN** 等于该错误触发点的源代码行号（与 `src/core/verilator_backend.cpp` 的实际行匹配，编译期可验证）

#### Scenario: wrapper 测试
- **WHEN** 测试 `bool VerilatorBackend::initialize(...)` 在 `try_initialize()` 失败时
- **THEN** 返回 `false`（向后兼容契约）
