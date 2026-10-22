# ADR-043: 移除 bundle 的「拼接 / 切片」设施（`bundle_concat` / `bundle_slice_view`）

- Status: Accepted
- Date: 2026-10-22
- Author: build-agent
- Parent: ADR-002（bundle 连接设计缺陷修复）、ADR-014（bundle 反射系统）、ADR-042（bundle 字段改为一等节点）
- OpenSpec change: `bundle-ecosystem-hardening-2026-10`（capability `bundle-master-slave-io`）

## Context

ADR-042 把 bundle 字段改成**一等节点**（不再是「总线 + 位切片」的派生）。
那次改动只覆盖了「单个 bundle、叶子字段、master↔slave 直连」这条**主路径**；
bundle 生态的**边角**设施在 2026-10-22 的全仓审计中被逐项检查，其中两个
**公开类型**的结论是「它们已经**静默失效**，且没有消费者需要它们」。

### `bundle_concat` —— 静默失效 + 语义自相矛盾

```cpp
template <typename Bundle1, typename Bundle2>
struct bundle_concat : public bundle_base<bundle_concat<Bundle1, Bundle2>> {
    Bundle1 bundle1;
    Bundle2 bundle2;
    CH_BUNDLE_FIELDS_T(bundle1, bundle2)
    void set_name_prefix(const std::string &prefix) override {
        bundle1.set_name_prefix(prefix + ".first");
        bundle2.set_name_prefix(prefix + ".second");
    }
};
```

1. **字段与原 bundle 断连**：ADR-042 之后字段是**自己的**节点（由
   `as_master()` / `as_slave()` 建立）；而 `bundle_concat` 的两个字段是
   **被包含的 bundle 对象**，`CH_BUNDLE_FIELDS_T(bundle1, bundle2)` 把它们
   当作「叶子字段」登记 ⇒ `create_field_proxy_nodes()` 会跳过它们
   （`is_bundle_v` 分支）⇒ 它们的节点由**各自**的 `as_master()` 建立，
   而 `bundle_concat::as_master_direction()` 调的正是 `bundle1.as_master()` ⇒
   字段节点**存在**，但**不在 `bundle_concat` 的位布局里被正确表达**
   （`get_bundle_width` 会把它们算进去，字段却不是它自己的）。
2. **唯一入口是按值拷贝**：`bundle_cat(Bundle1 b1, Bundle2 b2)` 走
   `std::move` ⇒ 副本与原件**共享字段节点** ⇒ 「拼接」的语义自相矛盾
   （副本上的写入会同时改原件）。
3. **零存活测试**：唯一引用点 `tests/test_bundle_advanced_features.cpp:72`
   位于**被注释掉**的 `/* */` 块内 ⇒ 实际零覆盖。
4. **它是 prefix 语义不统一的活例**：它 override 了 `set_name_prefix()` 且
   **不下推** prefix ⇒ 「嵌套 bundle 拿到什么名字」取决于派生类 override
   是否恰好调用了基类实现。

### `bundle_slice_view` —— 根本不做切片

```cpp
template <size_t Start, size_t Count, typename BundleT>
struct bundle_slice_view : public bundle_base<bundle_slice_view<Start, Count, BundleT>> {
    BundleT original_bundle;
    // constexpr auto __bundle_fields() const { ... slice_fields<Start, Count>(all_fields); }
};
```

- `Start` / `Count` 是**死参数**：`slice_fields()` 是**从未被实例化**的成员
  模板 ⇒ 真正的 `__bundle_fields()` 仍返回**整个**原 bundle 的字段
  ⇒ 它**根本不做切片**，宽度 = 整个原 bundle 的宽度。
- 它的 `C > 3` 的 `static_assert` 今天**不会触发**（模板未实例化），
  但**一旦**被实例化就会编译期拒止 ⇒ 「让它做切片」= **新增能力**。
- 零存活测试（同上）。

⇒ 两者都属于「看起来能用、实则静默失效」的公开 API。

## Decision

1. **物理移除** `bundle_concat` 与 `bundle_slice_view`（两个公开类型），
   **以及**它们的公开工厂 `bundle_cat()` / `bundle_slice()`
   —— 工厂是这两个类型的**唯一入口**，不一并移除则调用方一使用即触发
   deprecation 告警（仓库先例：`deprecate-hdl-logical-operators` 首轮
   315 warning 回滚）。
2. **MUST NOT** 只标记 `deprecated` 后继续留在公开派生类清单里 ——
   那会让「已废弃但仍计入清单」与 capability 的清单要求**并存矛盾**。
3. **MUST NOT** 顺手「实现真正的切片」—— 那是**新增能力**，
   不在本次的范围（Non-Goal）。若将来需要切片能力，MUST 另立 ADR，
   并从「字段一等节点」原则出发设计（切片应表现为**独立的 bundle 类型**，
   而不是「持有原 bundle 的视图」）。
4. 移除**同步** `bundle-base-integer-ctor` capability 的**派生类清单**
   （22 → 20）。

## Consequences

### 正向

- 公开派生类清单从 **22** 降到 **20**（`include/` 18 + `examples/riscv-mini/src/` 2）。
- 「prefix 语义统一」不再有活例：嵌套 bundle 的 prefix 一律走**非虚**通道下推。
- 消除「看起来能用、实则静默失效」的公开 API（ZERO-DEBT POLICY 的
  「No dead code」）。

### 负向 / 迁移

- **破坏性变更**：任何使用 `bundle_cat()` / `bundle_slice()` 的外部代码
  MUST 改为直接使用被包含的 bundle 本身（拼接语义在本次被裁决为
  **无消费者需要**；切片语义 MUST 另立 ADR）。
- 仓库内**零存活消费者** ⇒ 仓库内无需迁移。

### 被取代的债务

ADR-042 的「遗留债务」段把 `bundle_concat` / `bundle_slice_view` 记为
「独立 follow-up」⇒ **本 ADR 就是那个 follow-up**。

## Alternatives considered

| 方案 | 结论 |
|---|---|
| **A. 只标记 deprecated** | ❌ 拒绝 —— 「已废弃但仍计入清单」与 capability 的清单要求并存矛盾；且工厂是唯一入口，不移除则一用即告警。 |
| **B. 保留并「实现正确语义」** | ❌ 拒绝 —— 「正确语义」要先回答「副本写回原件吗」这类**没有消费者需要**的问题；「让切片真的切片」属**新增能力**。 |
| **C. 物理移除**（本 ADR） | ✅ 采纳 —— 零存活消费者 + 零语义价值 + 消除静默失效。 |

## References

- `openspec/changes/bundle-ecosystem-hardening-2026-10/proposal.md`（D2）
- `openspec/specs/bundle-master-slave-io/spec.md`（Requirement:
  「`bundle_concat` / `bundle_slice_view` MUST 被移除」）
- `docs/adr/ADR-042-bundle-field-first-class-nodes.md`（本 ADR 取代其
  「遗留债务」条目）
