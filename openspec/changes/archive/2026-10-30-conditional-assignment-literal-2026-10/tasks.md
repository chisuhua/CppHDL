# Tasks: conditional-assignment-literal-2026-10

## 1. 立项

- [x] 1.1 实测复现（探针：照抄头注释示例 ⇒ 编译失败）
- [x] 1.2 change 目录 + `proposal.md` + `design.md` + `tasks.md`
- [x] 1.3 capability delta：`chlib-conditional-semantics`（**MODIFIED ×1**）

## 2. 实现

- [x] 2.1 `assign()` 的 else 分支经 `to_operand` 归一化（**1 行**）
- [x] 2.2 头注释的示例保留**裸 literal** + 加注（⇒ 文档级回归点）

## 3. 验证

- [x] 3.1 编译期 fixture：`pos_literal.cpp`（4 种组合）MUST 编译通过
- [x] 3.2 对照 fixture：`neg_control.cpp` MUST 编译失败且指向本文件
- [x] 3.3 **fixture 存在性**的 fail-closed 检查（⚠️ 实测踩中的护栏盲区）
- [x] 3.4 源码级检查：**两条**路径都归一化（防止「只修一半」）
- [x] 3.5 运行期：裸 literal 的条件赋值语义正确（真/假两路）
- [x] 3.6 运行期：块外的裸 literal 直写语义正确

## 4. 变异

- [x] 4.1 **M1** 撤掉 `to_operand` 归一化 ⇒ 护栏 R1 + R3 转红
      （⚠️ 预期结果就是**编译失败** ⇒ 「先确认编译成功」这条纪律
      **不适用**于编译期判据 ⇒ 如实登记）
- [x] 4.2 **M2** 吞掉块外直写 ⇒ 运行期用例转红（**编译成功** ⇒ 合法）
- [x] 4.3 **M3** 删除 fixture ⇒ R0 转红（**fail-closed** 生效）

## 5. 回归与计数

- [x] 5.1 `ctest -LE perf` 零回归
- [x] 5.2 全部护栏（含 `--self-test`）exit 0
- [x] 5.3 **计数锚点不变**

## 6. 归档

- [x] 6.1 归档（**禁** `--skip-specs`）
- [x] 6.2 CI 挂载（主路径 + `--self-test`）
