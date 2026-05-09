# ADR-006: 错误处理策略（CHREQUIRE/CHCHECK/CHABORT 语义）

**状态**: 草稿（待评审）  
**日期**: 2026-05-06  
**决策人**: QoderWork Architecture Review  
**优先级**: P0  
**相关文档**: `include/utils/logger.h`, `docs/adr/prd.md` (已知架构债务 T3)

---

## 1. 背景

CppHDL 定义了多种错误处理宏（`CHREQUIRE`、`CHCHECK`、`CHABORT`、`CHFATAL` 等），但它们的语义不统一，且存在一个关键设计问题：**大多数检查宏在失败时只记录错误日志，不终止程序执行**。这导致错误被静默忽略，下游代码使用无效状态继续运行，产生难以调试的二次错误。

本文档记录当前的错误处理策略、各个宏的正确语义，以及为什么采用这种设计。

---

## 2. 架构决策

### D1: 错误处理宏分级使用

**决策**: 根据错误的严重程度，使用不同级别的错误处理宏：

| 宏 | 语义 | 失败行为 | 使用场景 |
|---|------|---------|---------|
| `CHABORT` | **无条件终止** | `std::abort()` | 不可恢复的内部错误 |
| `CHFATAL_EXCEPTION` | **致命错误** | 记录日志 + 抛出 `std::runtime_error` | 可捕获的致命错误 |
| `CHFATAL_RECOVERABLE` | **致命但可恢复** | 记录日志，不终止 | 需要记录但不想崩溃的场景 |
| `CHREQUIRE` | **前置条件检查** | 记录日志，**不终止** | 函数入口的参数/状态检查 |
| `CHCHECK` | **不变量检查** | 记录日志，**不终止** | 函数内部的中间状态检查 |
| `CHENSURE` | **后置条件检查** | 记录日志，**不终止** | 函数返回前的结果检查 |
| `CHCHECK_NULL` | **空指针检查** | 记录日志，**不终止** | 指针有效性检查 |
| `CHERROR` | **错误日志** | 仅记录 | 错误报告和诊断 |
| `CHWARN` | **警告日志** | 仅记录 | 潜在问题提醒 |

**理由**:
- **`CHABORT`** 用于绝对不应该发生的情况（如内存损坏、内部状态不一致），此时继续执行可能导致更严重的后果
- **`CHFATAL_EXCEPTION`** 用于可以抛出异常让调用者处理的致命错误
- **`CHREQUIRE`/`CHCHECK`** 等检查宏设计为**非终止**，是为了在仿真过程中尽可能多地收集错误信息，而不是在第一个错误时就崩溃

### D2: 检查宏不终止执行的原因

**决策**: `CHREQUIRE`、`CHCHECK`、`CHENSURE` 在失败时**只记录错误日志，不终止程序**。

**理由**:
1. **调试友好**: 在 HDL 仿真中，一个设计可能有多个错误。非终止检查可以让用户在一次运行中看到所有错误，而不是每次只看到一个
2. **容错运行**: 某些错误可能是预期的（如输入值超出范围），用户希望仿真继续运行
3. **历史原因**: CppHDL 早期设计受到 Verilog 仿真器的影响，Verilog 倾向于报告错误但继续运行

**后果**:
- ✅ 用户可以一次看到多个错误
- ✅ 仿真器对无效输入更容忍
- ❌ **错误被静默忽略**：调用者不知道检查失败了，继续使用无效状态
- ❌ **二次错误**：下游代码使用无效状态产生更多错误，掩盖了真正的根因
- ❌ **测试结果不可信**: 测试可能在应该失败的情况下通过了

**示例**:
```cpp
// 当前行为：CHREQUIRE 失败但继续执行
void some_function(int value) {
    CHREQUIRE(value >= 0, "value must be non-negative, got %d", value);
    // 如果 value < 0，上面只记录日志，继续执行
    int result = lookup_table[value];  // 数组越界！二次错误
}
```

### D3: CHABORT 用于不可恢复的错误

**决策**: `CHABORT` 是唯一无条件终止程序的宏，用于内部状态严重损坏的情况。

**当前使用场景**:
- Simulator 未初始化时访问端口值
- 端口实现为空（严重的设计错误）
- Bundle 值获取时 Simulator 未初始化

**示例**:
```cpp
// simulator.h: 获取端口值
template <typename T, typename Dir>
const ch::core::sdata_type get_port_value(const ch::core::port<T, Dir> &port) const {
    if (!initialized_) {
        CHABORT("Simulator not initialized when getting port value");  // 终止
    }
    auto *node = port.impl();
    if (!node) {
        CHABORT("Port implementation is null");  // 终止
    }
    // ...
}
```

