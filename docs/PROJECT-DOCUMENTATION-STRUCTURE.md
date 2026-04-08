# CppHDL 项目文档结构整理报告

**整理日期**: 2026-04-08  
**整理范围**: 根目录、docs、tests、samples、examples、skills  
**归档目录**: `docs/archive/`  

---

## 1. 项目概述

CppHDL = C++ based Hardware Description Language
将 SpinalHDL 的概念和硬件描述模式移植到 C++。

### 1.1 核心组件

```
CppHDL
├── include/          # 核心头文件
│   ├── core/         # 核心逻辑层 (节点、操作符、上下文)
│   ├── bundle/       # Bundle 类型定义与元编程
│   ├── chlib/        # Component 库
│   ├── ast/          # AST 节点定义
│   └── riscv/        # RISC-V 组件
├── src/              # 实现文件
├── tests/            # 单元测试
├── samples/          # 示例代码
├── examples/         # 完整示例项目
└── docs/             # 文档
```

---

## 2. 文档分类体系

### 2.1 文档层级

| 层级 | 目录 | 用途 | 维护人 |
|------|------|------|--------|
| **核心文档** | 根目录/*.md | 项目指南、开发规范 | 核心开发 |
| **使用指南** | docs/*UsageGuide.md | 面向用户，指导如何使用 | 核心开发 |
| **开发者指南** | docs/*DeveloperGuide.md | 面向框架维护者 | 框架维护者 |
| **测试指南** | docs/*Testing_Guide.md | 测试规范与最佳实践 | QA |
| **架构文档** | docs/architecture/ | 架构决策与设计方案 | 架构师 |
| **归档文档** | docs/archive/ | 历史报告、过时文档 | 存档 |

### 2.2 文档状态标记

| 标记 | 含义 | 更新频率 |
|------|------|----------|
| ✅ | 最新/已验证 | 每次发布 |
| 🟡 | 部分过时 | 定期审查 |
| 🔴 | 过时/待删除 | 归档或删除 |

---

## 3. 现有文档清单

### 3.1 根目录文档 (根级)

| 文件名 | 说明 | 状态 | 建议 |
|--------|------|------|------|
| `README.md` | 项目简介 | ✅ | 保留 |
| `AGENTS.md` | 开发代理指南 | ✅ | 保留 |
| `Compare.md` | CppHDL vs SpinalHDL | ✅ | 保留 |
| `PLAN.md` | 阶段 1 计划 | 🟡 | 归档 |
| `PLAN2.md` | 阶段 2 计划 | 🟡 | 归档 |
| `PHASE1-REVISED-PLAN.md` | 阶段 1 修订计划 | 🟡 | 归档 |
| `AI_CODING_GUIDELINES.md` | AI 编码规范 | ✅ | 保留 |

**执行动作**:
- 将 Plan 系列文档移至 `docs/archive/phase-reports/`

---

### 3.2 docs/ 目录文档

#### 核心指南 (保留)

| 文件名 | 说明 | 目标读者 |
|--------|------|----------|
| `Bundle_UsageGuide.md` | Bundle 使用指南 | 用户 |
| `Bundle_DeveloperGuide.md` | Bundle 开发者指南 | 框架维护者 |
| `CHLib_UsageGuide.md` | Chlib 库使用指南 | 用户 |
| `CppHDL_UsageGuide.md` | CppHDL 使用指南 | 用户 |
| `CppHDL_Testing_Guide.md` | 测试指南 | 测试人员 |
| `CppHDL_vs_SpinalHDL_Stream_Flow_Usage.md` | 流操作对比 | 用户 |

#### 架构文档 (保留)

```
docs/architecture/
├── 2026-03-30-cpphdl-architecture-gap-analysis.md
├── 2026-03-30-spinalhdl-port-architecture.md
├── 2026-03-30-state-machine-dsl-design.md
├── SYNC-REPORT.md
└── decisions/
    └── ADR-001-cpphdl-spinalhdl-port-strategy.md
```

#### 归档文档 (已移至 archive/)

| 原位置 | 新位置 | 说明 |
|--------|--------|------|
| `PHASE*.md` | `archive/phase-reports/` | 阶段性报告 |
| `PHASE3A*.md` | `archive/phase-reports/` | Phase 3A 报告 |
| `PHASE3B*.md` | `archive/phase-reports/` | Phase 3B 报告 |
| `DESIGN_*.md` | `archive/daily-reports/` | 日常设计报告 |
| `coding_*.md` | `archive/daily-reports/` | 编码日志 |
| `step1.md` | `archive/daily-reports/` | 步骤记录 |
| `SpinalHDL*.md` | `archive/spinalhdl-comparison/` | SpinalHDL 对比 |

---

### 3.3 tests/ 目录文档

| 文件名 | 说明 | 状态 |
|--------|------|------|
| `AGENTS.md` | 测试子包指南 | ✅ |
| `README.md` | 测试说明 | ✅ |

**测试分类**:

| 分类 | 文件模式 | 数量 | 示例 |
|------|----------|------|------|
| 核心测试 | `test_basic*.cpp`, `test_reg*.cpp` | 12 | `test_basic.cpp` |
| Bundle 测试 | `test_bundle*.cpp` | 6 | `test_bundle_connection.cpp` |
| 操作符测试 | `test_operator*.cpp` | 4 | `test_operator_advanced.cpp` |
| 内存测试 | `test_mem*.cpp` | 4 | `test_mem_timing.cpp` |
| Stream 测试 | `test_stream*.cpp` | 3 | `test_stream_phase1.cpp` |
| 字面量测试 | `test_literal*.cpp` | 3 | `test_literal_width.cpp` |
| IO 测试 | `test_io*.cpp`, `test_simple_io.cpp` | 2 | `test_simple_io.cpp` |
| 其他 | `test_*.cpp` | 20+ | - |

---

### 3.4 samples/ 目录

| 文件 | 说明 | 状态 |
|------|------|------|
| `AGENTS.md` | 示例指南 | ✅ |
| `fifo_example.cpp` | FIFO 示例 | ✅ |
| `shift_register.cpp` | 移位寄存器 | ✅ |
| `spinalhdl_stream_example.cpp` | Stream 示例 | ✅ |

**使用场景**: 学习 CppHDL 基础用法的快速示例

---

### 3.5 examples/ 目录

```
examples/
├── spinalhdl-ported/        # 从 SpinalHDL 移植的示例
│   ├── README.md
│   ├── counter/
│   ├── uart/
│   ├── fifo/
│   └── width_adapter/
└── axi4-lite-demo.cpp       # AXI-Lite 演示
```

**与 samples 的区别**:
- `samples/`: 简单示例，单个文件
- `examples/`: 完整项目，多文件+测试

---

### 3.6 skills/ 目录 (AI Agent 技能)

```
skills/
├── acp-rv32i-workflow/
├── cpphdl-assert-static-destruction/
├── cpphdl-chop-type-safety/
├── cpphdl-fifo-timing-fix/
├── cpphdl-mem-init-dataflow/
├── cpphdl-shift-fix/
├── cpphdl-type-system-patterns/
├── rv32i-branch-pattern/
└── rv32i-decoder-pattern/
```

**用途**: OhMyOpenCode Agent 技能库，提供特定领域的开发指导

---

## 4. 新增文档

### 4.1 问题报告

```
docs/problem-reports/
└── bundle-connection-issue.md  # Bundle 连接问题深度分析
```

### 4.2 实施计划

```
docs/plans/
└── bundle-connection-fix-plan.md  # Bundle 连接修复实施计划
```

---

## 5. 归档目录结构

```
docs/archive/
├── phase-reports/
│   ├── PHASE1-PHASE2-FINAL-REPORT.md
│   ├── PHASE1-2-TIMING-VERIFICATION-REPORT.md
│   ├── PHASE3-PLAN.md
│   ├── PHASE3A-PLAN.md
│   ├── PHASE3A-COMPILE-FIXES.md
│   ├── PHASE3A-DAY1-REPORT.md
│   ├── PHASE3A-DAY2-REPORT.md
│   ├── PHASE3A-DAY2-FINAL-REPORT.md
│   ├── PHASE3A-DAY2-SUCCESS.md
│   ├── PHASE3B-DAY1-REPORT.md
│   ├── PHASE3B-DAY2-REPORT.md
│   └── PHASE3B-FINAL-REPORT.md
│
├── daily-reports/
│   ├── DESIGN_component.md
│   ├── DESIGN_context.md
│   ├── DESIGN_io.md
│   ├── DESIGN_phase1.md
│   ├── DESIGN_phase2.md
│   ├── DESIGN_phase3.md
│   ├── coding_internal_module.md
│   ├── coding_internal_overview.md
│   ├── coding_internal_thread.md
│   ├── coding_lnode.md
│   ├── coding_logic_buffer.md
│   ├── coding_mem.md
│   └── step1.md
│
├── spinalhdl-comparison/
│   ├── SpinalHDL_Stream_Connection_Features_Comparison.md
│   └── SpinalHDL_Stream_Operators_Implementation.md
│
└── architecture/
    └── (保持不变)
```

---

## 6. 文档维护指南

### 6.1 新增文档规范

1. **位置**: 根据类型选择正确目录
   - 使用指南 → `docs/`
   - 问题报告 → `docs/problem-reports/`
   - 实施计划 → `docs/plans/`
   - 日常报告 → `docs/archive/daily-reports/`

2. **命名**: 使用 `kebab-case` 命名
   - ✅ `bundle-connection-issue.md`
   - ❌ `BundleConnectionIssue.md`

3. **格式**: 使用标准 Markdown + Mermaid 图表

### 6.2 文档审查周期

| 文档类型 | 审查频率 | 负责人 |
|----------|----------|--------|
| 核心指南 | 每季度 | 核心开发 |
| 使用指南 | 每发布版本 | 技术作者 |
| 问题报告 | 问题解决后 | 问题解决人 |
| 实施计划 | 完成后 | 项目经理 |

---

## 7. 文档对齐检查

### 7.1 与 AGENTS.md 对齐

当前核心指南与 `AGENTS.md` 保持一致：
- ✅ 使用中文注释
- ✅ 遵循编码风格规范
- ✅ 文档结构清晰

### 7.2 与代码对齐

| 文档 | 对应代码 | 状态 |
|------|----------|------|
| `Bundle_UsageGuide.md` | `include/core/bundle/` | ✅ 对齐 |
| `CHLib_UsageGuide.md` | `include/chlib/` | ✅ 对齐 |
| `CppHDL_UsageGuide.md` | `include/core/` | ✅ 对齐 |

### 7.3 待改进

- ❌ `test_bundle_connection.cpp` 与文档示例不一致
- ❌ 缺少 `operator<<=` 修复的文档更新

---

## 8. 附录：文件类型定义

### 8.1 AGENTS.md 文件

项目内各子系统的 Agent 指南，使用统一的 Schema:
- `OVERVIEW`: 子系统概述
- `STRUCTURE`: 文件结构
- `KEY FILES`: 关键文件
- `CONVENTIONS`: 编码约定
- `ANTI-PATTERNS`: 常见错误
- `RELATED`: 相关目录

### 8.2 问题报告

包含：
- 问题描述
- 根本原因分析
- 影响范围
- 修复方案对比
- 决策建议

### 8.3 实施计划

包含：
- 修复概述
- 详细步骤
- 风险管理
- 验收标准
- 时间规划

---

**整理人**: AI Agent  
**审阅人**: ________________  
**下次审查**: 2026-05-08
