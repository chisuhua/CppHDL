# Design: fix-bundle-base-null-ptr-hijack

## Context

P0 commit `47af57f`（"fix(core): prevent null pointer hijack in ch_uint/ch_bool integer ctors"）修复了 CppHDL 中 `ch_uint<N>` 和 `ch_bool` 通过继承的 `logic_buffer(lnodeimpl*)` 构造函数被 null pointer constant（`0`）劫持的 SEGV 级联 bug。

**Metis 审查的致命发现**：之前的 Oracle 审计结论**有误**。审计声称"19 个 bundle 派生类因无 `using` 声明而安全"，但实际：

- `include/core/bundle/bundle_meta.h:155` 中 `CH_BUNDLE_FIELDS_T(...)` 宏展开为：
  ```cpp
  using bundle_base<Self>::bundle_base;     // ← 关键行
  ```
- 所有 22 个使用 `CH_BUNDLE_FIELDS_T` 的 bundle 派生类（`ch_stream<T>`、`ch_flow<T>`、`fifo_bundle<T>`、`interrupt_bundle`、`config_bundle<...>`、所有 AXI 通道 bundle（`axi_lite_aw_channel`、`axi_lite_w_channel`、`axi_lite_b_channel`、`axi_lite_ar_channel`、`axi_lite_r_channel`、`axi_lite_write_interface`、`axi_lite_read_interface`、`axi_lite_bundle`、`axi_addr_channel`、`axi_write_data_channel`、`axi_write_resp_channel`、`axi_write_channel`）、`ch_fragment`、`bundle_slice_view`、`bundle_concat`）通过该 `using` 继承 `bundle_base` 的所有构造函数——包括 `bundle_base(lnodeimpl *node)`（`bundle_base.h:42`）

**Metis 已实证验证漏洞 live**：
```cpp
ch_stream<int>       s(0);   // 编译通过，impl() = nullptr  ← BUG
axi_lite_b_channel   b(0);   // 编译通过，impl() = nullptr  ← BUG
```

**漏洞机制**（与 P0 完全相同）：
```
ClassName(0)  // 用户合法代码，ClassName 是 bundle 派生类
  → bundle_base(lnodeimpl*) via null pointer constant   ← BUG
    → node_impl_ = nullptr
      → <<= 或 op 触发 → muximpl::create_instruction SEGV
```

**修复覆盖范围**：在 `bundle_base` 上添加 SFINAE 整数 ctor，通过 `using` 继承**自动覆盖所有 22 个派生类**——这是本 change 的核心价值。

## Goals / Non-Goals

**Goals:**
- 在 `bundle_base<Derived>` 上添加 SFINAE 受限的模板构造函数，拦截整数字面量构造
- 模板使用 identity match 优先于 `bundle_base(lnodeimpl *node)` 的 null pointer constant 标准转换
- 通过 `using` 继承自动覆盖所有 22 个 bundle 派生类
- 包含 `bool` 排除约束（镜像 P0 `ch_bool` 修复模式），避免 `bundle_base<D>(false)` 路径冲突
- 不破坏任何现有合法构造路径（`bundle_base<Derived>(some_lnodeimpl_ptr)`、`bundle_base<Derived>(some_ch_literal_*)`、`bundle_base<Derived>(bool)` 等）
- 提供回归测试覆盖 `bundle_base` 本身 + 至少 3 个派生类
- 关闭此类漏洞的最后一扇门

**Non-Goals:**
- 不修改 `logic_buffer<T>` 基类（其 `logic_buffer(lnodeimpl*)` 是合法基础原语；`explicit` 单独不能修复 `0` 劫持，因为 null pointer conversion 在 direct-init 下也是标准转换）
- 不修改 `ch_uint<N>` / `ch_bool` 的 P0 修复
- 不修改 `CH_BUNDLE_FIELDS_T` 宏本身（修复通过 `using` 自动传播）
- 不引入新的 capability（仅修改现有 `bundle-base-integer-ctor` capability 的初始 spec）
- 不修改 `node_builder::build_literal` 的语义
- 不审计 `ch_logic_out(outputimpl*)`、`ch_reg<T>` 等其他类似 raw-pointer ctor（这些是 MEDIUM/LOW 风险，独立 follow-up）

## Decisions

### Decision 1: SFINAE 受限模板 + `bool` 排除

