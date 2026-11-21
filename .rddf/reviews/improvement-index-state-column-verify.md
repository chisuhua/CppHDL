# 验收 —— 改进提案索引的**状态列**从 frontmatter 生成（+ 一致性护栏）

- **载体**: `scripts/lib/improvement_index.py`（生成器 + 判据）· `scripts/verify_improvement_index.sh`（薄壳 + fixture）
- **裁决**: **PASS**（0 失败项 · 已用轮次 **1**）
- **取证日**: 2026-11-21
- **性质**: **判据面补齐 + 治理漂移闭合**（零产品代码改动 · 零公开面改动）

---

## 1 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | **用户口径**（本会话） | 「做C3」⇒ C3 = 剩余清单里的「`improvement-suggestions.md` 索引表状态列自动生成」|
| 2 | `.rddf/roadmap/phase-6-rtl-productization.md` §4 ·「索引表状态列的自动生成」行 | 登记原文：现状 = **人工维护** ⇒ ⇒ **必然漂** ⇒ ⇒ 根本修法 = **从 frontmatter `状态:` 生成** + **一致性护栏**（类比 `verify_roadmap_fragments.sh` 的 vendored 渲染 + 一致性护栏）|
| 3 | 仓内既有纪律 | 「判别性护栏 MUST 有判别力自证」+「护栏 MUST 进 CI」+「登记表双向 1:1」+「**派生文件禁手改**」|

---

## 2 逐条判定 + 证据

### AC1 —— 生成器：状态词从 frontmatter 生成

```
$ python3 scripts/lib/improvement_index.py --render > /tmp/idx.md
$ diff /tmp/idx.md improvement-suggestions.md      # 一致时零差异
```

- 状态列 = `<emoji> **<word>**<附注>` ⇒ ⇒ 只**刷新 `<word>`** ⇒ ⇒ **emoji 与附注逐字保留**；
- fixture **F7** 钉死「`--render` 逐字保留 emoji 与附注」。

**③ 判定**：✅ 满足。

### AC2 —— 一致性护栏：**两个漂移方向**都转红

| 方向 | fixture | 实测 |
|---|---|---|
| 索引表手改了、frontmatter 没跟上 | **F1** | ✅ 红 |
| frontmatter 改了、索引表没跟上 | **F3** | ✅ 红 |

⇒ ⇒ 判据 = 「索引表的词 == frontmatter `状态:` 的词」⇒ ⇒ 双向覆盖 ✓

**③ 判定**：✅ 满足。

### AC3 —— fail-closed（四类缺失均红）

| 情形 | fixture | 实测 |
|---|---|---|
| frontmatter **缺** `状态:` 字段 | **F2** | ✅ 红 |
| 索引行指向的文件**不存在** | **F4** | ✅ 红 |
| 状态列**无** `**word**` 形态 | **F5** | ✅ 红 |
| 索引行形态异常（`|` 分段数 ≠ 7） | 代码路径 | ✅ 红（`collect()` 的分段数校验）|

⇒ ⇒ 索引文件缺失 / 无数据行 ⇒ ⇒ 同样红（`collect()` 的两条前置校验）。

**③ 判定**：✅ 满足。

### AC4 —— 边界：**不**判「每个 improvement 都在表里」

护栏输出**显式声明**该边界（`ℹ️ 边界：索引表是**提案池**…`）⇒ ⇒ 理由 = 索引表文件的
「生命周期」段（实施 / 归档后**从本表移除**）⇒ ⇒ 仓内 70+ 份 improvement · 表里 **16** 份
⇒ ⇒ 那样判必假红 ⇒ ⇒ 反向覆盖改由「表内每行都必须有对应文件」承担（F4 钉死）。

**③ 判定**：✅ 满足。

### AC5 —— 判别力自证（**8** 条 fixture · 5 类投毒 + 2 条渲染断言 + 1 条基线）

```
$ bash scripts/verify_improvement_index.sh --self-test
  ✓ F0 基线：一致的 fixture ⇒ 绿
  ✓ F1 索引表状态词与 frontmatter 不符 ⇒ 红
  ✓ F2 frontmatter 缺 `状态:` ⇒ 红
  ✓ F3 frontmatter 状态改了、索引表未跟上 ⇒ 红
  ✓ F4 索引行指向的文件不存在 ⇒ 红
  ✓ F5 状态列无 `**word**` 形态 ⇒ 红
  ✓ F6 `--render` 刷新状态词 ⇒ 渲染后绿
  ✓ F7 `--render` 逐字保留 emoji 与附注
✅ self-test: 8/8 fixture 全绿
```

⚠️ **fixture 在 `tempfile.mkdtemp()` 里**⇒ ⇒ **不碰真实仓** ⇒ ⇒ 不依赖真实仓历史
⇒ ⇒ 不会成为「第一个不可达护栏」（护栏可达性纪律 2）。

**③ 判定**：✅ 满足。

### AC6 —— 进 CI + 登记表双向 1:1

- `.github/workflows/ci.yml` 新增 **2** 个 step（`build-and-test` job · **无 gate**）：
  「Verify improvement index state column」+「(self-test)」；
- `scripts/verify_guard_reachability.sh --render` 重新派生登记表 ⇒ ⇒ 新增行：

```
| `scripts/verify_improvement_index.sh` | `build-and-test:Verify improvement index state column`,
  `build-and-test:Verify improvement index state column (self-test)` | `none` | — |
```

- 复跑：`✅ 三条静态判据全绿（55 个被引用脚本）· 登记表双向 1:1 且排除类型逐字派生一致`
  ⇒ ⇒ 排除类型 = `none`（CI 无 gate ⇒ 总是执行 ⇒ 可达）。

