# ADR-014: Bundle 反射系统 — 字段限制、`add_user()`、比特序

**状态**: ✅ 已采纳（决议已记录，修复任务推迟至单独会话）
**日期**: 2026-05-07
**决策人**: Sisyphus + 用户

---

## 1. 背景

Bundle 是 CppHDL 中用于分组信号的结构化类型（如 AXI 通道、Stream、Flow）。反射系统通过 `CH_BUNDLE_FIELDS_T` 宏在编译期收集字段元数据（字段名、偏移、宽度），支撑字段遍历、宽度计算、序列化、代码生成等能力。

议题 #9 存在三个已知问题：
1. `CH_BUNDLE_FIELDS_T` 有 **10 字段硬限制**
2. Bundle 字段 **缺失 `add_user()`** DAG 用户追踪
3. 比特序（bit ordering）**未明确指定**

---

## 2. 代码问题详细记录

### 2.1 Bug A：`HazardUnitBundle` 超出 10 字段限制（严重）

**文件**: `include/cpu/hazard/hazard_unit_bundle.h:161-169`

**问题代码**:
```cpp
CH_BUNDLE_FIELDS_T(
    id_rs1_addr, id_rs2_addr, ex_rd_addr, mem_rd_addr, wb_rd_addr,   // 字段 1-5
    ex_reg_write, mem_reg_write, wb_reg_write,                         // 字段 6-8
    mem_is_load, ex_branch, ex_branch_taken,                          // 字段 9-11 ← 只到此处
    ex_alu_result, mem_alu_result, wb_write_data,                     // 字段 12-14 ← 被静默丢弃
    rs1_data_raw, rs2_data_raw, forward_a, forward_b,                // 字段 15-18 ← 被静默丢弃
    rs1_data, rs2_data, stall_if, stall_id, flush_id_ex              // 字段 19-23 ← 被静默丢弃
)
```

**根因**: `include/core/bundle/bundle_meta.h:64-72` 的宏调度机制：

```cpp
#define CH_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

#define CH_BUNDLE_FIELDS_T(...)                                                \
    static constexpr auto __bundle_fields() {                                  \
        return std::make_tuple(CH_GET_NTH_ARG(                                 \
            __VA_ARGS__, CH_BUNDLE_FIELDS_T_10, CH_BUNDLE_FIELDS_T_9,          \
            ...                                                              \
            CH_BUNDLE_FIELDS_T_1, )(__VA_ARGS__));                            \
    }
```

当传入 23 个参数时，`CH_GET_NTH_ARG` 只返回第 11 个参数（而非 `CH_BUNDLE_FIELDS_T_N` 之一）。这导致只有前 10 个字段被注册为反射元数据，字段 11-23 **静默丢失**——`bundle_width_v`、序列化、连接验证均会得到错误结果。

**当前影响状态**: 该文件未被实例化（AGENTS.md 标注 "unused by pipeline"），bug 目前是**潜在**的。但一旦有代码实例化 `HazardUnitBundle<...>`，错误将静默发生而不报错，难以调试。

---

### 2.2 Bug B：`CH_BUNDLE_FIELDS`（非 `_T`）宏未定义

**文件**: `include/bundle/clock_reset_bundle.h:21`

**问题代码**:
```cpp
CH_BUNDLE_FIELDS(Self, clock, reset)  // CH_BUNDLE_FIELDS 从未被 #define
```

`CH_BUNDLE_FIELDS`（无 `_T` 后缀）从未在任何头文件中定义。`CH_BUNDLE_FIELDS_T` 是唯一存在的宏。

**当前影响状态**: 该文件是孤儿代码，从未被任何 `.cpp` 或聚合头文件包含，因此编译错误不会触发。但若有人尝试使用该文件，会在预处理阶段失败。

---

### 2.3 Q2：比特序未明确指定（低优先级）

**相关代码**: `include/core/bundle/bundle_base.h:268-312`

```cpp
void create_field_slices_from_node() {
    unsigned offset = 0;
    // 字段从 offset 0 开始顺序排列，无显式字节序说明
    std::apply(
        [&](auto &&...field_info) {
            (([&]() {
                field_ref = bits<W>(bundle_lnode, offset);
                offset += W;  // LSB-first 隐式约定
            }()), ...);
        },
        layout);
}
```

**现状**:
- 约定为隐式 LSB-first（第 0 个字段在 bit 0）
- 无文档、无注释、无类型级强制
- `bundle_layout.h` 和 `bundle_serialization.h` 使用同样的顺序偏移约定

**风险**: 若未来需要与明确指定 MSB-first 的外部协议交互，可能产生兼容性问题。

---

### 2.4 Q3：缺失 `add_user()` 追踪（已记录，延后处理）

**相关代码**:
- `include/core/lnodeimpl.h:141` — `add_user()` 定义在 `lnodeimpl` 基类上
- `include/core/io.h:851` — IO 端口连接时调用 `driver_impl->add_user()`
- **所有 bundle 文件** — 无 `add_user()` 调用

**现状**: Bundle 连接通过 `operator<<=` 进行方向感知的字段级连接（`bundle_base.h:86-134`），但每个字段的底层节点不追踪用户关系。这使得连接验证和调试工具无法追溯 Bundle 字段的消费者。

