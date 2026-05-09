# ADR-012: IO 端口双 API — `__io` 宏与 `ch_in<T>`/`ch_out<T>` 统一

**状态**: 🔍 待深入审查  
**日期**: 2026-05-07  
**决策人**: —（推迟决策）  
**更新**: 2026-05-08 — 经多仿真场景评估，维持延期决定（详见 ADR-018 §4.3）  

---

## 1. 背景

CppHDL 中存在两套 IO 端口声明机制，均定义在 `include/core/io.h` 中：

**机制 A — `__io` 宏**（`io.h:319-326`）:
```cpp
#define __io(...)                                                              \
    struct io_type { __VA_ARGS__; };                                           \
    alignas(io_type) char io_storage_[sizeof(io_type)];                        \
    [[nodiscard]] io_type &io() {                                              \
        return *reinterpret_cast<io_type *>(io_storage_);                      \
    }
```

**机制 B — `ch_in<T>`/`ch_out<T>` 类型别名**（`io.h:301-302`）:
```cpp
template <typename T> using ch_in = port<T, input_direction>;
template <typename T> using ch_out = port<T, output_direction>;
```

以及配套的便利宏（`io.h:338-339`）:
```cpp
#define __in(...) ch::core::ch_in<__VA_ARGS__>
#define __out(...) ch::core::ch_out<__VA_ARGS__>
```

---

## 2. 现状分析

### 2.1 使用规模

通过全代码库搜索获得精确数据：

| 模式 | 出现次数 | 涉及文件 |
|------|---------|---------|
| `__io(` 宏调用 | ~240 次 | ~100 个源文件 |
| `ch_in<` 出现 | ~1312 次 | 160 个文件 |
| `ch_out<` 出现 | ~1146 次 | 157 个文件 |
| `ch_in<`/`ch_out<` **在 `__io()` 外部**（独立具名端口） | ~80 处 | ~15 个测试/示例文件 |
| `ch_in<`/`ch_out<` **在 `__io()` 内部** | ~2378 处 | ~85 个组件文件 |
| `#define __io` | 1 处 | `include/core/io.h:319` |

**关键发现**：`ch_in<T>`/`ch_out<T>` 在代码库中的 ~2400 次出现中，**约 99% 在 `__io()` 宏内部**。独立使用仅约 80 处（主要为测试中的具名端口构造）。

### 2.2 `__io` 宏使用分类

| 类别 | 文件数 | 说明 |
|------|-------|------|
| **(a) 生产组件头文件** | ~45 文件 | `include/axi4/*`, `include/cpu/pipeline/*`, `include/cpu/riscv/*`, `include/cpu/cache/*`, `include/chlib/*` |
| **(b) 示例组件** | ~25 文件 | `examples/spinalhdl-ported/*`（17 个移植 SpinalHDL 设计）, `examples/axi4/*`, `examples/riscv-mini/src/*` |
| **(c) 测试文件** | ~20 文件 | `tests/test_*.cpp`, `tests/chlib/test_*.cpp` |
| **(d) 示例代码** | ~12 文件 | `samples/*.cpp` |

### 2.3 `__io` 宏的技术问题

| 问题 | 位置 | 严重性 |
|------|------|--------|
| `reinterpret_cast` 违反严格别名规则（UB） | `io.h:325` | 高 — 技术上未定义行为 |
| `__io` 双下划线前缀为编译器保留（C++ §17.6.4.3.2） | `io.h:319` | 中 — 实践中工作，但违反标准 |
| 不捕获字段名：所有端口在 Verilog 输出中获默认名 `"io"`, `"io_1"` | T002 报告 | 中 — 影响 Verilog 可读性 |
| 每个组件需手写 `create_ports() { new (io_storage_) io_type; }` | 全部 ~64 个组件 | 低 — 样板代码重复 |
| 无 RAII：`io_storage_` 缓冲区内容不会自动构造/析构 | 设计使然 | 低 — 由 `create_ports()` 管理 |

---

## 3. 架构约束分析

### 3.1 两阶段初始化的根因

端口不能是普通的 C++ 类成员，原因在于**两阶段初始化生命周期**：

```
Component 构造
    ↓ 此时 ctx_curr_ 尚未建立
build() 被调用
    ↓ ctx_swap 建立上下文
create_ports() 被调用
    ↓ placement new 构造 io_storage_ 中的端口节点
describe() 被调用
    ↓ 可安全访问 io().port_name
```

