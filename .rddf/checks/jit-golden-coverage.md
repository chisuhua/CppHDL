---
evidence: probe:scripts/verify_jit_golden_coverage.sh
---

# JIT golden 覆盖基线（phase-6 **P8** / 路线 **R4.3** · ADR-035 v2.2）

> **判据性质 = 集合，不是计数。**
> 计数判据**恒绿**：把已覆盖的 `SELECT` 换成 `MUL` ⇒ 计数仍是 `5 / 39` ⇒ 覆盖面已经漂移，
> 判据却看不见。⇒ ⇒ 基线 MUST 记录**集合**，比对 MUST 是**逐元素相等**。
>
> **数据行由 `scripts/verify_jit_golden_coverage.sh` 解析**（机器读），散文不参与判定。
> 派生执行体 = `scripts/lib/jit_golden_coverage.py`（**先剥 C++ 注释**再派生 ⇒ ⇒
> 禁裸 `grep -c`：注释里出现的 `JitOp::` 不算覆盖证据）。

## 分母

`include/jit/jit_ir.h` 的 `enum class JitOp` 块内**名字行**（`^  [A-Z_]+,?$`；
头行 `enum class JitOp : uint8_t {` 与 `};` 不计）。

denominator: 39

## 分子（已覆盖集）

`tests/jit/test_jit_golden.cpp` 中 **`TEST_CASE(` 声明头**（名字串 + tag 列表）上出现的
`JitOp::<NAME>`。⚠️ **只看声明头**：用例**正文**里出现的 `JitOp::` 是「使用」，不是
「该 op 有 golden 用例」的证据。

covered: LOAD_CONST, LOAD_DATA, REG_NEXT, SELECT, STORE_DATA

## 未覆盖集（= 分母 − 分子）

uncovered: ADD, SUB, MUL, MOD, AND, OR, XOR, NOT, SHIFT_LEFT, SHIFT_RIGHT, ROTATE_LEFT, ROTATE_RIGHT, SEXT, ZEXT, SSHRSHN, NEG, EQ, NE, LT, LE, GT, GE, BIT_SELECT, BITS_EXTRACT, CONCAT, DIV, POPCOUNT, AND_REDUCE, OR_REDUCE, XOR_REDUCE, BITS_UPDATE, ASSIGN, CALL_EXTERNAL, NOP

## 增长规则（为什么未覆盖集可以存在，但不能悄悄变大）

- **新增 golden 用例** ⇒ ⇒ `covered` 加一项、`uncovered` 减一项、`denominator` 不变
  ⇒ ⇒ 两处 MUST **同批**改（只改一处 ⇒ 判据红）。
- **新增 `JitOp` 枚举值** ⇒ ⇒ `denominator` +1，且新名字 MUST 落进 `uncovered`
  ⇒ ⇒ 否则判据红（防「新增了 op 却没登记缺口」）。
- **把覆盖换成另一个 op** ⇒ ⇒ 计数不变，但 `covered` 集合变了 ⇒ ⇒ 判据**必红**
  （这正是本基线用集合而不用计数的理由）。
- ⚠️ **删除 `uncovered` 里的一项必须同时给出覆盖** —— 否则等于「用改基线代替写测试」。
