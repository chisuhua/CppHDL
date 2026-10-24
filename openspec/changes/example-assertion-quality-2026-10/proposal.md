# 提案：examples 测试断言质量护栏（assertion quality）

## Why

`examples/riscv-mini-test-triage-2026-10`（C1）的立论是「**绿 ≠ 对**」：让从未被执行的
测试真正跑起来，并消除「账面覆盖」。实测发现「账面覆盖」有**两个独立的维度**：

1. **归属维度**（测试源有没有被执行到）—— C1 已用 `scripts/verify_example_test_wiring.sh`
   的 C0/C1/C2/C3/C3b 覆盖（判据 = `ctest -N`、基准 = 文件系统源文件集合、
   锚点 = root `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES`、运行期 exit + error/fatal 判据）。
2. **断言维度**（被执行的测试有没有真的断言语义输出）—— 本 change 负责。

C1 在连续三轮对抗审稿（第 4 / 5 / 6 轮）里，**全部** BLOCKER 都出在维度 2 的实现上：

| 轮次 | BLOCKER | 根因 |
|------|---------|------|
| 4 | C4 用「`REQUIRE` 词频」冒充「可执行断言」 | `grep -c` 数**行**、只剥整行注释 |
| 5 | `#define` 宏体 / `#if 1` 死分支可**灌计数** | 未建模预处理指令（**in-tree 已有实例**：`test_riscv_tests.cpp` 的 41 个宏生成用例） |
| 6 | `#elif` 死分支被计入；`#ifdef` 的 `#else` 被当活代码 | 条件状态机不完整 —— 且**新写的收窄声明立刻被自己的实现证伪** |

⇒ 结论（已写入 C1 的 proposal）：**任何自研的「C 子集静态分析」都不可能被完全闭合**，
每修一个绕过形态就会暴露下一个；而维度 1 的机制**不依赖**该分析器。

⇒ **拆分**：C1 只保留维度 1（`verify_example_test_wiring.sh`，370 行，简单可审、portable）；
维度 2 独立成**本 change**（`scripts/verify_example_assertion_quality.sh`），自带
**fixture 集作为规格**，长期演进。

## What Changes

### `scripts/verify_example_assertion_quality.sh`

> ⚠️ **该脚本与其 CI 挂载已随 C1 的拆分一并交付**（commit `fadf500`）⇒
> 本 change **不是**「引入」它，而是**把它规格化 + 补齐其验证**：
> ① fixture 集（当前**零**规格覆盖 ⇒ 脚本进了 CI 却没有可执行的期望值）；
> ② 跨平台实测（当前 macOS/BSD awk **未实测**）；
> ③ 文档与能力边界对齐（护栏头的「能力边界」必须与 delta 一致）。
>
> 拆分时的归属：C1 只保留「归属守恒」，「断言质量」连同该脚本归本 change。

- **静态分析**（`analyze_source`）：从测试源提取「**活代码**」并计数
  - 剥：整行/行尾/跨行块注释、字符串与字符字面量
  - 跳过：`#define` 宏体整行（含 `\` 续行）—— 宏体**可能永不展开** ⇒ 计入即后门
  - 条件编译：每层记 `gres`（求值结果 0/1/2=未知）+ `gtaken`（是否已有分支被取）
    - `#if <字面常量>` 求值；`#if 0` 区段跳过但**其 `#else` 之后的活代码计入**
    - `#if 1` 之后的 `#elif` / `#else` **都是死的** ⇒ 跳过
    - `#if 0 / #elif 1` 的 `#elif` 体**是活的** ⇒ 计入
    - `#ifdef` / `#ifndef` / 非常量 `#if` ⇒ **连同 `#elif`/`#else` 整组跳过**（宁可少算）
  - 词边界：`MYREQUIRE(` / `XCHECK(` 不计（哨兵空格实现，避免「组内 `^`」的 POSIX 未定义行为）
  - 断言宏表：Catch2 的 `REQUIRE` / `CHECK` / `*_FALSE` / `*_THROWS*` / `*_NOTHROW` / `STATIC_*`
- **C4（文件级）**：每个测试源的可执行断言数 MUST ≥ `CH4_ANCHOR` 登记的下限
- **C4b（用例级）**：每个**字面**用例 MUST ≥ 1 条可执行断言（防「某用例只有 `SUCCEED`」
  —— 文件级下限会被同文件其它用例掩盖）
