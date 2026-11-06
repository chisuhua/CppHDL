# 任务清单：`component-io-const-and-node-name-policy-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（2 个 capability，各 ADDED ×1）。
> ⚠️ **与层次化主线无依赖** ⇒ ⇒ 可全程并行。

## T1 U-2：`io() const`

- [x] 1.1 `__io` 宏加 const 重载（`const io_type& io() const`）
      ⇒ ⇒ **已落**（`include/core/io.h:319` 起的宏体）⇒ 两个重载：
      非 const ⇒ `io_type&`（既有行为**逐字不变**）· const ⇒ `const io_type&`
      ⇒ ⇒ ⚠️ **纯新增**（`io_storage_` 是 `char[]`）⇒ 重载决议对非 const 对象仍选非 const
      版本 ⇒ 既有代码**零变化**
- [x] 1.2 **编译期断言**（`static_assert` 或等价编译期探针）：const 上下文可读
      `impl()` / `name()` / `is_valid()`
      ⇒ ⇒ **已落**（`tests/test_component_io_const_and_node_name_policy.cpp`）：
      两处 `static_assert`（const 对象 ⇒ `const io_type&` · 非 const 对象 ⇒ `io_type&`）
      + `IoConstProbe::io_readable_in_const()`（const 成员函数里读三个访问器）
- [x] 1.3 **判别性对照**：移除 const 重载 ⇒ 断言 MUST **编译失败**
      ⇒ ⇒ **已验**（变异 **M-1**）：删掉 const 重载 ⇒ 该测试目标**编译失败** ✅

## T2 U-1：opt-in 节点名唯一性

- [x] 2.1 `context` 加策略枚举（`kAllowDuplicates`（**默认**）/ `kRejectDuplicates`）+ setter
      ⇒ ⇒ **已落**（`include/core/context.h`）：`enum class NodeNamePolicy` +
      `node_name_policy()` / `set_node_name_policy()` ⇒ ⇒ 默认值 = `kAllowDuplicates`
- [x] 2.2 创建点（`create_input` / `create_output` 等）检测同名 ⇒ **抛异常**
      （MUST NOT 只打日志）⇒ ⇒ 错误信息含**冲突的节点名**
      ⇒ ⇒ **已落**：**唯一判定实现** = `context::check_node_name_unique()`
      （`src/core/context.cpp`）⇒ ⇒ 调用点 = **节点创建漏斗** `create_node<T>()`
      （`include/lnode/context.tpp`）⇒ ⇒ ⇒ 覆盖**全部**节点类型（不止 IO 端口
      —— 实测 `ch_reg` 默认名恒为 `reg` ⇒ 32 个同名者正是在 opimpl 这一层产生的）
      ⇒ ⇒ 错误信息含**冲突的节点名** + ctx 名 + `file:line`
- [x] 2.3 ctx MUST **不改名**
      ⇒ ⇒ **已验**：宽松模式下 `a->name() == b->name() == "dup"`（**无** `_N` 后缀）
      ⇒ ⇒ 变异 **M-4**（冲突时自动改名）⇒ 测试**转红** ✅
- [x] 2.4 **两层**护栏：① 开启 ⇒ 抛 ② 默认 ⇒ 不抛
      ⇒ ⇒ **已验**：`[node_name_policy]` 的 9 个用例里
      ① 「严格模式 ⇒ 创建点抛异常且错误信息含冲突名」·「重名 DUT ⇒ build 抛」
      ② 「默认宽松 ⇒ 同名可共存」·「宽松模式 ⇒ build 成功」
      ⇒ ⇒ **各自独立成用例**（⇒ 任一层失效都会被单独抓到）

## T3 零回归

- [x] 3.1 `ctest -LE perf` / samples / examples 全绿 · 计数锚点同步
      ⇒ ⇒ **已跑**（2026-11-06 实测）：
      `cmake --build` **0 error** · `ctest -LE perf` **240/240**
      （`100% tests passed, 0 tests failed out of 240`）· samples **39/39** ·
      examples **28/28** · `verify_dual_backend_parity.sh` **66 / 289**（未变）·
      `verify_header_self_contained.sh` PASS · `verify_test_counts.sh` PASS
      ⇒ ⇒ 计数锚点 `CTEST_TOTAL` **250 → 251**（本 change 新增 1 个 ctest 条目）·
      non-perf **239 → 240** · `tests/test_*.cpp` **142 → 143** ·
      `VERILATOR_E2E_TOTAL` **不变**（76）⇒ ⇒ 新测试**不带** `verilator_e2e` 标签
      （它不跑真 verilator）
      ⇒ ⇒ ⚠️ **产物零变化判据**（roadmap T2b 的验收项「policy=off 时产物 `cmp` 0」）：
      用 `build/tests/hier_rtl_products` 生成 **3 个产物**（`bubble.v` /
      `row1_implicit.v` / `three_layers.v`）⇒ ⇒ 与「临时还原到 HEAD 后重建」的对照件
      **逐字节相同**（`DIFF_COUNT = 0`）⇒ ⇒ 还原后**逐字节还原** 4 个源文件 +
      **全量重建** + 测试**复绿**（41 断言）