`ch_in<T>`/`ch_out<T>` 的默认构造函数会通过 `ctx_curr_` 创建 `inputimpl`/`outputimpl` DAG 节点。如果在 `build()` 之前（ctx=null）构造，端口永远成为空节点，且没有"重新初始化"路径。

因此 `__io` 的 placement new + `create_ports()` 模式是**刻意为之的架构设计**，而非偶然。

### 3.2 `Component` 基类无 `io()` 方法

`include/component.h:16-101` 中 `Component` 基类**不定义 `io()`**。`io()` 方法完全由 `__io` 宏在每个子类中生成。`ch_device<T>::io()`（`device.h:27`）和 `ch_module<T>::io()`（`module.h:79`）都通过 `auto&` 推导调用 `T::io()`，因此依赖 `__io` 生成的 `io()` 方法。

### 3.3 `port<T, Dir>` 与方向系统

`port<T, Dir>` 有两个显式特化：
- `port<T, input_direction>`（`io.h:193`） — 包装 `ch_logic_in<T>`，删除 `operator=`，提供只读访问
- `port<T, output_direction>`（`io.h:250`） — 包装 `ch_logic_out<T>`，`operator=` 委托写入

方向由三个空标签类型（`direction.h:9-11`）在编译期确定：`input_direction`、`output_direction`、`internal_direction`。

---

## 4. 方案分析

### 4.1 方案一：修复宏（修复 UB + 吸收样板）

**内容**：
- 用 `std::launder` 替换裸 `reinterpret_cast`
- 将 `__io` 重命名为 `CH_IO`（避免双下划线保留标识符）
- 将 `create_ports()` 的 placement new 吸收进宏定义
- 保留 `__io` 作为向后兼容别名

**预期实现**：
```cpp
#define CH_IO(...)                                                         \
    struct io_type { __VA_ARGS__; };                                       \
    alignas(io_type) char io_storage_[sizeof(io_type)];                    \
    [[nodiscard]] io_type &io() {                                          \
        return *std::launder(reinterpret_cast<io_type*>(io_storage_));     \
    }                                                                      \
    void create_ports() override {                                         \
        std::construct_at(reinterpret_cast<io_type*>(io_storage_));        \
    }

#define __io(...) CH_IO(__VA_ARGS__)  // 向后兼容别名
```

| 维度 | 评估 |
|------|------|
| **改动范围** | 仅 2 文件（`io.h` + 可能 `component.h`） |
| **用户影响** | 零 — 向后兼容别名保持所有现有代码可用 |
| **消除问题** | 修复 `reinterpret_cast` UB + `__io` 保留标识符 + 消除 ~64 处样板重复 |
| **剩余问题** | 仍不捕获字段名（T002 未解决）；仍基于宏而非类型系统 |

### 4.2 方案二：直接成员端口（废除 `__io`，无 `io()`）

**内容**：端口成为直接类成员，通过 `this->port_name` 而非 `io().port_name` 访问。需要修改 `ch_device<T>`/`ch_module<T>` 的 `io()` 方法。

| 维度 | 评估 |
|------|------|
| **改动范围** | ~111 个文件全部修改 |
| **架构障碍** | **不可行** — 端口在 `build()` 之前构造时 ctx=null，永远成为空节点。端口需要延迟初始化或重新初始化能力，这需要修改整个 `port<T,Dir>` 类型体系 |
| **用户影响** | 破坏性变更 — 所有 `io().xxx` 需改为 `this->xxx` |
| **结论** | **架构上被阻塞**，不作为可行选项 |

### 4.3 方案三：Bundle 类型 + `std::optional<IO>` 成员

**内容**：为每个组件定义一个具名 Bundle 类型，组件通过 `std::optional<IO>` 成员持有，`create_ports()` 中 `emplace()`。

**预期实现**：
```cpp
struct HazardUnitIO {
    ch_in<ch_uint<5>> id_rs1_addr;
    ch_out<ch_bool> branch_taken;
};

class HazardUnit : public ch::Component {
    std::optional<HazardUnitIO> io_opt_;
public:
    HazardUnitIO& io() { return *io_opt_; }
    void create_ports() override { io_opt_.emplace(); }
};
```

