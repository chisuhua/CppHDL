# CppHDL Phase 架构总览

> **项目**: CppHDL - SpinalHDL 到 C++ 的系统移植  
> **创建时间**: 2026-04-12  
> **当前阶段**: Phase 1-5 (进行中) → Phase 6 (规划) → Phase 7 (远期)

---

## 📊 Phase 路线

| Phase | 主题 | 状态 | 完成度 | 核心交付 |
|-------|------|------|--------|---------|
| [Phase 1](phase-1-foundation/) | 基础移植验证 | ✅ 完成 | 100% | Counter, FIFO, UART TX, 17 个移植 |
| [Phase 2](phase-2-core-components/) | 核心组件完善 | 🟡 测试中 | ~40% | Catch2 测试 + CTest 注册（代码已存在） |
| [Phase 3](phase-3-stream-advanced/) | 高级流处理 | 🟡 部分 | ~30% | Width Adapter ✅, Stream Mux/Arbiter (待实现) |
| [Phase 4](phase-4-axi-bus/) | AXI 总线完整实现 | 🟡 简化 | ~20% | AXI4-Lite (简化), AXI4-Full (规划) |
| [Phase 5](phase-5-riscv-pipeline/) | RISC-V 5 级流水线 | 🟡 原型 | ~25% | RV32I 流水线, Branch Predictor v2 |
| [Phase 6](phase-6-advanced-features/) | 高级特性与性能优化 | 🔴 规划 | 0% | Tournament BPU, L2 Cache, 超标量 |
| [Phase 7](phase-7-plugin-architecture/) | Plugin 插件架构 | 🔴 远期 | 0% | 插件化 RISC-V 核心 |

---

## 🏗️ 模块文档规范

每个模块目录下应包含 4 个标准文档：

| 文档 | 说明 | 创建时机 |
|------|------|---------|
| `detailed-design.md` | 模块详细设计（算法、状态机、数据流） | 编码前 |
| `api-spec.md` | API 接口规范（IO 端口、接口方法） | 编码前 |
| `test-strategy.md` | 模块测试策略（测试用例、边界条件） | 编码前 |
| `database-schema.md` | 数据结构定义（如 BRAM/存储配置） | 按需 |

> ⚠️ 所有模块 test-strategy.md 必须引用项目级错误处理策略（待创建）

---

## 📁 目录结构

```
phases/
├── README.md                           ← 当前位置（总览）
├── phase-1-foundation/                 ← Phase 1: 基础移植验证
│   ├── README.md
│   ├── tasks/
│   ├── modules/counter/
│   ├── modules/uart-tx/
│   └── modules/fifo/
├── phase-2-core-components/            ← Phase 2: 核心组件
├── phase-3-stream-advanced/            ← Phase 3: 高级流处理
├── phase-4-axi-bus/                    ← Phase 4: AXI 总线
├── phase-5-riscv-pipeline/             ← Phase 5: RISC-V 流水线
├── phase-6-advanced-features/          ← Phase 6: 高级特性
└── phase-7-plugin-architecture/        ← Phase 7: Plugin 架构
```

---

## 🔗 相关文档

- [Gap Analysis](../2026-03-30-cpphdl-architecture-gap-analysis.md)
- [SpinalHDL Port Architecture](../2026-03-30-spinalhdl-port-architecture.md)
- [State Machine DSL Design](../2026-03-30-state-machine-dsl-design.md)
- [ADR 记录](../decisions/)
- [Phase 6 详细架构](phase-6-advanced-features/README.md)

---

**维护**: AI Agent  
**创建日期**: 2026-04-12  
**下次审查**: 2026-05-12