- [x] 3.2 ⚠️ `policy=on` 的判据 MUST 用**专门构造的重名 DUT**
      —— 仓内 `include/` 的 641 个 `ch_in` 声明**零**显式名字 ⇒ ⇒
      拿现有设计当 DUT ⇒ 全仓转红是**规格预期**，不是缺陷
      ⇒ ⇒ **已落**：`DupNameDut`（`describe()` 里两个**显式**同名 `create_input`）
      ⇒ ⇒ 断言「严格 ⇒ `build()` 抛（消息含 `same_name`）」+「宽松 ⇒ `build()` 成功」
      ⇒ ⇒ ⚠️ 变异 **M-5**（默认改成严格）⇒ 该 DUT 的**宽松**分支转红
      ⇒ ⇒ 这正是 641 个零显式名字的 `ch_in` 在「默认严格」下会全仓转红的**最小复现**

## 实现期的两处**承重**决定（超出 proposal 字面 · 已在 delta / 代码注释里写死）

| # | 决定 | 为什么 |
|---|---|---|
| **D1** | 检查放在**节点创建漏斗**（`create_node<T>`）而非逐个 `create_input` / `create_output` | ① 单一判定实现 ⇒ 不会漏路径；② `ch_reg` 的 32 个同名 `reg` 是在 **opimpl** 这一层产生的 ⇒ 只查 IO 创建点**抓不到** proposal 自己举的证据 |
| **D2** | 检查 MUST 在 `create_node` 的 `try` **之外** | 该函数有 `catch (const std::exception &)` ⇒ 放在里面会被**吞掉** ⇒ 「抛异常中止」退化成**静默 `nullptr`** ⇒ 违反 fail-fast 契约（变异 M-3 实测：只记录不抛 ⇒ 3 条断言转红） |

## 实现期定义的**降级行为**（planner 在 roadmap 的 U2 ⑤ 要求「MUST 明确定义」）

> ⚠️ 静态析构期抛异常会 `std::terminate`（既有 `CHFATAL_*` 一族宏在该期**一律不抛**）
> ⇒ ⇒ 若不定义，则「off 不抛」与「on 抛」之间存在**未定义带**。

**定义**：静态析构期 ⇒ **不抛** · **记录**到 `context::duplicate_name_violations()` ·
且该次创建 **失败**（节点**不**进入 ctx）⇒ ⇒ 「记录 + 失败结果」二者缺一不可。

⇒ ⇒ **已落进 delta**（`core-ctx-node-name-uniqueness` 新增 Scenario
「静态析构期 MUST 降级为『记录 + 失败结果』」）—— 因为归档后主 spec 是唯一权威，
不写进 delta ⇒ 这条已定义行为会**随归档死亡**。
⇒ ⇒ 已验：用例「静态析构期降级」+ 变异 **M-6**（取消降级 ⇒ 改成永远抛）⇒ 测试**转红** ✅

## 判别力自证（变异对照 · 每条先确认仍能编译 · 还原后重建复绿）

| 变异 | 改动 | 期望 | 实测 |
|---|---|---|---|
| **M0** | 无 | 绿 | ✅ 9 用例 / 33 断言 · `All tests passed` |
| **M-1** | 删 `__io` 的 **const 重载** | **编译失败** | ✅ 目标**编译失败**（`static_assert` / `io_readable_in_const` 均依赖它） |
| **M-2** | 唯一性检查**恒真**（`if (true)`） | 红 | ✅ `23 passed / 4 failed` |
| **M-3** | 只记录**不抛** | 红 | ✅ `26 passed / 3 failed` |
| **M-4** | 冲突时**自动改名**（`+= "_1"`） | 红 | ✅ `23 passed / 4 failed` |
| **M-5** | 默认策略改成**严格** | 红 | ✅ `23 passed / 2 failed`（宽松分支转红） |
| **M-6** | 取消静态析构期降级（永远抛） | 红 | ✅ `19 passed / 3 failed` |
| **M-7** | 切严格时**取消播种**既有节点 | 红 | ✅ `36 passed / 2 failed`（两条 BLOCKER 闭合探针都转红） |