**理由**:
- 这些情况表示**调用者严重违反了 API 契约**，继续执行没有意义
- 早期失败比静默产生错误结果更好

### D4: CHFATAL_EXCEPTION 用于可传播的致命错误

**决策**: `CHFATAL_EXCEPTION` 记录日志后抛出 `std::runtime_error`，允许调用者捕获和处理。

**使用场景**:
- 需要跨栈帧传播的错误
- 测试框架需要捕获异常来验证错误行为
- 库代码应该将错误报告给调用者，而不是自行终止

**当前状态**: 此宏已定义但**使用较少**，大多数代码倾向于使用 `CHABORT` 或 `CHERROR`。

### D5: 静态销毁阶段的错误处理抑制

**决策**: 所有错误处理宏在静态销毁阶段（`in_static_destruction()` 返回 true 时）**不执行任何操作**。

**理由**:
- 静态对象销毁时，日志系统可能已经部分销毁
- 在静态销毁阶段输出错误可能导致 segfault
- `debug_context_lifetime()` 标志用于防止静态销毁期间的访问

**实现**:
```cpp
// logger.h
#define CHREQUIRE(condition, fmt, ...)                                         \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            // 只有不在静态销毁阶段才记录错误
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Requirement failed [%s]: " fmt, #condition, ##__VA_ARGS__);   \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)
```

---

## 3. 当前问题（P0 级架构债务）

### 3.1 问题：断言宏不终止执行

**PRD T3 任务**: 断言宏重新设计

**症状**:
```cpp
// 期望失败并终止
CHREQUIRE(node != nullptr, "node cannot be null");
node->do_something();  // 如果 node 是 null，这里会崩溃

// 实际行为
// 1. CHREQUIRE 失败，记录日志
// 2. 继续执行
// 3. node->do_something() 崩溃（二次错误）
```

**影响**:
- 错误根因被掩盖（真正的错误是 `node == nullptr`，但崩溃发生在 `do_something()` 内部）
- 调试困难（需要追踪到几层调用之前的失败检查）
- 测试不可信（测试可能在应该失败的情况下通过了）

### 3.2 修复方案对比

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **方案 A**: `CHREQUIRE` 改为抛出异常 | 调用者可捕获，错误可传播 | 改变现有行为，可能破坏依赖容错运行的代码 | ⭐⭐⭐ |
| **方案 B**: 添加 `CHASSERT` 宏（终止），保留 `CHREQUIRE`（不终止） | 向后兼容，语义清晰 | 增加宏的数量，开发者需要选择用哪个 | ⭐⭐⭐⭐ |
| **方案 C**: 环境变量控制是否终止 | 运行时可配置 | 增加复杂性，测试结果依赖环境配置 | ⭐⭐ |
| **方案 D**: `CHREQUIRE` 改为 `std::terminate()` | 简单，错误立即暴露 | 完全改变现有行为，可能破坏大量代码 | ⭐⭐ |

**推荐方案 B**: 添加 `CHASSERT` 宏用于需要终止的检查，保留 `CHREQUIRE` 用于容错检查。

**实施计划（PRD T3）**:

| 阶段 | 内容 | 预计时间 | 依赖 |
|------|------|---------|------|
| 1. 宏定义 | 在 `include/utils/logger.h` 中添加 `CHASSERT` 宏 | 2h | 无 |
| 2. 关键路径替换 | 将 Simulator、JIT、context 中的关键 `CHREQUIRE` 替换为 `CHASSERT` | 4h | 阶段 1 |
| 3. 测试更新 | 更新依赖错误行为的测试，确保 `CHFATAL_EXCEPTION` 用于可捕获场景 | 2h | 阶段 2 |
| 4. 代码审查 | 逐文件审查替换正确性，确保不破坏容错逻辑 | 2h | 阶段 3 |

**执行时间**: Phase 1（稳定性修复阶段），与 T1（时钟边沿检测）和 T4（64 位掩码 UB）并行进行。

**CHASSERT 设计**:
```cpp
#define CHASSERT(condition, fmt, ...)                                          \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Assertion failed [%s]: " fmt, #condition, ##__VA_ARGS__);     \
            ch::detail::log_message(ch::log_level::fatal, msg, loc);           \
            std::abort();                                                      \
        }                                                                      \
    } while (0)
```

---

## 4. 错误处理最佳实践

### 4.1 应该使用哪个宏？