**理由**：
- P0 `47af57f` 已证明 SFINAE 模板模式有效：`ch_uint<N>(0)` 现在产生有效 `node_impl_`
- **必须包含 `bool` 排除**：`std::is_same_v<std::decay_t<IntT>, bool>` 为 true 时 SFINAE 失败，`bundle_base<Derived>(false)` 走其他现有路径（如果存在）或产生歧义/编译错误
- P0 `ch_bool` 修复使用相同模式（`bool.h` 中的 SFINAE 模板）
- Identity match 永远优于标准转换（C++ 重载决议规则）

**实现**（在 `include/core/bundle/bundle_base.h` 现有构造函数后追加）：

```cpp
// BUGFIX (fix-bundle-base-null-ptr-hijack): SFINAE-restricted
// integer literal ctor. Without this, `bundle_base<Derived>(0)`
// matches `bundle_base(lnodeimpl *node)` via null pointer
// conversion, creating a node with node_impl_ = nullptr.
//
// Through `using bundle_base<Self>::bundle_base;` in the
// CH_BUNDLE_FIELDS_T macro (bundle_meta.h:155), this ctor is
// inherited by all 22 bundle derived classes (ch_stream, ch_flow,
// all AXI/axi_lite channels, fifo_bundle, interrupt_bundle,
// config_bundle, ch_fragment, bundle_slice_view, bundle_concat).
//
// `bool` is excluded (mirror P0 ch_bool pattern) so that
// `bundle_base<Derived>(false)` doesn't become ambiguous.
//
// Identity match for integer args wins over the standard-conversion
// null pointer path. The existing `bundle_base(lnodeimpl *node)`
// ctor remains available for legitimate lnodeimpl* arguments.
template <typename IntT,
          typename = std::enable_if_t<
              std::is_integral_v<std::decay_t<IntT>> &&
              !std::is_same_v<std::decay_t<IntT>, bool>>>
explicit bundle_base(
    IntT v, const std::string &name = "bundle_lit",
    const std::source_location &sloc = std::source_location::current())
    : logic_buffer<Derived>() {
    ch_literal_runtime lit(static_cast<std::uint64_t>(v));
    this->node_impl_ =
        node_builder::instance().build_literal(lit, name, sloc);
    if (!this->node_impl_) {
        CHERROR("[bundle_base] Failed to create literal node "
                "from integer value");
    }
}
```

**关键设计点**：
- 使用 `ch_literal_runtime(static_cast<std::uint64_t>(v))` 单参数 ctor（`literal.h:53-54`），它内部调用 `compute_width(v)` 计算宽度（`0 → 1`，正数 → `bit_width(v)`）
- `bundle_base<Derived>` 没有 `N` 模板参数（不像 `ch_uint<N>`），所以宽度由 `compute_width(v)` 决定
- 这与现有 `ch_literal_runtime` ctor 路径（`bundle_base.h:60-71`）的宽度语义一致

### Decision 2: 实现放在头文件内联（不需要 .tpp）

**理由**：
- `bundle_base.h` 的现有构造函数（包括 `bundle_base(lnodeimpl *node)`、`bundle_base(ch_literal_runtime)`、`bundle_base(ch_literal_impl<V,W>)`）都是头文件内联实现
- 保持局部一致性
- `node_builder`、`ch_literal_runtime` 已通过 include chain 可用

### Decision 3: 通过 `using` 继承自动覆盖派生类

**Metis 审查修正**：原 Oracle 审计错误声称派生类安全。实际它们通过 `CH_BUNDLE_FIELDS_T` 宏的 `using bundle_base<Self>::bundle_base;` 暴露了 `bundle_base(lnodeimpl*)`。

**关键点**：在 `bundle_base` 上添加 SFINAE ctor 后，**22 个派生类自动获得修复**——这是本 change 的核心价值。无需逐个修改派生类。

### Decision 4: 使用 `explicit` 关键字

**理由**：
- `bundle_base(lnodeimpl *node)` 当前**没有** `explicit` 标记
- 添加 SFINAE 整数 ctor 时加 `explicit`，避免 `bundle_base<Derived> x = 0;` 这种 copy-init 路径意外触发
- **这比 P0 `ch_uint`/`ch_bool` 更严格**（P0 的 SFINAE ctor 不是 explicit）；此处使用 explicit 是**有意更严格**，作为额外的防御层

### Decision 5: 回归测试放新文件 `tests/test_bundle_literal.cpp`