⇒ ⇒ **还原完整性**：`io.h` / `context.cpp` `cmp` 逐字节相同 · `context.h` sha256 相同
⇒ ⇒ **还原后重建 + 复绿**（`All tests passed (33 assertions in 9 test cases)`）。

> ⚠️ **如实登记一次无效的变异流程**：首轮 M-5 的脚本**先改后备份** `context.h`
> ⇒ ⇒ 还原写回了**变异版** ⇒ 复验红。⇒ ⇒ 已按「先备份 → 改 → 跑 → 从未变异备份还原 →
> sha256 比对 → 重建 → 复绿」重跑 ⇒ 绿。⇒ 复现「还原源码 ≠ 复原（二进制也是状态）」的反面：
> **备份顺序也是状态**。

## 审查（critic · `build` 档 · 公开 API 面）

**第 1 轮**：`REJECT` · `mergeable: no` ⇒ **1 个 BLOCKER** + 2 个 SHOULD_FIX + 2 个 NIT。

### BLOCKER（已闭合）

> 「登记是**严格期增量式**的 ⇒ 宽松期创建的名字（含 `ctx` 构造时 `init()` 建的
> `default_clock` / `default_reset`）**永远不进登记表** ⇒ 在**任何** ctx 上开启严格后
> 创建同名节点**不会抛** —— 而 delta 的 Scenario 字面是「创建了一个与**既有节点**同名的
> 新节点 ⇒ MUST 抛出异常」。」

⇒ ⇒ **成立**。「先宽松建 DUT、后开严格」这个用法下保护是**幻觉** ⇒ 恰好回到本 feature
要治的「按名采到错节点 ⇒ 假绿」。

**闭合路径（选第三条 · 优于 critic 给的两条）**：`set_node_name_policy(kRejectDuplicates)`
时把**既有节点**的名字**播种**进登记表（`src/core/context.cpp`）。

| 路径 | 契约是否成立 | 默认路径开销 |
|---|---|---|
| ① 收窄 delta 措辞到「开启后登记的节点」 | 需改 delta（**弱化契约**） | 零 |
| ② 宽松期也登记（仅严格期强制） | ✅ | **每节点一次 map 插入** ⇒ 违反「默认零开销」 |
| **③ 切严格时播种（本轮采用）** | ✅（契约**不弱化**） | **零**（只在切换时一次 O(N)） |

⇒ ⇒ 已验（**M-7** 取消播种）：`[node_name_policy]` 的 2 条断言**转红**
（`no exception was thrown where one was expected`）⇒ 播种**承重**。
⇒ ⇒ 新增 2 个用例（+8 断言 ⇒ 现 **11 用例 / 41 断言**）：
「先宽松建、后开严格 ⇒ 既有名字 MUST 同样受保护」（含 `default_clock` 这条**构造期**节点
的探针）·「切回宽松会清空登记，再开严格会重新播种」。

### SHOULD_FIX（已修）

| # | 问题 | 处置 |
|---|---|---|
| 1 | 用户面文档 §1.3 未告知**适用边界**：`_` 前缀是框架保留（`create_input` / `create_output` 会额外建 `"_" + name` 的 proxy）· 内部默认名（`ch_reg` 的 `reg` / mux 默认名）会让**未经显式命名**的普通设计必然报错 | ✅ `docs/usage_guide/05-component-design.md` §1.3 补「**开启时机**」+「**适用边界**」两段；`set_node_name_policy()` 的 API 注释同步写死 |
| 2 | `create_output` 在静态析构期降级时，proxy **已入表**而 output 被拒 ⇒ 留下 **stray proxy**（与 `create_input` 不对称） | ✅ 在 `create_output` 处加「**已接受的边界**」注释（**MUST NOT** 当 bug「修」出回归）；不引入清理逻辑（会新增「半删节点」失败面） |

### NIT（已记录）

- `next_node_id()` 被移到 `try` 之外 ⇒ 与旧实现的**失败路径同样消耗 id** ⇒ 语义等价（无行为变化）。
- `context.tpp` 末尾补了文件尾换行（顺手修复）。

## 验证命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
( cd build && ctest -LE perf --output-on-failure )
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate component-io-const-and-node-name-policy-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
bash scripts/verify_test_counts.sh
```
