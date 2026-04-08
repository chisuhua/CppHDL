# ADR-002: Bundle 连接设计缺陷修复

**日期**: 2026-04-08  
**状态**: 已记录，待修复  
**优先级**: P0 (阻塞性)  

---

## 背景

在 `test_bundle_connection` 测试中发现连接失败问题。

### 当前状态

| 测试用例 | 状态 |
|----------|------|
| Bundle 连接 in module | ❌ 失败 |
| 其他 Bundle 测试 | ✅ 通过 (16个) |

---

## 决策

### 问题

```cpp
void describe() override {
    io().input_bundle.as_slave();    // 创建字段节点
    io().output_bundle.as_master();  // 创建字段节点
    
    // ❌ 错误：字段已有节点，无法使用 operator= 或 <<
    io().output_bundle.data = io().input_bundle.data;
}
```

**错误**:

```
[ERROR] [ch_uint::operator<<=] Error: node_impl_ or src_lnode is not null for ch_uint<4>!
```

### 根因

1. `as_master/as_slave()`创建字段节点
2. `operator=<<` 要求目标字段无节点
3. `operator=` 使用 `operator=<<` 或`set_src`，但实现不统一

### 修复方案对比

| 方案 | 工时 | 风险 | 推荐度 |
|------|------|------|----------|
| **方案 A**: 修改 connect 使用 set_src | 8h | 中 | ✅ 推荐 |
| 方案 B: 重构 operator= 统一行为 | 16h | 高 | ❌ |

### 方案 A 细节

```cpp
template <typename BundleT> void connect(BundleT &src, BundleT &dst) {
    const auto &fields = src.__bundle_fields();
    
    std::apply([&](auto &&...f) {
        ([&, f_ptr = f.ptr]() {
            using FieldT = std::decay_t<decltype(src.(*f_ptr))>;
            if constexpr (!is_bundle_v<FieldT>) {
                auto* dst_impl = (dst.*f_ptr).impl();
                auto* src_impl = (src.*f_ptr).impl();
                if (dst_impl && src_impl) {
                    dst_impl->set_src(0, src_impl);
                }
            }
        }(), ...);
    }, fields);
}
```

**挑战**:
1. C++20 fold expression 语法 (第 331 行编译错误)
2. `src.(*f_ptr)` 语法需要修正
3. 需要处理嵌套 Bundle

### 暂时规避

```cpp
// 临时规避：直接使用底层 set_src
auto* dst_impl = io().output_bundle.data.impl();
auto* src_impl = io().input_bundle.data.impl();
if (dst_impl && src_impl) {
    dst_impl->set_src(0, src_impl);
}
```

---

## 后果

### 积极
- 文档清晰
- 问题已跟踪
- 修复路径明确

### 消极
- 测试未修复
- CI/CD 阻塞
- 影响用户信心

### 技术债务

- 累积技术债务：2 小时
- 修复优先级：P0
- 最后更新：2026-04-08

---

## 相关文档

- [docs/plans/bundle-connection-fix-plan.md](bundle-connection-fix-plan.md)
- [docs/problem-reports/bundle-connection-issue.md](bundle-connection-issue.md)

---

**决策人**: AI Agent  
**审阅人**: ________________  
**状态**: ◐ 已记录，未修复
