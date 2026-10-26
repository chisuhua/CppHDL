# Tasks: examples 测试断言质量护栏

> 承接自 C1（`riscv-mini-test-triage-2026-10`）的拆分：C1 只保留「归属守恒」，
> 本 change 承接「断言质量」。**强前置**：C1 已归档。

## Phase 1: fixture 集（= 规格）

- [x] T1.1 ✅ 建 `tests/fixtures/assertion_quality/` 目录与 10 个最小 fixture
  （`live_assert` / `commented_out` / `dead_macro` / `dead_elif` /
  `dead_ifdef_else` / `live_else` / `live_elif` / `word_boundary` /
  `throws_only` / `succeed_only`），每个带注释说明它钉住哪种形态
  （2026-10-26 落地；fixture **不参与编译** —— `tests/CMakeLists.txt` 用显式源文件
  列表，无 `GLOB_RECURSE`）
- [x] T1.2 ✅ 建自检脚本 `scripts/verify_assertion_quality_fixtures.sh`：
  对每个 fixture 断言 `A` 与逐用例计数的**期望值**
  （2026-10-26 落地；期望值**显式写死**在脚本的期望表 + 每个 fixture 的文件头
  注释里 ⇒ 不得由实现反推；C1：期望表未登记的 fixture 文件 ⇒ 报违规）
- [x] T1.3 ✅ 把自检脚本挂进 CI（与护栏同处 2 个 job：`build-and-test` 矩阵 +
  `pr-verilator-fast`）
- [x] T1.4 ✅（2026-10-26 实施时新增）**共享 lib 抽取**：
  `scripts/lib/assertion_quality.sh` 承载 `analyze_source`（分析器）+
  `aq_analyze_status` / `aq_violation_reason`（fail-closed 判定）；
  护栏与 fixture 自检**共用同一份** ⇒ 实现与规格不可能分叉。
  ⚠️ **行为等价性已实测**：抽取前后护栏输出**逐字一致**
  （`diff` 基线快照 ⇒ 无漂移）。
  ⚠️ **实测确认的已登记局限**（2026-10-26）：逐用例扫描用**子串**匹配 `TEST_CASE`
  ⇒ 含该子串的标识符（如 `MY_TEST_CASE(`）会**吃掉一个名字槽** ⇒ 后续真实用例的
  名字被误报为 `?`；**计数不受影响**。已由 fixture `identifier_lookalike.cpp` 钉住，
  并写进 lib 的「能力边界 ④」。

## Phase 2: 护栏收尾

- [x] T2.1 ✅ `CH4_ANCHOR` 与 `CH4_ANCHOR_README` **已在**
  `scripts/verify_example_assertion_quality.sh` 内（随 C1 拆分交付，**不**与
  C1 的护栏共享）
- [x] T2.2 ✅ 确认 `scripts/verify_example_test_wiring.sh` 内**零**静态分析代码
  （`grep -c 'analyze_source\|CH4_ANCHOR\|count_asserts'` = 0 ——
  2026-10-26 实测确认）
- [x] T2.3 ✅ 护栏文档已含「能力边界」小节（①②③，明确不覆盖等量替换 / 宏生成用例）

## Phase 3: 跨平台

- [ ] T3.1 **macOS-14（BSD/BWK awk）实测**：`bash scripts/verify_example_assertion_quality.sh`
      与 fixture 自检在 macos-14 上均绿
      （⚠️ C1 第 6 轮才发现「组内 `^`」在 POSIX 中未定义 ⇒ 这条不可省）
      ⚠️ **2026-10-26 补注**：本机是 Linux ⇒ **本地无法完成** ⇒ 权威结果 =
      **CI 的 `macos-14` job** ⇒ 归档前 MUST 看到该 job 绿（禁止「假定 OK」）。
      ⚠️ 本次抽取 lib 时已把「组内 `^`」换成**哨兵空格**写法（lib 的 `count_asserts`
      里 `s = " " str` + `[^A-Za-z0-9_]`）⇒ 规避了该 POSIX 未定义项 ⇒ macOS 风险面收窄。
- [ ] T3.2 若 BSD awk 有差异：改用 POSIX 明确定义的构造（哨兵空格法已在 C1 采用）
- [ ] T3.3 在 proposal 里记录实测平台与结果（不写「假定 OK」）

## Phase 3.5: 失效路径护栏（2026-10-24 第 8 轮审稿新增）

- [ ] T3.5.1 fixture 增两个「分析器失效」形态：
      ① `analyze_source` 返回空；② awk 在输出 `A`/`S` 之后 `exit 3`
      ⇒ 两者 MUST 都被护栏报违规（**不是**打出「0 个字面用例」的假 OK）
- [x] T3.5.2 ✅ 护栏已有**结束标记**（`E` 行）以区分「完整跑完」与「中途退出」
      （2026-10-24 第 8 轮审稿落地）
- [x] T3.5.3 ✅ `SRC_LIST` 为空 ⇒ 报违规（否则两个循环都不执行 ⇒ 假绿）
      （2026-10-24 第 8 轮审稿落地）
- [x] T3.5.4 ✅ 逐用例计数 MUST 与 `E` 行携带的字面用例数**相等** ——
      否则 raw string（`R"( ... )"`）里的裸 `{` / `"` 破坏 brace-matching ⇒
      逐用例扫描一行 `C` 都不产出 ⇒ 假绿。（2026-10-24 第 9 轮审稿落地，
      但**归属本 change** ⇒ 从本 change 的 checklist 中移除，改为**已实现**）

## Phase 4: 验收

- [x] T4.1 ✅ `bash scripts/verify_example_assertion_quality.sh` 绿
      （2026-10-26 实测；且重构前后输出**逐字一致** ⇒ 抽取 lib 无行为漂移）
- [x] T4.2 ✅ fixture 自检绿（11/11 —— 比 proposal 的 10 个多 1 个
      `identifier_lookalike`，钉住实测确认的局限「名字错位、计数不错」）
- [x] T4.3 ✅ 变异验证 4 条全被抓（2026-10-26 实测，逐一改坏 lib 后自检转红）：
      ① 行注释不剥离 ② 宏体被计入 ③ `#elif` 死分支被计入 ④ `SUCCEED` 被当断言
      ⇒ 自检对每种机制都有判别力（非「能跑 vs 崩」）
- [x] T4.4 ✅ 假阳性回归：`#if 0 / #else` 内的活断言**不**报违规
      （fixture `live_else.cpp` 期望 `A`=1 ⇒ 活断言计入 ⇒ 不误报）
- [x] T4.5 ✅ `openspec validate --strict` 通过
      （2026-10-26：`Change 'example-assertion-quality-2026-10' is valid`）
- [x] T4.6 ✅ `scripts/verify_example_test_wiring.sh` 仍绿（拆分未破坏归属守恒）
- [ ] T4.7 ⏳ **macOS-14 实测**（= Phase 3 的 T3.1–T3.3）：
      本机是 Linux ⇒ **无法本地完成** ⇒ 权威结果 = **CI 的 `macos-14` job**
      ⇒ 归档前 MUST 看到该 job 绿（禁止用「假定 OK」代替）

## Out of Scope

- 防「等量替换」（由 code review 承担，护栏 MUST NOT 声称覆盖）
- 覆盖宏生成的用例（由 `NOTICE` 点名）
- 引入 C++ 预处理器（`cpp -E`）作为实现
