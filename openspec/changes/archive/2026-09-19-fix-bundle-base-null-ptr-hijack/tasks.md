# fix-bundle-base-null-ptr-hijack

## 1. 分析与审计阶段

- [x] 1.1 完成 Oracle 审计：扫描所有 `logic_buffer<T>` 派生类
      ✅ 已识别 3 个直接继承者（`ch_uint`, `ch_bool`, `bundle_base`）和 22 个 bundle 派生类
- [x] 1.2 确认 `bundle_base` 是唯一仍易受攻击的类
      ✅ `bundle_base<Derived>(0)` 仍可通过 null pointer constant 标准转换匹配 `bundle_base(lnodeimpl *node)`
- [x] 1.3 Metis 审查发现：Oracle 审计**错误地**声称派生类安全
      ✅ 实际 `CH_BUNDLE_FIELDS_T` 宏（`bundle_meta.h:155`）包含 `using bundle_base<Self>::bundle_base;`
      ✅ 22 个派生类通过该 `using` 暴露了 `bundle_base(lnodeimpl*)`
      ✅ Metis 已实证 `ch_stream<int>(0)` 和 `axi_lite_b_channel(0)` 产生 null impl
      ✅ 修复在 `bundle_base` 上添加 SFINAE ctor 后，**通过 `using` 自动覆盖全部派生类**

## 2. 实施修复

- [ ] 2.1 在 `include/core/bundle/bundle_base.h` 添加 SFINAE 受限的模板构造函数
      📝 参考 P0 commit `47af57f` 在 `ch_uint`/`ch_bool` 上的修复模式
      📝 **模板签名**（包含 `bool` 排除）：
      ```cpp
      template <typename IntT,
                typename = std::enable_if_t<
                    std::is_integral_v<std::decay_t<IntT>> &&
                    !std::is_same_v<std::decay_t<IntT>, bool>>>
      explicit bundle_base(IntT v, const std::string &name = "bundle_lit",
                           const std::source_location &sloc = std::source_location::current());
      ```
      📝 **实现**（无 `N`，使用 `ch_literal_runtime` 单参数 ctor 应用 `compute_width`）：
      ```cpp
      : logic_buffer<Derived>() {
          ch_literal_runtime lit(static_cast<std::uint64_t>(v));
          this->node_impl_ = node_builder::instance().build_literal(lit, name, sloc);
          if (!this->node_impl_) {
              CHERROR("[bundle_base] Failed to create literal node from integer value");
          }
      }
      ```
- [ ] 2.2 添加必要的 include（如未包含）
      📝 确认 `node_builder.h`、`literal.h` 已通过 include chain 可用（`literal.h` 已在 `bundle_base.h:8`）
      📝 `node_builder` 已在 `bundle_base.h:55,67` 使用，无需新增 include

## 3. 注册回归测试

- [ ] 3.1 创建 `tests/test_bundle_literal.cpp`
      📝 至少 6 个 TEST_CASE 覆盖以下场景：
      - `bundle_base<Derived>(0)` 在 active context 中 `b.impl() != nullptr`
      - `bundle_base<Derived>(42)` 同上
      - `bundle_base<Derived>(some_lnodeimpl_ptr)` 仍走原路径（`b.impl() == ptr`）
      - `bundle_base<Derived>(ch_literal_runtime(...))` 仍走原路径
      - `bundle_base<Derived>(false)` 无歧义（bool 排除验证）
- [ ] 3.2 在测试文件中加入派生类覆盖
      📝 至少 3 个派生类证明 `using` 继承自动修复：
      - `ch_stream<ch_uint<8>>(0).impl() != nullptr`
      - `ch_flow<ch_uint<8>>(0).impl() != nullptr`
      - `axi_lite_b_channel(0).impl() != nullptr`
- [ ] 3.3 在 `tests/CMakeLists.txt` 注册新测试
      📝 `add_catch_test(test_bundle_literal test_bundle_literal.cpp)`

## 4. 验证测试

- [ ] 4.1 编译验证
      📝 `cmake --build build -j$(nproc)` 无 error/warning
      📝 22 个 bundle 派生类仍能正常编译
- [ ] 4.2 回归测试
      📝 新 `test_bundle_literal` 全部通过
      📝 P0 `test_chipforge_mux_segv_repro` 仍 5/5 通过（确认无回归）
- [ ] 4.3 全测试套件
      📝 `ctest -E perf_tests` 所有测试通过（pre-existing Verilator 环境失败不计）
- [ ] 4.4 示例程序验证
      📝 `./run_all_ported_tests.sh`（28 个 main() 示例，覆盖 ch_stream、axi_lite_bundle 等的实际使用）

## 5. 文档与变更记录

- [ ] 5.1 CHANGELOG / release notes
      📝 记录行为变更：`bundle_base<Derived>(整数字面量)` 从"静默 null 节点"变为"有效字面量节点"
      📝 这影响所有 22 个 `CH_BUNDLE_FIELDS_T` 派生类
- [ ] 5.2 下游迁移指南
      📝 chipforge 等下游用户的迁移说明：若曾用 `if (b.impl() == nullptr)` 做存在性检查应迁移（anti-pattern）
- [ ] 5.3 OpenSpec 校验
      📝 `openspec validate fix-bundle-base-null-ptr-hijack --strict` pre-archive
- [ ] 5.4 提交变更
      📝 commit message 风格与 P0 `47af57f` 保持一致
- [ ] 5.5 运行 `openspec archive fix-bundle-base-null-ptr-hijack`
      📝 把 `specs/bundle-base-integer-ctor/spec.md` 合并到 `openspec/specs/bundle-base-integer-ctor/spec.md`