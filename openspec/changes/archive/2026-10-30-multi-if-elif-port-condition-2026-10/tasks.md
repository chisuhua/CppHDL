# Tasks: multi-if-elif-port-condition-2026-10

## 1. 立项
- [x] 1.1 实测复现（`elif` 的端口条件编译不过）
- [x] 1.2 change 目录 + `proposal.md` + `design.md` + `tasks.md`
- [x] 1.3 capability delta（**MODIFIED ×1**，E3 Scenario **原地订正**）

## 2. 实现
- [x] 2.1 `multi_if` 的 ctor 条件**模板化** + `to_operand` 归一化
- [x] 2.2 `multi_if::elif` 的条件**模板化** + `to_operand` 归一化
- [x] 2.3 归一化**幂等性**的注释说明

## 3. 验证
- [x] 3.1 编译期 fixture 补 2 种组合（`elif` 端口条件 · `multi_if` 直构）
- [x] 3.2 护栏 R3 补 `elif` + ctor 的**正 + 负**检查
- [x] 3.3 self-test **原地投毒**（⚠️ 投毒副本不会被 include ⇒ 判据恒绿 ⇒ 必须原地）
- [x] 3.4 还原后 MUST 再确认 fixture 编译通过（⇒ 还原没写坏文件）
- [x] 3.5 修两处**未转义反引号**（shell 当命令替换 ⇒ 消息被吃掉）

## 4. 回归与计数
- [x] 4.1 `ctest -LE perf` 零回归
- [x] 4.2 全部护栏（含 `--self-test`）exit 0
- [x] 4.3 **计数锚点不变**

## 5. 归档
- [x] 5.1 归档（**禁** `--skip-specs`）