**决策**: 延后至 P2 级任务。当前字段级连接已能通过方向检查正确工作，`add_user()` 是增强功能而非紧急修复。

---

## 3. 决议

| 问题 | 决议 | 行动 |
|------|------|------|
| **Q1：10 字段限制** | **立即修复（单独任务）** | 创建专项任务重构 `CH_BUNDLE_FIELDS_T` 为 variadic 模板，解除字段数硬限制；同时审计所有使用 `CH_BUNDLE_FIELDS_T` 的文件，确保字段数正确注册 |
| **Q2：比特序** | **文档化（无需代码修改）** | 在 `bundle_meta.h`、`bundle_base.h`、`bundle_layout.h` 添加注释说明 LSB-first 约定；在 ADR 文档中明确记录比特序策略 |
| **Q3：`add_user()`** | **延后至 P2** | 在 `bundle_operations.h` 或新文件中添加 Bundle 字段的 `add_user()` 追踪支持，作为单独增强任务 |

---

## 4. 修复任务前置条件

### 4.1 Q1 修复的前置条件

1. `CH_BUNDLE_FIELDS_T` 重构为 variadic 模板，支持任意数量字段
2. 审计 `hazard_unit_bundle.h` 确认字段数（23 个）并修复宏调用
3. 验证 `bundle_width_v<HazardUnitBundle<...>>` 返回正确的总位宽（应包含全部 23 个字段）
4. 确认 `bundle_base::create_field_slices_from_node()` 能正确处理 >10 个字段的切片创建

### 4.2 建议的重构方案

使用 C++20 `consteval` 和可变参数模板替代 X-macro：

```cpp
template<typename Self, typename... Fields>
constexpr auto __bundle_fields() {
    return std::make_tuple(
        bundle_field<Self, Fields>{Fields::name, &Self::name}...
    );
}

#define CH_BUNDLE_FIELDS(...)                                                  \
    using bundle_base<Self>::bundle_base;                                      \
    static constexpr auto __bundle_fields() {                                  \
        return __bundle_fields_impl<Self, __VA_ARGS__>();                     \
    }
```

此方案无字段数限制，且编译期性能更优。

---

## 5. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Q1-Q3 分析与决议；详细记录 Bug A/B 的代码证据 | Sisyphus + 用户 |
| 2026-06-18 | v1.1 | **Q1 修复完成**：`CH_BUNDLE_FIELDS_T` 宏从 10 字段扩展到 **40 字段**（`CH_GET_NTH_ARG` + `CH_BUNDLE_FIELDS_T_21` ~ `_40`）。修复了 `HazardUnitBundle` 23 字段被静默丢弃 3 字段的潜在 bug。新增 `tests/test_bundle_large.cpp` 回归测试覆盖 1/20/23/25/40 字段边界。**未来工作**：变基到 C++20 variadic 模板可彻底解除限制（见 §4.2 方案）| Sisyphus |

---

## 6. 当前限制与未来工作

### 6.1 Q1 限制（已缓解）

**当前实现**: 宏支持 1~40 字段。已通过 `tests/test_bundle_large.cpp` 验证所有边界。

**已知不足**: 仍是 X-macro 实现，每次扩展需手动添加 20 个宏。编译期 `CH_GET_NTH_ARG` 深度随字段数线性增长，对 >40 字段的 Bundle 仍会静默截断。

**未来重构方案**（来自 §4.2，**非阻塞**）:
```cpp
template<typename Self, typename... Fields>
constexpr auto __bundle_fields() {
    return std::make_tuple(
        bundle_field<Self, Fields>{Fields::name, &Self::name}...
    );
}

#define CH_BUNDLE_FIELDS(...)                                                  \
    using bundle_base<Self>::bundle_base;                                      \
    static constexpr auto __bundle_fields() {                                  \
        return __bundle_fields_impl<Self, __VA_ARGS__>();                     \
    }
```

**触发条件**: 当任何 Bundle 超过 40 字段，或当 variadic 重构被列为低风险任务时再实施。当前 27 个 Bundle 中最大 23 字段，余量充足（17 字段）。

### 6.2 Q2 比特序（未启动）

比特序隐式 LSB-first 约定无文档注释，详见 §2.3。

### 6.3 Q3 add_user（已延期）

P2 延期任务，按 §2.4 当前决策不变。

---

**相关链接**:
- `include/core/bundle/bundle_meta.h:23-152` — `CH_BUNDLE_FIELDS_T` 宏定义（扩展到 1-40 字段）
- `include/core/bundle/bundle_base.h:268-312` — `create_field_slices_from_node()` 比特序实现
- `include/core/bundle/bundle_layout.h` — 字段布局计算（LSB-first 隐式约定）
- `tests/test_bundle_large.cpp` — 1/20/23/25/40 字段边界回归测试
- `examples/riscv-mini/src/hazard_unit_bundle.h:161-169` — **Bug A**（23 字段，现已支持）
- `include/bundle/clock_reset_bundle.h:21` — **Bug B**：使用未定义的 `CH_BUNDLE_FIELDS` 宏
- `include/core/lnodeimpl.h:141` — `add_user()` 定义
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #9