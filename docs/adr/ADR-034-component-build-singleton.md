# ADR-034: 组件构建非幂等性分析

**状态**: ✅ 已采纳
**日期**: 2026-05-11
**决策人**: Sisyphus + 用户（Oracle 分析参考）

---

## 1. 背景

议题 #28 审查 `Component::build()` 的 `built_` 标志。该标志使 `build()` 为**单次调用**，不可重复构建（非幂等）。

## 2. 分析

### 2.1 `built_` 预防的故障

Oracle 确认 `built_` 守卫防止了两个真实故障：

1. **内存泄漏** — `build()` 在 `external_ctx` 为空时分配新的 `ch::core::context`（`component.cpp:115`）。无守卫时，第二次 `build()` 会覆盖 `ctx_`，泄漏之前分配的 context。
2. **逻辑重复** — `build_internal()` 调用 `create_ports()` 然后 `describe()`（`component.cpp:135-138`）。重复运行会重新注册端口并重新实例化子逻辑，产生损坏/重复的硬件图。

### 2.2 移动语义兼容

`built_` 在移动构造/赋值中正确传输到目标，在源中重置。这是必要的，因为源的 `ctx_` 也被置空（`component.cpp:52,88`），源必须显示为"未构建"状态。

### 2.3 合法场景

无合法 HDL 工作流需要重建：
- `ch_device<T>` → 构建一次 → 仿真或代码生成
- `ch_module<T>` → 在 `describe()` 内构建一次 → 连接端口

---

## 3. 决议

**保留现状，不做修改。** `built_` 守卫是防御性检查，防止意外重复调用 `build()`。

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-11 | v1.0 | 初始版本：确认 built_ 守卫合理，无实际非幂等问题 | Sisyphus |

---

**相关链接**:
- `include/component.h:93` — built_ 声明
- `src/component.cpp:96-122` — build() 实现
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #28
