# CppHDL 项目概览 (PROJECT OVERVIEW)

**最后更新**: 2026-04-08  
**项目定位**: SpinalHDL 的 C++ 实现  
**当前阶段**: Phase 3 (AXI4 外设 + RISC-V)  

---

## 1. 项目目标

将 **SpinalHDL** 的硬件描述概念和模式移植到 C++，提供:
- ✅ 类型安全的硬件描述
- ✅ 编译时错误检测
- ✅ 可生成的 Verilog 代码
- ✅ 内置仿真器

**核心差异**:

| 特性 | SpinalHDL | CppHDL |
|------|-----------|--------|
| 语言 | Scala + DSL | C++ 模板元编程 |
| 类型系统 | 运行时检查 | 编译时检查 |
| 仿真 | Java/Scala | C++ |
| 依赖 | JVM | 无 |

---

## 2. 技术栈

### 2.1 核心依赖

```cmake
- CMake >= 3.16
- C++20 (GCC 12+)
- Catch2 v3.7.0 (测试框架)
```

### 2.2 架构层次

```
┌─────────────────────────────────────────┐
│ 应用层： AXI4 外设，RISC-V               │
├─────────────────────────────────────────┤
│ 库层： Chlib (Component 库)              │
├─────────────────────────────────────────┤
│ 组件层： Component, Module               │
├─────────────────────────────────────────┤
│ 核心层： Bundle, Node, Context, Operator │
├─────────────────────────────────────────┤
│ 基础层： AST, LNode, Codegen             │
└─────────────────────────────────────────┘
```

---

## 3. 关键设计决策

### 3.1 线程局部存储 (thread_local)

**问题**: 如何在多线程环境中隔离硬件上下文？

**解决方案**: 使用 `thread_local` 存储当前上下文指针。

```cpp
// include/core/context.h
extern thread_local context *ctx_curr_;

// 使用
{
    ch::core::ctx_swap swap(ctx);  // RAII 交换上下文
    // 在此作用域内，所有硬件创建都在 ctx 中
}
```

**参考**: `include/core/context.h`, ADR-001

### 3.2 硬件连接语义

**问题**: 如何表示硬件连接而非软件赋值？

**解决方案**: 使用 `<<=` 操作符，创建 assign 操作节点。

```cpp
// 错误：软件赋值
ch_uint<8> a = b;  // x

// 正确：硬件连接
a <<= b;           // ✓
```

### 3.3 Bundle 元编程

**问题**: 如何支持字段反射和自动连接？

**解决方案**: 使用 C++20 模板元编程 + `CH_BUNDLE_FIELDS_T` 宏。

```cpp
struct MyBundle : public bundle_base<MyBundle> {
    ch_uint<8> data;
    ch_bool valid;
    
    CH_BUNDLE_FIELDS_T(data, valid)  // 生成字段元数据
    
    void as_master_direction() {
        this->make_output(data, valid);
    }
};
```

---

## 4. 已完成功能 (Phase 1-3)

### Phase 1: 核心基础设施 ✅

- ✅ `ch_bool`, `ch_uint`, `ch_sint` 类型定义
- ✅ Bundle 元编程支持
- ✅ Component/Module 基类
- ✅ 编译时 Verilog 生成
- ✅ 基本仿真器

### Phase 2: Chlib 组件库 ✅

- ✅ FIFO 组件
- ✅ Shift Register
- ✅ Counter
- ✅ Stream Operators
- ✅ State Machine DSL

### Phase 3: AXI4 + RISC-V 🟡

- ✅ AXI4-Lite 外设 (SPI, PWM, I2C, GPIO)
- ✅ AXI4 DMA Controller
- 🟡 RISC-V RV32I 核心 (Pipeline 阶段)
- 🟡 分支预测单元

### Phase 4: 性能优化 🔴

- 🔴 并行仿真
- 🔴 增量编译
- 🔴 内存优化

