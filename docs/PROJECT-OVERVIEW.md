# CppHDL 项目概览 (PROJECT OVERVIEW)

**最后更新**: 2026-11-19
**项目定位**: SpinalHDL 的 C++ 实现（header-only HDL 库 + 解释器 / JIT / Verilator 后端 + 层次化 RTL 输出）
**当前阶段**: **Phase 6**（Phase 1–5 已 done；Phase 6 = 高级特性与 RTL 产品化，进行中）

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

## 4. 功能现状（**按 roadmap 的 phase 状态**）

> 状态源（唯一真相）：`.rddf/roadmap/phases/phase-*.md` 的 `status` 与 `.rddf/roadmap/features/`。

| Phase | 状态 | 主题 |
|---|---|---|
| **Phase 1** | ✅ done | 核心抽象与算子基础设施 |
| **Phase 2** | ✅ done | 高级特性与 JIT 编译器 · C-class 结构重构 · 组件库与 Bundle 系统 |
| **Phase 3** | ✅ done | Verilator 后端集成 · Verilog codegen 完整化 · 性能对比框架 |
| **Phase 4** | ✅ done | 双后端测试覆盖 · cache 子系统重建（`ch_mem` · Sync/Async 双变体）· 逻辑算子 deprecation |
| **Phase 5** | ✅ done | RV32I 五级流水线 + riscv-mini SoC 集成 · ISA 语义边界收口 · 层次化 RTL 输出 |
| **Phase 6** | 🔄 active | 高级特性与 RTL 产品化（层次化产物 / SoC 级 RTL+TB / 验证单一真相源） |

**能力快照**（`.rddf/roadmap/features/` 的 9 个 feature）：

| feature | 状态 | 内容 |
|---|---|---|
| `feat-cache-rewrite` | ✅ | cache 子系统从 C-style 数组重写为 `ch_mem` + Sync/Async 双变体 |
| `feat-dual-backend-coverage` | ✅ | 单元测试统一跑 interpreter + verilator 双后端 |
| `feat-riscv-mini-convergence` | ✅ | `include/cpu` 与 `examples/riscv-mini` 收敛成单一 RV32I 实现 |
| `feat-riscv-mini-rtl-debt` | ✅ | riscv-mini 实测挖出的两个 RTL 缺陷收口 |
| `feat-rv32i-integration` | ✅ | RV32I 5-stage 流水线 + riscv-mini SoC 端到端（双路 parity） |
| `feat-rv32i-isa-completeness` | ✅ | SB/SH/LB/LH/JAL/JALR/SRA 等语义边界 + 流水线健壮性 |
| `feat-platform-matrix` | ✅ | CI 平台矩阵 = 仅 Ubuntu（macOS/Windows 明确不做） |
| `feat-hdl-logical-deprecation` | 🔄 | ADR-036 逻辑算子 deprecation → 迁移 → 移除（移除受日历锁 2027-04-17） |
| `feat-hierarchical-rtl-output` | 🔄 | 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline） |

**已知边界**（详见根 [`AGENTS.md`](../AGENTS.md) §KNOWN LIMITATIONS）：

- Verilator 后端 **仅 Linux**（`BUILD_VERILATOR=OFF` 时其余后端不受影响）；端口位宽上限 64 bit
- RV32I **未实现** CSR（`csrrw/csrrs/csrrc`）与 `ECALL/EBREAK` ⇒ 走「非法指令可观测」路径（`dbg_illegal_reason` 4 态）
- FreeRTOS 支持**仅有规划文档**（[`archive/freertos-planning/`](archive/freertos-planning/)），仓内无实现

---

## 5. 代码库统计（2026-11-21 实测）

| 统计项 | 数量 |
|--------|-----:|
| 头文件（`include/**/*.h`） | 172 |
| 源文件（`src/**/*.cpp`） | 45 |
| C++ 代码行（include + src） | 54,517 |
| 测试源文件（`tests/**/test_*.cpp`） | 224 |
| CTest 条目 | 273 |
| samples | 40 |
| examples 的 main() 程序 | 34 |
| 文档（`docs/**/*.md`） | 266（其中归档 144） |

> ⚠️ **本表是**快照**，不是锚点** ⇒ ⇒ **无机械判据**（CI 只校验根 `AGENTS.md` 的
> `CTEST_TOTAL` 等 5 个锚点 ⇒ ⇒ 本表**不参与**任何护栏）⇒ ⇒ 漂移靠**人工**发现。
> - **2026-11-21 回填**（5 项陈旧值的根因；**口径未变** —— 在 2026-11-19 的 commit
>   `4985baa` 上按同一命令复算，逐项命中当时的表值 ⇒ ⇒ 确认是**纯数值漂移**）：
>   · C++ 代码行 **53,518 → 54,517**（口径 = `find include src -name '*.h' -o -name '*.cpp'`
>     的行数总和）；· 测试源文件 **220 → 224**（口径 = `find tests -name 'test_*.cpp'`）；
>   · CTest 条目 **269 → 273**（口径 = 根 `AGENTS.md` 的 `CTEST_TOTAL` 锚点）；
>   · examples 的 main() 程序 **42 → 34**（口径 = `grep -rl '^int main' examples --include='*.cpp'`；
>     根因 = **发射入口退役 + 改名** ⇒ 删 20 个含 main 的文件、新增 12 个 ⇒ 净 −8）；
>   · 文档 **265 → 266**（口径 = `find docs -name '*.md'`）。
> - ⚠️ **头文件 / 源文件 / samples** 三项**当期即正确**（172 / 45 / 40）⇒ 未变。

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
- [QUICK_REFERENCE.md](../QUICK_REFERENCE.md) - 快速参考
- [Compare.md](archive/root-scratch/Compare.md) - 与 SpinalHDL 详细对比
- [docs/](README.md) - 完整文档目录

---

**维护**: 项目核心团队  
**更新频率**: 每月或里程碑完成后  
**最后审阅**: ________________