**理由**：
- 单独文件便于未来扩展（如果发现其他 bundle 漏洞）
- 覆盖 `bundle_base` 本身 + 3+ 个派生类（`ch_stream`、`ch_flow`、至少一个 AXI 通道）
- 不需要 compile-fail 测试（Catch2 在本项目无 negative-compile 机制，且修复后 `ch_stream<T>(0)` 不再是编译错误而是有效构造——这正是修复目的）

## Risks / Trade-offs

### Risk 1: SFINAE 模板可能在某些边缘情况下不匹配

**场景**：`bundle_base<Derived>(some_user_defined_int_like_type)` 如果用户定义了类似 `struct MyInt { int v; };` 但未实现 `operator int()`，则 SFINAE 不会匹配。

**缓解**：符合预期行为——用户应显式构造。本 change 不假设非整数类可隐式转换。

### Risk 2: 现有代码可能隐式依赖 `bundle_base<Derived>(0)` 的 nullptr 行为

**场景**：如果某个 bundle 派生类的代码故意写 `bundle b(0);` 然后用 `if (b.impl() == nullptr)` 做存在性检查。

**缓解**：
- 这种代码模式是 anti-pattern（应该用 `b.is_valid()` 或显式 `bundle b;` 然后检查 `dir_`）
- 修复后行为变化是**预期的**（从"null 节点"变为"有效 false 节点"），下游代码应迁移
- CHANGELOG 记录此行为变更

### Risk 3: 修改 `bundle_base.h` 影响 22 个 bundle 派生类的编译

**缓解**：
- 头文件内联实现，编译时所有派生类都包含 `bundle_base.h`
- CMake 完整构建会验证所有派生类仍能编译
- 如果有任何派生类编译失败，说明该类有特殊行为，需要单独处理

### Risk 4: `bundle_base<Derived>(nullptr)` 仍产生 null 节点

**场景**：用户显式传 `nullptr` 作为显式意图。

**状态**：**已知 out-of-scope**。`nullptr` 不是 integral type，SFINAE 不会匹配；它会匹配 `bundle_base(lnodeimpl *node)`。这是**显式用户意图**，不视为 bug。如果未来需要拒绝 `nullptr`，需要额外处理（独立 follow-up）。

### Risk 5: 负整数 wrap 到 uint64

**场景**：`bundle_base<Derived>(-1)` 转换为 `uint64_t(-1)` = `0xFFFFFFFFFFFFFFFF`（width 64）。

**状态**：**已知行为**，与 `ch_literal_runtime(int64_t)` 库的现有语义一致。CHANGELOG 不记录（库统一行为）。

### Risk 6: 枚举类型 edge case

**场景**：`bundle_base<Derived>(MyEnum::VALUE)` 或 `bundle_base<Derived>(some_unscoped_enum)`。

**分析**：枚举类型**不可被劫持**——理由：
- `std::is_integral_v<EnumT>` 为 false（枚举不是 integral type），所以 SFINAE 模板 ctor **不会**被匹配
- 枚举值**不是** null pointer constant（C++ 标准：null pointer constant 仅限 integer literal 0 或 `nullptr_t`），所以 `bundle_base(lnodeimpl*)` 不会通过 null pointer conversion 匹配
- 枚举值通过 standard + user-defined conversion 匹配 `bundle_base(const ch_literal_runtime&)`（与正整数路径相同），产生有效字面量节点
- 严格 scoped enum (`enum class`) 无 implicit conversion to integral，匹配不到 `ch_literal_runtime(uint64_t)`，会产生编译错误（清晰错误，非静默 null）

**状态**：**已知安全**，无需额外处理。文档化此分析以供未来审计参考。

## Verification Strategy

1. **编译验证**：`cmake --build build -j$(nproc)` 无 error/warning（pre-existing warnings 不计）
2. **回归测试**：新增 `tests/test_bundle_literal.cpp` 6-8 个 TEST_CASE 全部通过
3. **P0 回归**：`test_chipforge_mux_segv_repro` 仍 5/5 通过（确认无回归）
4. **全测试套件**：`ctest -E perf_tests` 所有测试通过（pre-existing Verilator 环境失败不计）
5. **示例验证**：`./run_all_ported_tests.sh`（28 个 main() 示例，覆盖 ch_stream、axi_lite_bundle 等的实际使用）
6. **派生类编译验证**：22 个 bundle 派生类仍能正常编译（由完整构建保证）
7. **API 兼容性**：手动检查所有现有 `bundle_base<Derived>(...)` 调用仍走原路径（grep + 编译测试）
8. **OpenSpec 校验**：`openspec validate fix-bundle-base-null-ptr-hijack --strict` pre-archive