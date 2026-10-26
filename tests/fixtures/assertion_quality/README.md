# `tests/fixtures/assertion_quality/` —— 断言质量护栏的 **fixture 集（= 规格）**

## 这是什么

`scripts/verify_example_assertion_quality.sh`（断言质量护栏）的**静态分析机制**是手写的
C 子集词法分析，而 C1（`riscv-mini-test-triage-2026-10`）在连续三轮对抗审稿
（第 4 / 5 / 6 轮）里**全部** BLOCKER 都出在它身上：

| 轮次 | BLOCKER | 根因 |
|---|---|---|
| 4 | 用「`REQUIRE` 词频」冒充「可执行断言」 | `grep -c` 数的是**行**、只剥整行注释 |
| 5 | `#define` 宏体 / `#if 1` 死分支可**灌计数** | 未建模预处理指令 |
| 6 | `#elif` 死分支被计入；`#ifdef` 的 `#else` 被当活代码 | 条件编译状态机不完整 |

⇒ 护栏进了 CI 却**没有任何可执行的期望值**：「它到底认什么」只有散文描述。
本目录的 11 个 fixture 就是那份**可执行的期望值**。

## 每个 fixture 钉住一种机制

| fixture | 钉住的形态 | 期望 |
|---|---|---|
| `live_assert.cpp` | 正常（断言在活代码里） | `A`=1 |
| `commented_out.cpp` | 断言被 `//` / `/* */` 注释 | `A`=0 |
| `dead_macro.cpp` | 断言在**永不展开**的 `#define` 宏体里 | `A`=0 |
| `dead_elif.cpp` | `#if 1` 之后的 `#elif 1`（死分支） | `A`=0，`S`=1 |
| `dead_ifdef_else.cpp` | `#ifdef X` 整组（含 `#else`）保守跳过 | `A`=0，`S`=2 |
| `live_else.cpp` | `#if 0` 死 / `#else` 活 | `A`=1 |
| `live_elif.cpp` | `#if 0` 死 / `#elif 1` 活 | `A`=1 |
| `word_boundary.cpp` | `MYREQUIRE(` / `XCHECK(` 不是断言宏 | `A`=0 |
| `throws_only.cpp` | 只有 `REQUIRE_THROWS` | `A`=1 |
| `succeed_only.cpp` | 只有 `SUCCEED()`（**故意违反** C4b 的反例） | `A`=0 |
| `identifier_lookalike.cpp` | `MY_TEST_CASE(` 这类含 `TEST_CASE` 子串的标识符 | `A`=2（**钉住已登记的局限**：名字错位、计数不错） |

期望值**写死**在 `scripts/verify_assertion_quality_fixtures.sh` 的期望表 +
每个 fixture 的文件头注释里 ⇒ **不得由实现反推**（那会变成循环论证）。

## 三条纪律

1. **fixture 优先**：任何扩展分析机制 MUST **先补 fixture** 再改实现
   （delta「静态分析的期望值 MUST 有可执行的 fixture」）。
2. **单一实现**：护栏与自检共用 `scripts/lib/assertion_quality.sh`
   ⇒ 两份实现会漂移 —— 那正是 C1 三轮 BLOCKER 的来源。
3. **刻意不参与编译**：本目录的 `.cpp` 是**静态分析 fixture**，**不是**测试源。
   `tests/CMakeLists.txt` 用**显式源文件列表**（无 `GLOB_RECURSE`）⇒ 它们不会被编译、
   不会被 `ctest` 执行、也**不计入** `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES`。
   ⇒ 它们不是「孤儿文件」，而是**规格数据**（与 `tests/benchmark/perf_baseline.json`
   同型：进版本库、不进构建）。

## 怎么跑

```bash
bash scripts/verify_assertion_quality_fixtures.sh   # 自检（CI 已挂 2 个 job）
bash scripts/verify_example_assertion_quality.sh    # 护栏本体
```
