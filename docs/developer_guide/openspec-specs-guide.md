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

## 提案阶段的 delta 自检 4 条（2026-10-22 补）

> 这 4 条来自 2026-10-22 两次 change（`samples-regression-guard-2026-10` 与
> `bundle-ecosystem-hardening-2026-10`）的**四轮对抗式审稿** —— 那四轮审稿
> **全部**在补同一类错误 ⇒ 值得固化成规则。
> ⚠️ 配套的自动化护栏：`scripts/verify_openspec_delta.sh`（CI 2 个 job 都跑）。

| # | 规则 | 为什么 | 怎么查 |
|---|------|--------|--------|
| **1** | delta 内 **MUST NOT** 写含 **change 名**的具体文件名 | archive 会把 delta **逐字**并入主 spec ⇒ 主 spec 会残留 change 名（仓库既有约定：主 spec 内的 change 名引用必须全部改为指向 capability） | `scripts/verify_openspec_delta.sh`（规则 1） |
| **2** | delta 内 **MUST NOT** 写**占位符路径**（`<change-name>` / `<file>` 之类） | 占位符在归档后仍是占位符 ⇒ 主 spec 里的路径**不可寻址** ⇒ verifier 无法判 done。若确实需要「一个可寻址的路径」，把它**写进 change 的 tasks**（用 `test -f` 断言），**不要**写进 spec | 同上（规则 2） |
| **3** | `## MODIFIED Requirements` **MUST 完整重述**该 Requirement（含**全部** Scenario），且**标题与主 spec 逐字一致** | 只写「改动点」⇒ archive 后主 spec 里该条 Requirement 的**其余部分丢失**；标题不一致 ⇒ CLI 匹配不到 ⇒ **静默不合并**（最坏的失败模式：看起来归档成功了，主 spec 却没变） | 同上（规则 3） |
| **4** | 每个 verify **MUST「按字面执行可达」** | 「按字面执行必红」的 guard 是最常见的自欺 —— 它看起来在守护，实际上永远红（或永远绿）⇒ 护栏形同虚设。**判据**：把 verify 逐字复制到 shell 里跑，期望它**绿**；再把被守护的代码改坏，期望它**红** | 同上（规则 4：`bash -n` + 人工核对可达性） |

### 补充：`## Purpose` 段 **MUST 在 archive 之后手工写入**

⚠️ **实测**（CLI 1.4.1）：archive 对**新建** capability **一律生成**骨架

```
## Purpose
TBD - created by archiving change <change-name>. Update Purpose after archive.
```

⇒ delta 里的 `## Purpose` **不会**被合并 ⇒ MUST 在 archive **之后**用 delta 的
Purpose **替换**该骨架行（仓库先例：`axi4-full-slave-burst` / `sample-regression-guard`）。
⚠️ 骨架行里带着 **change 名** ⇒ 不替换就违反规则 1。

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

# ⚠️ 提案阶段的 delta 自检（4 条）—— MUST 在提 PR 前跑
bash scripts/verify_openspec_delta.sh            # 检查全部 active change
bash scripts/verify_openspec_delta.sh <change>   # 只检查一个
```

### ⚠️ 口径分层（★ 2026-11-10 · 与根 `AGENTS.md` 同步）

三者**不是同一件事**，不要互相替代：

| 命令 | 作用面 | 当前状态（实测 2026-11-10） | 地位 |
|---|---|---|---|
| `openspec validate --specs`（**非 strict**） | `openspec/specs/` 主 spec 的**结构 / 关键字**（`SHALL`/`MUST` · `## Purpose`） | **exit 0** · `Totals: 44 passed, 0 failed` | **ADR 判据面的人工 MUST**（`ADR-052 V8` / `ADR-063 §5.3` / `ADR-066 V12` / `ADR-067` 验收清单）· ⚠️ **CI 不跑** |
| `openspec validate --specs --strict` | 上面 + 1.14.x 新增的两条 strict 规则（Requirement 正文 > 500 字符 · Purpose 占位符） | **exit 1** · `21 passed / 23 failed (44 items)` ⇒ **红项与本仓改动无关**（纯净 HEAD 同样红） | **只登记、不消除**（根 `AGENTS.md` 的「openspec CLI 版本漂移」条 · phase-6 **P10**）⇒ 「钉回 CLI 1.4.1」= 环境级、不可复现、不落仓 ⇒ **不实施** |
| `bash scripts/verify_openspec_delta.sh` | **active change** 的 delta（R1–R4） | 零 active change 时 **零信号**（只打 `::warning::`，**不是** pass） | **CI / archive 前的实际门** |

⇒ ⇒ 实践口径：**提 PR / 归档前跑 delta 门**（有 active change 时它才有判别力）；
**改完主 spec 后跑一次非 strict 的 `validate --specs`**（它只抓「本仓把 spec 写坏」，
不抓 CLI 规则差异）；**strict 的红当作背景噪声**，先确认 CLI 版本再判断。

## 未来规则

参见 `AGENTS.md` → "OPENSPEC WORKFLOW" 节。简要：

1. **新建 spec**：`openspec archive` **必须**不跳过 spec（不要加 `--skip-specs`）
2. **修改 spec**：现有 `openspec/specs/<capability>/spec.md` 的修改必须通过新的 change 流程
3. **格式合规**：每条 requirement 描述必须含 `SHALL`/`MUST`；Purpose/Requirements 必须是 heading 不是引用块

## 已知遗留问题

历史 `openspec/changes/` 下的 change 目录使用**顶层 `spec.md` 格式**（与 OpenSpec 1.4 期望的 `specs/<capability>/spec.md` 不一致），但 archive 流程仍可工作。**未来新 change 应使用 capability 文件夹结构**。