- **可见性**：被条件编译跳过的区段里若有用例宏 ⇒ 报 `NOTICE`（不静默）

### 配套 fixture 集（本 change 的**规格**）

新增 `tests/fixtures/assertion_quality/` 下的最小用例文件 + 一个自检脚本，把上表每一行
「机制 → 期望计数」固化成**可执行**的期望值：

| fixture | 内容 | 期望 |
|---------|------|------|
| `live_assert.cpp` | 正常文件 | `A` = 断言数 |
| `commented_out.cpp` | 断言被 `//` / `/* */` 注释 | `A` = 0 |
| `dead_macro.cpp` | 从不展开的 `#define` 内含断言 | `A` = 0 |
| `dead_elif.cpp` | `#if 1 / #elif 1 / REQUIRE` | `A` = 0 |
| `dead_ifdef_else.cpp` | `#ifdef X / #else / REQUIRE` | `A` = 0 |
| `live_else.cpp` | `#if 0 / #else / REQUIRE` | `A` = 1 |
| `live_elif.cpp` | `#if 0 / #elif 1 / REQUIRE` | `A` = 1 |
| `word_boundary.cpp` | `MYREQUIRE(` / `XCHECK(` | `A` = 0 |
| `throws_only.cpp` | 只有 `REQUIRE_THROWS` | `A` = 1、C4b 通过 |
| `succeed_only.cpp` | 只有 `SUCCEED` | `A` = 0、C4b **报违规** |

### 挂进 CI

- 与 `verify_example_test_wiring.sh` 同处 2 个 job（`build-and-test` 矩阵 + `pr-verilator-fast`）
- ⚠️ **CI 的 macos-14 会用 BSD/BWK awk 跑本脚本** ⇒ 归档前 MUST 在该平台实测一次
  （C1 的教训：第 6 轮才发现「组内 `^`」在 POSIX 中未定义）

## Acceptance

> ✅ = **随 C1 拆分已交付**（`fadf500`）；⬜ = 本 change 待做

- ✅ [x] `bash scripts/verify_example_assertion_quality.sh` 在当前 examples 树上绿
- ✅ [x] 该脚本已挂进 CI 的 2 个 job
- [ ] 上表 **10 个 fixture** 全部被自检脚本验证（期望值逐一相符）
- [ ] 变异验证：注释掉断言 / 死宏体 / `#elif` 死分支 / `#ifdef` 死 `#else` /
      `SUCCEED`-only 用例 —— **全部**被护栏报违规
- [ ] 假阳性回归：`#if 0 / #else` 内的活断言**不**报违规
- [ ] **macOS-14（BSD awk）实测通过**
- [ ] `scripts/verify_example_test_wiring.sh` 不再包含任何静态分析代码（拆分干净）

## Non-Goals

- **不防「等量替换」**（把 `REQUIRE(pc == X)` 换成 `REQUIRE(1 == 1)` 断言数不变）
  ⇒ 任何计数守恒都发现不了 ⇒ 由 **code review** 承担。护栏 MUST NOT 声称覆盖它。
- **不覆盖宏生成的用例**（如 `TEST_CASE_FOR_each`）：其断言在宏体里 ⇒ 按「宏体不计」
  的保守规则不计入 ⇒ 该类用例对 C4b **不可见**（由 `NOTICE` 点名）
- 不引入 C++ 预处理器（`cpp -E`）作为实现 —— 那需要为每个 target 复现 include 路径，
  成本与漂移风险都高于收益

## Impact

- **已交付**（随 C1 的拆分 `fadf500`）：`scripts/verify_example_assertion_quality.sh`、
  其 CI 挂载（2 处）
- **本 change 新增**：`tests/fixtures/assertion_quality/**`、
  `scripts/verify_assertion_quality_fixtures.sh`、跨平台实测记录
- **不改**：`scripts/verify_example_test_wiring.sh`（C1 已拆干净）
- **依赖**：C1 的 `riscv-mini-test-triage-2026-10` 归档（本 change 承接其维度 2）
- **风险**：静态分析**不可能完全闭合**（本 change 的存在本身即为此断言）⇒
  任何扩展 MUST 先补 fixture 再改实现
