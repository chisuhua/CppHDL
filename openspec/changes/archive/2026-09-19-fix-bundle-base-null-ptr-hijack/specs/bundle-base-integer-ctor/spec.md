## ADDED Requirements

### Requirement: bundle_base 整数字面量构造必须产生有效节点

`bundle_base<Derived>` 是所有 Bundle 类型的基类。当用户用整数字面量（如 `0`、`42`）构造 `bundle_base<Derived>` 时，**MUST** 产生一个 `node_impl_` 非空的 IR 节点（即字面量节点），而**MUST NOT** 通过 null pointer constant 标准转换匹配 `bundle_base(lnodeimpl *node)` 产生 `node_impl_ = nullptr` 的空节点。

通过 `CH_BUNDLE_FIELDS_T` 宏（`bundle_meta.h:155`）的 `using bundle_base<Self>::bundle_base;`，此 ctor 被所有 22 个 bundle 派生类（`ch_stream<T>`、`ch_flow<T>`、`fifo_bundle<T>`、`interrupt_bundle`、`config_bundle<...>`、所有 AXI/axi_lite 通道 bundle、`ch_fragment`、`bundle_slice_view`、`bundle_concat`）继承——本 Requirement 隐式覆盖全部派生类。

#### Scenario: bundle_base<Derived>(0) 不再被空指针转换劫持
- **WHEN** 用户在 active context 中执行 `bundle_base<SomeType> b(0);`
- **THEN** `b.impl() != nullptr`
- **THEN** `b.impl()` 是一个有效的 `litimpl` 节点（字面量 0）
- **THEN** 后续 `b <<= ...` 等操作不会触发 null deref SEGV

#### Scenario: bundle_base<Derived>(42) 产生有效字面量节点
- **WHEN** 用户在 active context 中执行 `bundle_base<SomeType> b(42);`
- **THEN** `b.impl() != nullptr`
- **THEN** `b.impl()` 是一个有效的 `litimpl` 节点（字面量 42）

#### Scenario: bundle_base<Derived>(some_lnodeimpl_ptr) 仍走原路径
- **WHEN** 用户执行 `bundle_base<SomeType> b(some_lnodeimpl_ptr);` 其中 `some_lnodeimpl_ptr` 是真正的非 null `lnodeimpl*`
- **THEN** `b.impl() == some_lnodeimpl_ptr`（保持与修复前行为一致）

#### Scenario: bundle_base<Derived>(ch_literal_runtime) 仍走原路径
- **WHEN** 用户执行 `bundle_base<SomeType> b(some_ch_literal_runtime);`
- **THEN** 构造成功，字面量节点的 `value` 和 `width` 与 `some_ch_literal_runtime` 一致（保持与修复前行为一致）

#### Scenario: bundle_base<Derived>(false) 编译错误（bool 不被劫持）
- **WHEN** 用户执行 `bundle_base<SomeType> b(false);`
- **THEN** 编译失败，错误信息明确指出无可用构造函数
- **THEN** 修复前与修复后行为一致——`false` 不是 null pointer constant，`ch_literal_runtime(bool)` 是 explicit 构造（`literal.h:66`）不可用于隐式转换，`bundle_base` 无 bool 构造函数
- **NOTE**: SFINAE 排除 bool 确保 `bundle_base<Derived>(false)` 不会匹配新增的整数 ctor（避免歧义）

#### Scenario: ch_stream<T>(0) 继承修复（派生类自动覆盖）
- **WHEN** 用户在 active context 中执行 `ch_stream<ch_uint<8>> s(0);`
- **THEN** `s.impl() != nullptr`（修复前为 nullptr——本 Requirement 隐式覆盖所有 `CH_BUNDLE_FIELDS_T` 派生类）

#### Scenario: ch_flow<T>(0) 继承修复（派生类自动覆盖）
- **WHEN** 用户在 active context 中执行 `ch_flow<ch_uint<8>> f(0);`
- **THEN** `f.impl() != nullptr`（修复前为 nullptr）

#### Scenario: axi_lite_b_channel(0) 继承修复（派生类自动覆盖）
- **WHEN** 用户在 active context 中执行 `axi_lite_b_channel b(0);`
- **THEN** `b.impl() != nullptr`（修复前为 nullptr）

#### Scenario: bundle_base<Derived>(nullptr) 行为（已知 out-of-scope）
- **WHEN** 用户执行 `bundle_base<SomeType> b(nullptr);`
- **THEN** 编译通过，`b.impl() == nullptr`（`nullptr` 是显式用户意图，SFINAE 不拦截）
- **NOTE**: 此行为与修复前一致；视为显式用户意图而非 bug，独立 follow-up 可考虑拒绝 nullptr