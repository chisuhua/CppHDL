# 变更规格：roadmap 簿记一致性

## ADDED Requirements

### Requirement: 根 AGENTS.md 的 AUTO 块 MUST 与 feature front-matter 守恒

- **WHEN** 检视根 `AGENTS.md` 里两个 AUTO 哨兵之间的 feature 块
      （哨兵 = `AUTO: feature fragments start` 与 `AUTO: feature fragments end` 两行注释）
- **THEN** 该块 MUST 与 `.rddf/roadmap/**` 下 `kind: feature` 的 fragment
      front-matter **逐字一致**
- **AND** 渲染规则 MUST 与工具链生成器的规则**同源**
      （`id` 排序 / `phase_refs` 以 `, ` 连接 / 空则 `(none)` / theme 的 `|` 转义 /
      `主题` 键才是 theme / `kind` 缺省为 `phase`）
- **AND** 渲染规则一旦变更，MUST 重跑与工具链的**等价性验证**
- **NOTE**: 判据 MUST 用「**渲染期望块 + 逐字比对**」，
      **不**用「双向解析 + 字段比对」—— 后者要额外解析已提交块的表格 ⇒ 多一类误报。

#### Scenario: AUTO 块与 front-matter 不一致

- **WHEN** 某个 feature 的 `status` 在 front-matter 里是 `active`，
      而已提交的 AUTO 块里写 `completed`
- **THEN** 护栏 MUST 报违规并打印两侧的差异
- **AND** MUST NOT 仅给一句「不一致」而无差异内容

#### Scenario: AUTO 块哨兵缺失

- **WHEN** 根 `AGENTS.md` 不含两个 AUTO 哨兵
- **THEN** 护栏 MUST **fail-closed**（报违规）
- **AND** MUST NOT 静默跳过（跳过 = 闸门形同虚设）

### Requirement: feature 文件内部的完成度 MUST 一致

- **WHEN** 同一个 feature 文件里，**三个来源**提到**同一个键**
      （`### phase-*` 段的 bullet / `## 验收标准` 的 checkbox / 正文表格行）
- **THEN** 三者给出的完成度 MUST 一致
- **AND** 键 MUST 是**行首**的 `Phase N` 标签，或**行首**的**已知 change 名**
      （已知 = `openspec/changes/**` 的目录名，含去日期前缀 / 去日期后缀的形态）
- **AND** 完成度符号 MUST 只认 `✅` / `☑` / `[x]` 与 `⏳` / `🟡` / `[ ]`
      ⇒ 其余符号 MUST 跳过
- **AND** 同一面出现多个完成度符号 ⇒ MUST **跳过**（无法判定 ≠ 违规）
- **NOTE**: ⚠️ 只在**行首**比对 —— 「行中提及」（如「Phase 3 续 + Phase 5」）
      MUST NOT 参与比对，否则会产生**误报**。

#### Scenario: 同一 phase 的符号冲突

- **WHEN** phase bullet 说 `Phase 4` 未完成，而验收标准 checkbox 说 `Phase 4` 已完成
- **THEN** 护栏 MUST 报违规

#### Scenario: 叙事性陈旧判不了

- **WHEN** 同一 change 的两侧符号**一致**、只是**内容**过时
- **THEN** 护栏 MUST NOT 报违规
- **AND** 该类漂移 MUST 被登记为**人工 + 代码评审**兜住

### Requirement: 护栏 MUST 有可执行的 fixture 规格且 MUST 可移植

- **WHEN** 护栏本体变更
- **THEN** 每条判据 MUST 有 fixture 钉住它**有判别力**（变异 ⇒ 转红）
- **AND** fixture MUST 覆盖**误报陷阱**（行中提及 / 歧义符号 / 判不了的形态）
- **AND** 护栏 MUST 在 CI 的**全部**平台矩阵上可用
      ⇒ fixture 的变异 MUST NOT 依赖 GNU-only 形态（如 `sed -i` 不带后缀）

#### Scenario: 变异使判据失效

- **WHEN** 某条判据被改成恒不触发
- **THEN** 对应 fixture MUST 转红（self-test exit 非 0）

#### Scenario: 不可移植的 fixture 变异

- **WHEN** fixture 用 `sed -i`（无后缀）改写文件
- **THEN** 在 BSD sed 平台上该变异**静默不发生** ⇒ fixture 的期望不会成立
- **AND** MUST 用可移植写法（如 `python3` heredoc）替代

### Requirement: fail-closed 与软着陆的边界 MUST 写明

- **WHEN** 护栏无法完成检查
- **THEN** 「内容不可解析」类 MUST **fail-closed**（报违规）
- **AND** 「环境性缺失」类（fragments 目录不存在）MUST **软着陆**
      （`::warning` + exit 0），且 MUST 打印可操作说明
- **AND** 判不了的形态 MUST 在能力规格里**如实登记**，MUST NOT 静默放过

#### Scenario: 环境性缺失

- **WHEN** `.rddf/roadmap/` 目录**不存在**（稀疏检出 / 下游 vendor / fork）
- **THEN** 护栏 MUST 打 `::warning` 并以 exit 0 结束
- **AND** MUST 打印可操作的说明（不是一句无信息的跳过）

#### Scenario: 内容不可解析

- **WHEN** fragments 目录**存在**，但某个 feature 的 front-matter 损坏
      （重复键 / 未闭合 / 缺 `---`）
- **THEN** 护栏 MUST **fail-closed**（报违规）
