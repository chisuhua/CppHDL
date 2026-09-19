# fix-bundle-base-null-ptr-hijack

## Why

P0 commit `47af57f`（"fix(core): prevent null pointer hijack in ch_uint/ch_bool integer ctors"）修复了 `ch_uint<N>(0)` 和 `ch_bool(0)` 走继承自 `logic_buffer<T>` 的 `logic_buffer(lnodeimpl*)` 构造函数，导致 `node_impl_ = nullptr` 的 SEGV 级联 bug。

**Metis 审查发现的致命错误**（之前的 Oracle 审计遗漏）：`bundle_base<Derived>` 不仅是"防御性 MEDIUM 风险"——它的漏洞**在生产代码中已经 live**。

**根因**：`include/core/bundle/bundle_meta.h:155` 中 `CH_BUNDLE_FIELDS_T(...)` 宏展开为：

```cpp
using bundle_base<Self>::bundle_base;     // ← 关键行
static constexpr auto __bundle_fields() { ... }
```

每个使用 `CH_BUNDLE_FIELDS_T` 宏的 bundle 派生类（`ch_stream<T>`、`ch_flow<T>`、`fifo_bundle<T>`、所有 AXI/axi_lite 通道、`ch_fragment`、`bundle_concat` 等，共 22 个类，分布于 9 个头文件）通过 `using bundle_base<Self>::bundle_base;` **继承 `bundle_base` 的所有构造函数**——包括存在漏洞的 `bundle_base(lnodeimpl *node)`（`bundle_base.h:42`）。

**实证验证**（Metis 已完成）：
```cpp
ch_stream<int>       s(0);   // 编译通过，impl() = nullptr  ← BUG
axi_lite_b_channel   b(0);   // 编译通过，impl() = nullptr  ← BUG
```

**级联路径与 chipforge SEGV 同源**：
```
bundle_base<Derived>(0)          // 用户合法代码
  → bundle_base(lnodeimpl*) with node=nullptr  ← BUG
    → node_impl_ = nullptr
      → bundle <<= ch_uint<5>(...)  → operator<<= 检查通过但 this->node_impl_ 为 null
        → opimpl with null src → simulator SEGV in muximpl::create_instruction
```

**实际风险 HIGH（不是 MEDIUM）**：原 Oracle 审计低估了风险。漏洞在生产代码中已 live，22 个 bundle 类都可触发，级联路径与 chipforge SEGV 完全相同。

**修复覆盖范围**：在 `bundle_base` 上添加 SFINAE 整数构造函数，**会通过 `using` 继承自动覆盖所有 22 个派生类**——这是本 change 的核心价值。

## What Changes

- **`include/core/bundle/bundle_base.h`**：添加 SFINAE 受限的模板构造函数 `bundle_base<Derived>(IntT v, ...)`，使用 identity match 拦截整数字面量，**优先于** `bundle_base(lnodeimpl *node)` 通过 null pointer constant 的标准转换匹配
  - **包含 `bool` 排除**：SFINAE 增加 `!std::is_same_v<std::decay_t<IntT>, bool>` 约束，镜像 P0 `ch_bool` 修复模式，避免 `bundle_base<D>(false)` 路径冲突
  - **不使用 `N` 模板参数**（`bundle_base<Derived>` 只有 `Derived`，无宽度模板参数）；字面量宽度由 `ch_literal_runtime::compute_width(v)` 计算，与 `ch_literal_runtime(uint64_t)` 单参数 ctor 默认行为一致
