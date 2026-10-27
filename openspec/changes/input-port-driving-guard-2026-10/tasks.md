# Tasks: input-port-driving-guard-2026-10

## 0. 准备
- [ ] 0.1 读 `verify_example_assertion_quality.sh` 的**失败史**（C 子集分析器 ⇒ 6 轮审稿往返）

## 1. spike（**只出文档**）
- [ ] 1.1 列出「需要解析什么」（C++ 表达式 → 端口集合）
- [ ] 1.2 候选设计 ≥ 3 个（登记表 / clang 解析 / 运行期检查）⇒ 各自成本 + 误报面 + 可测性
- [ ] 1.3 明确**推荐方案** + 理由（含「为什么不是另外两个」）
- [ ] 1.4 明确「若实施 ⇒ 前置」（ADR？框架层 API？CI 挂载点？）

## 2. 决策门
- [ ] 2.1 结论：实施 **或** 关闭（关闭 ⇒ 义务留在 `AGENTS.md §ANTI-PATTERNS`）
- [ ] 2.2 **不得**硬造一个误报率高的护栏

## 3. 归档
- [ ] 3.1 结论写入根 `AGENTS.md`（无论实施与否）
- [ ] 3.2 `openspec archive input-port-driving-guard-2026-10 --yes --skip-specs`