**③ 判定**：✅ 满足（⚠️ 「最近一次 executed+绿」列 = `—` ⇒ **CI 未跑过** ⇒ 见 §4 局限 ①）。

### AC7 —— 真实漂移：护栏首跑抓到 **3** 处 ⇒ 全部对齐

| 提案 | 索引表原值 | frontmatter | 处置 |
|---|---|---|---|
| `rv32i-isa-gap-closure-2026-11` | `draft` | `partial`（G3/G1/G2/G4/G5 已落地）| 索引表 `--render` 刷新 |
| `rv32i-hierarchical-example-2026-11` | `draft` | `approved`（ADR-070 Accepted）| 索引表 `--render` 刷新 |
| `openspec-purpose-placeholders-2026-11` | `implemented` | **缺 `状态:` 字段** | frontmatter 补 `状态: implemented`（对齐正文 `**Status**：✅ implemented` + change 已归档）|

⇒ ⇒ 这是 §4 登记「本轮三处漂移里 **2 处**出在索引表」的**独立实证**（本次又抓到 1 处
frontmatter 缺字段 ⇒ ⇒ 共 **3** 处）。

**③ 判定**：✅ 满足。

### AC8 —— 文档同步

| 落点 | 内容 |
|---|---|
| `.rddf/roadmap/phase-6-rtl-productization.md` §4 | 该行**追加 dated 注记**（**不改写**原登记 · 记口径 / 边界 / 3 处漂移 / 3 个实现期缺陷）|
| `docs/CHANGELOG.md` | 一条（缺口 / 载体 / 口径 / 判据 / 边界 / 3 处漂移表 / 3 个实现期缺陷）|

**③ 判定**：✅ 满足。

---

## 3 回归面

```
ctest -LE perf                            → rc=0（base 237 + chlib 1 + spinalhdl-ported 24 + verilator_e2e 80）
./run_all_ported_tests.sh                 → PASS: 27 · FAIL: 0 · STALE: 0 · MISSING: 0
bash scripts/verify_*.sh                  → 53 个（含**新增** `verify_improvement_index`）全 PASS · 0 FAIL
```

另：`verify_guard_reachability.sh`（含 `--self-test` 6/6）· `verify_docs_links` ·
`verify_roadmap_fragments` · `verify_ledger_evidence` · `verify_agents_size` ·
`verify_test_counts` · `verify_public_api_visibility` 全 PASS。

---

## 4 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / 半截实现 | **0** |
| 临时调试输出 | **0** |
| 临时文件 | **0**（fixture 在 `tempfile.mkdtemp()` ⇒ 出作用域即删 ⇒ **不碰真实仓**）|
| 被 skip 的测试 | **无** |
| 公开面登记义务 | **不触发**（未改任何公开符号）|
| 探针 / 临时 CMake 条目 | **无** |

**已知局限（登记，不修）**：

1. **「最近一次 executed+绿」列 = `—`** ⇒ ⇒ 新护栏**尚未在 CI 里真正 executed 且绿**过
   （护栏可达性纪律 3）⇒ ⇒ **待 CI 回填**（本地无法代替 ⇒ 如实登记，**不**谎报）。
2. **emoji 不参与判定** ⇒ ⇒ `approved` 目前有 `📋` 与 `✅` 两种写法 ⇒ ⇒ **不**判红
   ⇒ ⇒ 归一化 emoji 属**独立小项**（本次不做 ⇒ 避免无谓 diff）。
3. **`--render` 需手工跑**（无自动 hook）⇒ ⇒ 漂移由 CI 护栏**抓住**⇒ ⇒ 修复动作是
   「跑 `--render`」⇒ ⇒ 这与 `verify_roadmap_fragments.sh` 的定位一致
   （生成器只负责写，护栏负责判「该不该写」）。
4. **`状态:` 的值 MUST 以 ASCII 词法 token 开头**（`draft` / `partial` / …）⇒ ⇒ 非 ASCII
   开头 ⇒ ⇒ fail-closed 判红（「无法抽出词」）⇒ ⇒ 这是**刻意**的（保证词可机械比对）。

---

## 5 实现期抓到的自身缺陷（fixture 抓的 ⇒ ⇒ 证明 fixture 有判别力）

| # | 缺陷 | 发现方式 | 处置 |
|---|---|---|---|
| ① | frontmatter 键正则只认 `[A-Za-z_]` ⇒ ⇒ **`状态:` 是中文键** ⇒ ⇒ 永远解析不到 ⇒ ⇒ 护栏对**每一行**都误判红 | fixture **F0/F6/F7 全红**（基线不该红） | 改为「第一个 `:` 前」（键名允许非 ASCII；嵌套键因前导空白被跳过）|
| ② | 状态词不一致的报错用 `%` 格式化时**占位符与参数个数不匹配**（3 个 `%s` vs 4 个参数）⇒ ⇒ Python 编译器**不报错** ⇒ ⇒ 只有 fixture 能抓到 | fixture **F1** 抛 `TypeError` | 修正占位符（`%s:%d` + 3 个 `%s`）|
| ③ | `--render` 用 `print` ⇒ ⇒ **追加**尾部换行 ⇒ ⇒ 渲染结果比原文件多一个空行 ⇒ ⇒ 与「逐字一致」冲突 | `diff` 报 `28d27` | 改用 `sys.stdout.write` |

⚠️ 三条都**不是**「测试写得不好」，而是**实现缺陷被 fixture 抓到** ⇒ ⇒ 与「判别性探针」的
目的同型 ⇒ ⇒ 如实登记。

---

## 6 已用轮次

**1**（首次验收即通过；无打回 ⇒ 剩余额度 2）。
