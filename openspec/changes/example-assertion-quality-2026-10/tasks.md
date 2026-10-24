# Tasks: examples 测试断言质量护栏

> 承接自 C1（`riscv-mini-test-triage-2026-10`）的拆分：C1 只保留「归属守恒」，
> 本 change 承接「断言质量」。**强前置**：C1 已归档。

## Phase 1: fixture 集（= 规格）

- [ ] T1.1 建 `tests/fixtures/assertion_quality/` 目录与 10 个最小 fixture
  （`live_assert` / `commented_out` / `dead_macro` / `dead_elif` /
  `dead_ifdef_else` / `live_else` / `live_elif` / `word_boundary` /
  `throws_only` / `succeed_only`），每个带注释说明它钉住哪种形态
- [ ] T1.2 建自检脚本 `scripts/verify_assertion_quality_fixtures.sh`：
  对每个 fixture 断言 `A` 与逐用例计数的**期望值**
- [ ] T1.3 把自检脚本挂进 CI（与护栏同处 2 个 job）

## Phase 2: 护栏收尾

- [x] T2.1 ✅ `CH4_ANCHOR` 与 `CH4_ANCHOR_README` **已在**
  `scripts/verify_example_assertion_quality.sh` 内（随 C1 拆分交付，**不**与
  C1 的护栏共享）
- [ ] T2.2 确认 `scripts/verify_example_test_wiring.sh` 内**零**静态分析代码
  （`grep -c 'analyze_source\|CH4_ANCHOR\|count_asserts'` = 0）
- [x] T2.3 ✅ 护栏文档已含「能力边界」小节（①②③，明确不覆盖等量替换 / 宏生成用例）

## Phase 3: 跨平台

- [ ] T3.1 **macOS-14（BSD/BWK awk）实测**：`bash scripts/verify_example_assertion_quality.sh`
      与 fixture 自检在 macos-14 上均绿
      （⚠️ C1 第 6 轮才发现「组内 `^`」在 POSIX 中未定义 ⇒ 这条不可省）
- [ ] T3.2 若 BSD awk 有差异：改用 POSIX 明确定义的构造（哨兵空格法已在 C1 采用）
- [ ] T3.3 在 proposal 里记录实测平台与结果（不写「假定 OK」）

## Phase 4: 验收

- [ ] T4.1 `bash scripts/verify_example_assertion_quality.sh` 绿
- [ ] T4.2 fixture 自检绿（10/10）
- [ ] T4.3 变异验证 4 条全被抓：注释掉断言 / 死宏体 / `#elif` 死分支 / `SUCCEED`-only 用例
- [ ] T4.4 假阳性回归：`#if 0 / #else` 内的活断言**不**报违规
- [ ] T4.5 `openspec validate --strict` 通过
- [ ] T4.6 `scripts/verify_example_test_wiring.sh` 仍绿（拆分未破坏归属守恒）

## Out of Scope

- 防「等量替换」（由 code review 承担，护栏 MUST NOT 声称覆盖）
- 覆盖宏生成的用例（由 `NOTICE` 点名）
- 引入 C++ 预处理器（`cpp -E`）作为实现