| 维度 | 评估 |
|------|------|
| **改动范围** | ~111 个文件 + 每个组件需要新的 IO 结构体定义 |
| **优点** | 消除宏、类型安全、字段名保留（可反射） |
| **缺点** | 迁移工作量大；`ch_in<T>`/`ch_out<T>` 默认构造在 `std::optional::emplace()` 内部触发，同样需要活跃的 ctx |
| **可行性** | 技术上可行但迁移成本高，属于远期方向 |

### 4.4 方案四：CRTP `ComponentWithIO<IO>`

**内容**：引入带类型的模板基类，消除宏和 `create_ports()` 样板。

**预期实现**：
```cpp
template<typename IO>
class ComponentWithIO : public Component {
protected:
    std::optional<IO> io_opt_;
public:
    IO& io() { return *io_opt_; }
    void create_ports() override { io_opt_.emplace(); }
};

class HazardUnit : public ComponentWithIO<HazardUnitIO> {
    // 无 __io，无 create_ports()，无宏
};
```

| 维度 | 评估 |
|------|------|
| **改动范围** | ~111 个文件 + 新的 `component_with_io.h` 头文件 |
| **优点** | 消除宏、消除样板、类型安全、字段名保留 |
| **缺点** | 需同时创建 IO 结构体定义 + 修改每个组件的基类；所有用户代码需更新 |
| **可行性** | 远期理想方向，适合在新组件中逐步采用 |

### 4.5 方案对比总结

| 方案 | 改动范围 | 消除 UB | 消除宏 | 保留字段名 | 迁移风险 | 推荐阶段 |
|------|---------|---------|--------|-----------|---------|---------|
| **一：修复宏** | 2 文件 | ✅ | ❌ | ❌ | 极低 | **短期** |
| **二：直接成员** | ~111 文件 + 类型体系修改 | ✅ | ✅ | ✅ | 高（架构阻塞） | ❌ 不可行 |
| **三：Bundle + optional** | ~111 文件 + 新结构体 | ✅ | ✅ | ✅ | 高 | 远期方向 |
| **四：CRTP 基类** | ~111 文件 + 新头文件 | ✅ | ✅ | ✅ | 高 | **远期理想** |

---

## 5. Oracle 建议

### 5.1 建议结论：两阶段策略

**阶段一（短期）— 采用方案一，修复宏**：
- 改动量：1-2 文件，零用户可见变更
- 消除三个实际技术问题（UB、保留标识符、样板重复）
- 不改动任何已有 ~111 个组件文件

**阶段二（远期）— 逐步引入方案四（CRTP）**：
- 仅在新建组件或大规模重构时采用 `ComponentWithIO<IO>`
- 不做批量迁移
- `ch_device<T>::io()` 和 `ch_module<T>::io()` 通过 `auto&` 推导继续正常工作

### 5.2 关键建议点

1. **111 个文件的迁移成本不值得为纯外观改善而付出** — 宏丑陋但功能完整
2. **短期内优先消除 UB**，而非追求完美的 API 设计
3. **`create_ports()` 样板完全可以吸收进宏内部**，这是最直接的改进
4. `ch_device<T>::io()` 和 `ch_module<T>::io()` 使用 `auto&` 推导，无论哪种方案都无需修改

---

## 6. 决议（待审查）

**状态**: 🔍 **待深入审查** — 此议题推迟到后期再决策。

**决议**: 暂不做最终决定。当前分析记录完毕，待以下条件触发时重新评估：
- 下一次大规模需要修改 IO 体系时（如 T002 Verilog 命名修复）
- 或出现新的架构需求使某一方案明显更优
- 或积累的 UB 问题需要在特定编译器/工具链上解决

**临时行动项**：
- 当前不需要修改代码
- 维护现有 `__io` 宏不变
- 未来重新评估时，优先考虑 Oracle 推荐的**两阶段策略**

---

## 7. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Oracle 分析过程和待审查决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/io.h:301-339` — `ch_in`/`ch_out` 类型别名 + `__io` 宏定义 + `__in`/`__out` 宏
- `include/core/io.h:190-298` — `port<T, Dir>` 模板类定义（输入/输出特化）
- `include/core/direction.h:9-11` — `input_direction`/`output_direction`/`internal_direction` 标签
- `include/component.h:16-101` — `Component` 基类（无 `io()` 方法）
- `include/device.h:8-32` — `ch_device<T>` 封装
- `include/module.h:35-101` — `ch_module<T>` 封装
- `docs/developer_guide/tech-reports/T002-verilog-naming-fix-analysis.md` — `__io` 字段名丢失问题分析
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #7