- **修复覆盖范围（关键价值）**：通过 `CH_BUNDLE_FIELDS_T` 宏（`bundle_meta.h:155`）的 `using bundle_base<Self>::bundle_base;`，新增的 SFINAE ctor **自动被所有 22 个 bundle 派生类继承**——无需逐个修改派生类，一次修复覆盖全部
- **新增 `tests/test_bundle_literal.cpp`**：覆盖以下场景
  - `bundle_base<Derived>(0)` 在 active context 中产生有效 `node_impl_`（非 nullptr）
  - `bundle_base<Derived>(42)` 同上
  - `bundle_base<Derived>(some_lnodeimpl_ptr)` 仍走原 `bundle_base(lnodeimpl*)` 路径
  - `bundle_base<Derived>(ch_literal_runtime(...))` 仍走原 `ch_literal_runtime` 路径
  - `bundle_base<Derived>(false)` 仍走现有 bool 路径（SFINAE 排除 bool 保证无歧义）
  - **派生类覆盖**（证明通过继承自动修复）：`ch_stream<ch_uint<8>>(0).impl() != nullptr`、`axi_lite_b_channel(0).impl() != nullptr`、`ch_flow<ch_uint<8>>(0).impl() != nullptr`
- **不动**：
  - `logic_buffer<T>` 基类（基类构造函数是合法基础原语；`explicit` 单独不能修复 `0` 劫持，因为 null pointer conversion 是 direct-init 也允许的标准转换）
  - `ch_uint<N>` 和 `ch_bool`（P0 修复范围之外）
  - `CH_BUNDLE_FIELDS_T` 宏本身（修复通过 `using` 继承自动传播，无需修改宏）
  - `node_builder::build_literal` 的实现

## Capabilities

### New Capabilities

无。本 change 不引入新 capability。

### Modified Capabilities

- `bundle-base-integer-ctor` (新 capability folder)：定义 `bundle_base<Derived>` 及 22 个通过 `CH_BUNDLE_FIELDS_T` 宏继承构造函数的 bundle 派生类对整数字面量的构造语义，新增 1 条 REQUIREMENT 把"整数字面量构造 MUST 产生有效节点，MUST NOT 被空指针转换劫持"契约形式化，覆盖 `bundle_base` 本身 + 所有 `using bundle_base<Self>::bundle_base;` 派生类。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响头文件 | `include/core/bundle/bundle_base.h`（新增 ~20 行模板 ctor + 注释） |
| 隐式覆盖 | 通过 `CH_BUNDLE_FIELDS_T` 宏的 `using bundle_base<Self>::bundle_base;`（`bundle_meta.h:155`），修复自动传播到 22 个 bundle 派生类，无需逐个修改 |
| 新增测试 | `tests/test_bundle_literal.cpp`（~120 行，6-8 个 TEST_CASE，覆盖 `bundle_base` + 3+ 个派生类） |
| 受影响 spec | `openspec/specs/bundle-base-integer-ctor/spec.md`（新增，archive 时合并） |
| **API 行为变更** | **是**。所有 `bundle_base<Derived>(整数字面量)` 调用（包括派生类）从"静默 null 节点"变为"有效字面量节点"。这是 **bug 修复** 而非破坏，但属于语义变更 |
| 迁移影响 | 下游用户（chipforge 等）若曾用 `if (b.impl() == nullptr)` 做存在性检查应迁移——但这是 anti-pattern，本就是 bug 利用 |
| 性能 | 无变化（新增 ctor 路径与现有 `ch_literal_runtime` ctor 路径等价） |
| 风险 | **HIGH（不是 MEDIUM）**。原 Oracle 审计低估——漏洞在生产代码中已 live，22 个 bundle 类都可触发 |
| 回滚 | `git revert` 单一 commit；`bundle_base<Derived>(0)` 恢复静默 null 节点（pre-fix 行为） |
| 关联 commit | P0 `47af57f` 修复 `ch_uint`/`ch_bool`；本次修复同类漏洞在 `bundle_base` 上的入口 |
| 文档变更 | CHANGELOG/release-notes（行为变更记录）；下游迁移指南 |
| CI 验证 | `openspec validate --specs --strict` pre-archive；`./run_all_ported_tests.sh`（28 个 main() 示例验证） |