| 场景 | 推荐宏 | 示例 |
|------|--------|------|
| 函数入口的前置条件 | `CHREQUIRE` 或 `CHASSERT` | `CHREQUIRE(ctx != nullptr, "context required")` |
| 内部不变量 | `CHCHECK` | `CHCHECK(state == valid, "invalid state")` |
| 函数返回的后置条件 | `CHENSURE` | `CHENSURE(result > 0, "result must be positive")` |
| 不可恢复的内部错误 | `CHABORT` | `CHABORT("internal error: unreachable")` |
| 需要传播的致命错误 | `CHFATAL_EXCEPTION` | `CHFATAL_EXCEPTION("JIT compilation failed")` |
| 错误报告（不终止） | `CHERROR` | `CHERROR("failed to open file: %s", path)` |
| 潜在问题提醒 | `CHWARN` | `CHWARN("JIT not available, using interpreter")` |

### 4.2 静态销毁保护

所有错误处理宏在静态销毁阶段不执行任何操作。这是为了保护程序在退出时不因为日志系统已销毁而崩溃。

```cpp
// ✅ 正确：检查条件 + 格式化消息 + 位置信息 + 静态销毁保护
CHREQUIRE(ptr != nullptr, "pointer cannot be null");

// ❌ 错误：不应该在静态销毁敏感区域使用
// （但宏会自动处理，无需手动检查）
```

### 4.3 错误传播

```cpp
// ✅ 正确：使用 CHFATAL_EXCEPTION 传播错误
void compile_jit() {
    if (!llvm_available) {
        CHFATAL_EXCEPTION("LLVM not available for JIT compilation");
    }
}

// ✅ 正确：调用者可以捕获异常
try {
    compile_jit();
} catch (const std::runtime_error& e) {
    CHWARN("JIT compilation failed: %s, falling back to interpreter", e.what());
    use_interpreter();
}
```

---

## 5. 与竞品的对比

| 工具 | 错误处理策略 | 特点 |
|------|-------------|------|
| **Verilog 仿真器** | 报告错误但继续运行 | 适合收集所有错误 |
| **Chisel** | 编译期检查 + 运行时异常 | 错误在 elaboration 阶段暴露 |
| **SpinalHDL** | 编译期检查 + 运行时异常 | 类似 Chisel |
| **CppHDL（当前）** | 检查宏不终止 | 类似 Verilog，但容易掩盖根因 |
| **CppHDL（目标）** | `CHASSERT` 终止 + `CHREQUIRE` 容错 | 兼顾调试友好和错误暴露 |

---

## 6. 技术债务

| 债务 | 影响 | 修复计划 | 参考 |
|------|------|---------|------|
| `CHREQUIRE` 不终止执行 | 错误被静默忽略 | PRD T3: 添加 `CHASSERT` 宏 | §3.2 |
| `CHCHECK_NULL` 不终止 | 空指针检查无意义 | 与 T3 一起修复 | — |
| 宏体系过于精细（9 个宏） | 开发者选择困难，`CHFATAL_EXCEPTION` vs `CHFATAL_RECOVERABLE` 边界模糊 | 考虑合并为 6-7 个宏，删除或合并使用率低的宏 | 未来评审 |
| 错误码未使用 | `error_code` 枚举定义了但很少使用 | 考虑删除或统一使用 | — |
| 日志格式不统一 | 有些宏显示位置信息，有些不显示 | 统一日志格式 | — |
| 没有错误恢复机制 | 错误发生后无法恢复到有效状态 | 未来改进 | — |

---

## 7. 决策影响

### 对开发者的影响

1. **新代码**: 优先使用 `CHASSERT` 进行关键检查，使用 `CHREQUIRE` 进行容错检查
2. **现有代码**: 逐步将关键路径的 `CHREQUIRE` 替换为 `CHASSERT`
3. **测试**: 测试错误行为时，使用 `CHFATAL_EXCEPTION` 以便捕获异常

### 对用户的影响

1. **错误报告**: 用户会看到更清晰的错误信息（根因而不是二次错误）
2. **崩溃行为**: 某些原来继续运行的情况现在会终止（这是预期的改进）
3. **调试体验**: 更容易定位真正的错误原因

---

## 8. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录错误处理策略和已知问题 | QoderWork |
| 2026-05-06 | v1.1 | 添加 T3 实施计划时间线，补充宏体系简化建议和 PRD 任务引用 | QoderWork Review |

---

**相关链接**:
- `include/utils/logger.h` — 错误处理宏定义
- [PRD - 已知架构债务 T3](prd.md)
- [ADR-003: JIT 编译器架构](ADR-003-jit-compiler-architecture.md) — JIT 模块中的错误处理策略
- [ADR-004: 数据所有权和生命周期](ADR-004-data-ownership-lifecycle.md) — 静态销毁阶段的错误处理保护
- [ADR-004: 数据所有权和生命周期](ADR-004-data-ownership-lifecycle.md)

---

**决策人**: AI Agent  
**审阅人**: ________________  
**状态**: 草稿（待评审）
