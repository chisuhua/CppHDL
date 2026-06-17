# OpenSpec Main Specs 建立指南

> 适用于 `openspec/specs/` 目录的建立与维护。  
> **建立日期**：2026-06-17  
> **首次内容来源**：`archive/2026-06-17-fix-include-aggregation/spec.md`

## Purpose

`openspec/specs/` 是项目**当前生效规范**的 source of truth。OpenSpec 1.4.0 CLI 强制要求 capability 文件夹结构。

## 关键约束（OpenSpec 1.4.0 强制）

| 约束 | 正确格式 | 错误示例 |
|------|---------|---------|
| **目录结构** | `openspec/specs/<capability>/spec.md` | ❌ `openspec/specs/<capability>.md`（扁平） |
| **Purpose 段** | `## Purpose`（heading） | ❌ `> **Purpose**`（引用块） |
| **Requirements 段** | `## Requirements`（heading） | — |
| **每条 requirement** | 描述必须含 `SHALL` 或 `MUST` | ❌ 缺关键字会被 validate 拒绝 |
| **Scenario 段** | `#### Scenario: <name>`（4 级 heading） | — |
| **change 端结构** | `openspec/changes/<name>/specs/<capability>/spec.md`（含 `## ADDED Requirements` delta 头） | ❌ `<name>/spec.md`（旧格式） |

## 目录结构示例（当前状态）

```
openspec/
├── specs/                              # 主规范（source of truth）
│   └── chlib-aggregator/               # 能力文件夹
│       └── spec.md                     # 该能力的完整规范
└── changes/
    ├── <active-change>/
    │   ├── specs/                      # 增量规范（OpenSpec 1.4 期望）
    │   │   └── <capability>/
    │   │       └── spec.md             # 包含 ## ADDED Requirements
    │   └── ...                         # 其他制品
    └── archive/
        └── <date>-<name>/
            └── spec.md                 # 顶层（项目历史遗留格式）
```

## 历史背景

### 2026-06-17 首次建立

- **触发原因**：归档 `2026-06-17-fix-include-aggregation` 时使用了 `--skip-specs` 标志，导致 2 个 ADDED Requirements 未合并到主规范。
- **首次内容**：迁移自 `archive/2026-06-17-fix-include-aggregation/spec.md`。
- **格式修正**：
  - 删除 `## ADDED Requirements` heading（主规范无 delta 概念）
  - 转换 `> **Purpose**` 引用块 → `## Purpose` heading
  - 添加 `MUST` 关键字到 requirement 描述（RFC 2119 规范）
  - 调整为 `openspec/specs/chlib-aggregator/spec.md`（capability 文件夹结构）

### 2026-05-18 缺失

`fix-ns-pollution` change 显式记录"无 spec 级别行为变更"（占位 README），但**未触发本目录建立**——因为无实质性内容可迁。

## 当前文件

| Capability | 来源 | Requirements 数 |
|------------|------|----------------|
| `chlib-aggregator/` | fix-include-aggregation | 2 |

## 验证命令

```bash
# 列出所有 spec
openspec list --specs

# 验证单个 spec
openspec validate <capability-name>
openspec validate <capability-name> --strict

# 全局验证
openspec validate --specs
openspec validate --specs --strict

# 显示具体 spec 内容
openspec show <capability-name>
```

## 未来规则

参见 `AGENTS.md` → "OPENSPEC WORKFLOW" 节。简要：

1. **新建 spec**：`openspec archive` **必须**不跳过 spec（不要加 `--skip-specs`）
2. **修改 spec**：现有 `openspec/specs/<capability>/spec.md` 的修改必须通过新的 change 流程
3. **格式合规**：每条 requirement 描述必须含 `SHALL`/`MUST`；Purpose/Requirements 必须是 heading 不是引用块

## 已知遗留问题

历史 `openspec/changes/` 下的 change 目录使用**顶层 `spec.md` 格式**（与 OpenSpec 1.4 期望的 `specs/<capability>/spec.md` 不一致），但 archive 流程仍可工作。**未来新 change 应使用 capability 文件夹结构**。