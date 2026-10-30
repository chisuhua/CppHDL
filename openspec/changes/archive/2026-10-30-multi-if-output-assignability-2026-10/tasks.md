# Tasks: multi-if-output-assignability-2026-10

## 1. 立项

- [x] 1.1 实测复现（E1 / E3 两个编译错误）
- [x] 1.2 change 目录 + `proposal.md` + `design.md` + `tasks.md`
- [x] 1.3 capability delta（**MODIFIED ×1**，E1 / E3 两个 Scenario **原地订正**）

## 2. 实现

- [x] 2.1 **E1**：`chlib::get_lnode(const multi_if<T> &)`（**类型专用**入口）
- [x] 2.2 **E1**：`ch::core::ch_width_impl<::chlib::multi_if<T>>` 特化
      （⚠️ MUST 在 `ch::core` ⇒ ⇒ 写在 `if.h` **末尾**）
- [x] 2.3 **E3**：`if_then` 的条件形参**模板化** + 经 `to_operand` 归一化
- [x] 2.4 **零** `core/` 改动（靠 **ADL**）

## 3. 验证

- [x] 3.1 编译期 fixture（4 种组合：显式/推导 `T` × 端口/硬件条件）MUST 编译通过
- [x] 3.2 对照 fixture MUST 编译失败且指向本文件
- [x] 3.3 **fixture 存在性**的 fail-closed 检查
- [x] 3.4 源码级检查（两条修复 MUST 都在 + 条件 MUST NOT 退回 `const ch_bool &`）
- [x] 3.5 运行期：直接赋 `ch_out` 与「先转 `ch_uint`」**逐位等价**
- [x] 3.6 运行期：零扩展金标（`0x0A5` / `0x00F`）

## 4. 变异

- [x] 4.1 **M1** 删掉 `get_lnode(multi_if)` ⇒ 护栏 R1 + R3 转红
- [x] 4.2 **M2** 条件改回 `const ch_bool &` ⇒ 护栏 R1 + R3 转红

## 5. 回归与计数

- [x] 5.1 `ctest -LE perf` 零回归
- [x] 5.2 全部护栏（含 `--self-test`）exit 0
- [x] 5.3 **计数锚点不变**

## 6. 归档

- [x] 6.1 归档（**禁** `--skip-specs`）
- [x] 6.2 CI 挂载（主路径 + `--self-test`）