**图例**: ✅ 完成 | 🟡 进行中 | 🔴 待开始

---

## 5. 代码库统计

| 统计项 | 数量 |
|--------|------|
| 头文件 | ~150 |
| 源文件 | ~50 |
| 测试文件 | ~60 |
| 文档文件 | ~40 |
| 代码行数 (C++) | ~50K |
| 测试断言数 | ~5K |

---

## 6. 开发工作流

### 6.1 本地开发

```bash
# 1. 克隆项目
git clone <repo>
cd CppHDL

# 2. 构建项目
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# 3. 运行测试
ctest --output-on-failure

# 4. 运行示例
./samples/fifo_example
```

### 6.2 Git 工作流

```bash
# 1. 创建功能分支
git checkout -b feature/axi-dma

# 2. 开发并提交
git add . && git commit -m "feat: add AXI DMA controller"

# 3. 推送到远程
git push origin feature/axi-dma

# 4. 创建 Pull Request
```

---

## 7. 质量指标

### 7.1 测试覆盖率

| 模块 | 覆盖率 | 测试文件 |
|------|--------|----------|
| Core | 85% | test_basic*, test_operator* |
| Bundle | 90% | test_bundle* |
| Chlib | 80% | test_fifo*, test_stream* |
| AXI | 70% | test_axi_* |
| RISC-V | 60% | test_rv32i* |

### 7.2 编译速度

| 目标 | 首次编译 | 增量编译 |
|------|----------|----------|
| cpphdl | ~30s | ~5s |
| 测试 | ~120s | ~20s |

### 7.3 代码质量

| 工具 | 规则 | 结果 |
|------|------|------|
| clang-tidy | cppcoreguidelines-* | 0 警告 |
| clang-format | LLVM style | 自动格式化 |
| Cppcheck | 内存/逻辑错误 | 0 错误 |

---

## 8. 已知问题

### 8.1 阻塞性问题 (P0)

| ID | 问题 | 状态 | 负责人 |
|----|------|------|--------|
| #001 | Bundle 连接失败 (`test_bundle_connection`) | 🔴 待修复 | - |
| #002 | RV32I Pipeline 测试失败 | 🔴 待修复 | - |

### 8.2 技术债务

| ID | 问题 | 优先级 | 备注 |
|----|------|--------|------|
| TD-001 | `operator=` 和 `operator<<=` 设计冲突 | 高 | 见 bundle-connection-issue.md |
| TD-002 | LSP 诊断警告 (bundle 模板) | 中 | - |
| TD-003 | 测试覆盖率不足 | 低 | 需补充边界测试 |

---

## 9. 路线图

### 2026 Q2

| 里程碑 | 目标 | 预计完成 |
|--------|------|----------|
| M5 | 修复 Bundle 连接问题 | 2026-04-15 |
| M6 | 完成 RV32I 核心 | 2026-05-01 |
| M7 | Phase 3 结项 | 2026-06-01 |

### 2026 Q3

| 里程碑 | 目标 | 预计完成 |
|--------|------|----------|
| M8 | Phase 4 启动 (性能优化) | 2026-07-01 |
| M9 | 并行仿真实现 | 2026-08-01 |
| M10 | v1.0 发布 | 2026-09-01 |

---

## 10. 团队与角色

| 角色 | 职责 | 人员 |
|------|------|------|
| 架构师 | 技术决策、架构设计 | |
| 核心开发 | 核心功能实现 | |
| QA | 测试、质量保证 | |
| 技术作者 | 文档编写与维护 | |
| AI Agent | 自动化开发辅助 | Sisyphus/Oracle/Artistry |

---

## 11. 相关资源

- [README.md](README.md) - 项目简介
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - 快速参考
- [Compare.md](Compare.md) - 与 SpinalHDL 详细对比
- [docs/](docs/) - 完整文档目录

---

**维护**: 项目核心团队  
**更新频率**: 每月或里程碑完成后  
**最后审阅**: ________________
