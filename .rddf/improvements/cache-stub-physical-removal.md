# Improvement: cache-stub 物理删除 (ADR-037 落地)

- **Date drafted**: 2026-10-18
- **Drafted by**: build-agent (architect role)
- **Source**: 用户 "选择 2"（ADR-037 cache stub 物理删除 6-month plan 提前落地）
- **Status**: 🔄 **Active — 重启流程** (2026-10-19 cache-complete-rewrite Session B 结束)
  - **cache-complete-rewrite 已完成** (commit 91a0833 + 1221819 + 49902b2 + bb286a2; openspec change 'cache-complete-rewrite' 已归档)
  - ADR-039 §2 R2 spike 闭环 → cache_complete.h 重写为 ch_mem-based (commit 91a0833)
  - ADR-041 Sync/Async variant split (commit 49902b2): `ICacheComplete` / `DCacheComplete` 改为兼容 shim (alias `ICacheSync` / `DCacheSync`)
  - Test 9 个 test_case 覆盖 miss-fill / write-through / g1 / g2 / variant equivalence (53 assertions, 全过)
  - **Prerequisite 已满足**: cache-complete-doa-note ch_mem rewrite 已落地 (与本 improvement 同根)
  - 下一步: ADR-037 §2 variant (a) "BYPASS mode" 还是 variant (b) "Slim Sync smoke + git rm stub" — 需要用户决议
  - **Dead check**: deadline 2027-04-17 (6-month from 2026-10-17 deprecation), 当前 2026-10-19 — 还有 6 个月, **不立即 git rm**, 流程重启后按 variant (a)/(b) 推进
- **Related**:
  - ADR-037 (`docs/adr/ADR-037-cache-stub-removal.md`) — 本 change 的父决策（已 2026-10-18 增补 status 修订）
  - `.rddf/improvements/cache-complete-doa-note.md`（本目录）— ch_mem rewrite 立项
  - Commit `78fa178` — `__attribute__((deprecated))` on stub classes
  - Commit `67cc739` — rename `i_cache.h` / `d_cache.h` → `*_stub.h`
  - Root AGENTS.md §KNOWN LIMITATIONS（已 2026-10-18 增补 `*_complete.h` 是 dead-on-arrival bullet）
  - include/cpu/AGENTS.md STRUCTURE row
  - 调研备忘录: `.rddf/improvements/cache-stub-physical-removal.research.md`（本目录，§3 包含 11 个 build error 详细分析）

---

## 1. Why (背景与动机)

**stub 头文件已经没有任何生产调用方**（grep 验证 0 hit，riscv-mini 只 include `_complete.h`，封装在 `ICachePrefetch` / `DCacheWriteBuffer` 里）。

**它们是 known debt**：
- root AGENTS.md §KNOWN LIMITATIONS 显式记录 "pass-through smoke fixtures, awaiting redesign"
- `__attribute__((deprecated))` 已贴（commit `78fa178`）
- 文件名已加 `_stub` 后缀（commit `67cc739`）
- 3 个测试文件 pragma-wrap 抑制警告

**ADR-037 给的 6-month deadline 是 2027-04-17**，到今天（2026-10-18）还剩 ~6 个月。但：
- 项目无 formal release cadence（每次 merge when CI green）
- 6-month deadline 解决的问题是"≥1 release cycle 不可判定" —— 这个动机已经满足（calendar deadline）
- 0 production 调用方 = 没有外部消费者等迁移窗口
- 项目处于"刚完成一批大重构 + 测试全绿"的稳定期，**没有更好的执行窗口**

**Why now**：
- 提前 6 个月落地 = deadline 风险归零（无 later reminder 失约风险）
- 减少 ~200 行 stub 源码 + ~150 行 pragma-wrapped test = 项目瘦身
- 消除 stub vs complete 的 include path 歧义（root AGENTS.md §KNOWN LIMITATIONS bullet 消失）
- 把 ADR-037 §6 "Future Work" 列表里"test_cache_complete_lru.cpp"前置为可选项（不强制，但开了窗口）

**Why not just delete the headers immediately**：
- 必须先迁移 5 个 test_case 的断言语义（它们 assert `miss==0 && data==axi_data` —— 这只能由 stub 满足）
- 必须先证明 `_complete.h` 的真实行为在测试中可断言（hit/miss 状态机 + LRU），不然就退化成"删了但没验证"

---

## 2. What Changes (变更范围)

### 2.1 头文件 (2 文件)
- `git rm` `include/cpu/cache/i_cache_stub.h`
- `git rm` `include/cpu/cache/d_cache_stub.h`

### 2.2 测试迁移 (3 文件改 + 1 文件新增)

| 文件 | 当前 | 变更后 |
|------|------|--------|
| `tests/chlib/test_cache_simple.cpp` | 2 个 `REQUIRE(true)` smoke instantiate `ICache` / `DCache` stub | 改用 `ICacheComplete` / `DCacheComplete` 同样 smoke（`STATIC_REQUIRE(is_base_of_v)` 保留）|
| `tests/chlib/test_cache_pipeline_integration.cpp` | 5 test_case (3 SKIP, 2 active 断言 stub pass-through) | 5 test_case 改用 `_complete.h`；active 3 个改断言语义为 **fill-after-reset then re-read** 路径；SKIP 2 个保留（依赖未实现的 `Rv32iPipelineWithCache`）|
| `tests/chlib/test_phase4_integration.cpp` | 7 test_case (5 SKIP, 2 active instantiate `HazardUnitComplete` + stub caches) | 改用 `ICacheComplete` / `DCacheComplete`；语义不变 |
| `tests/chlib/test_cache_complete_bypass.cpp` (新增) | 不存在 | **可选**：覆盖 `ICacheComplete` / `DCacheComplete` 的真实 Hit/Miss 状态机（fill → re-read 应 hit）|

### 2.3 CMake (1 文件)
- `tests/CMakeLists.txt` line 101/120/224：3 个 `add_catch_test()` 行保持（`test_cache_simple` / `test_cache_pipeline` / `test_phase4_chlib` 目标名不变），但 source path 不变（因为我们重写内容而非改名）
- 如果新增 `test_cache_complete_bypass.cpp` → 加 1 行 `add_catch_test(test_cache_complete_bypass chlib/test_cache_complete_bypass.cpp)`

### 2.4 文档同步 (3 文件)
- `include/cpu/AGENTS.md` STRUCTURE 节删 `i_cache_stub.h` / `d_cache_stub.h` 行
- root `AGENTS.md` §KNOWN LIMITATIONS 删 cache stub bullet
- `ADR-037` §5 Status 改为 "Executed 2026-10-18"（commit hash 留 TBD）

### 2.5 公共 API 影响
**无新增 / 无修改**（stub 是已 deprecated 的死代码，删除是 cleanup）。生产代码无任何变化（grep 已验证 0 调用方）。

---

## 3. Acceptance (验收标准)

### 3.1 编译 + 测试 (PHASE GATES 🟢)
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] `ctest -L base` 全绿（159 tests, 含所有改写后的 cache tests）
- [ ] `ctest -L chlib` 全绿（含 `test_cache_simple` / `test_cache_pipeline` / `test_phase4_chlib` / 可选 `test_cache_complete_bypass`）

### 3.2 源码清洁度
- [ ] `git ls-files include/cpu/cache/ | grep _stub` 返回空
- [ ] `grep -rn '_cache_stub\|chlib::ICache\b\|chlib::DCache\b' --include='*.cpp' --include='*.h'` 返回空
- [ ] 3 个测试文件不再有 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` 包裹（除了 pragma 本身也消失）
- [ ] root AGENTS.md NOTES §Test counts: **CTEST_TOTAL 仍为 159**（不改 test 总数 —— 重写不是删 test；如果新增 `test_cache_complete_bypass` 则 +1 = 160，需同步 NOTES）

### 3.3 文档同步
- [ ] root AGENTS.md §KNOWN LIMITATIONS 删 cache stub bullet
- [ ] root AGENTS.md NOTES §Test counts 数字与 `ctest -N | tail -1` 一致
- [ ] `include/cpu/AGENTS.md` STRUCTURE 节不再列 `_stub.h`
- [ ] ADR-037 §5 Status 标记 executed

### 3.4 行为正确性 (P1 风险)
- [ ] 改写后的 `test_cache_pipeline_integration.cpp` 断言在 `_complete.h` 上**真实通过**（不是 `REQUIRE(true)` 伪装）
- [ ] 如果某 test_case 改写后 `_complete.h` 行为不可断言（axi_resp_valid 时序问题），改为 SKIP 而非删 test（保留 test 编号）

---

## 4. Capabilities (涉及的能力维度)

| Capability | 影响 | 文档 |
|------------|------|------|
| `cpu-cache-subsystem` | stub 删除 + test 迁移 | `openspec/specs/cpu-cache-subsystem/spec.md` 新增 requirement（如果存在该 capability）|
| `chlib-test-coverage` | 3 个 test 文件改写 + 1 个新增（可选）| `openspec/specs/chlib-test-coverage/`（如存在）|

**如果 `openspec/specs/cpu-cache-subsystem/` 不存在**：在 proposal.md 标注 "no spec changes"（按 root AGENTS.md OPENSPEC §3 archive 规则）。

---

## 5. Impact (影响面分析)

### 5.1 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| 改写后 test 行为不可断言 | 🟡 中 | 改写时如果发现 `_complete.h` 的某时序/状态机不可被 Catch2 unit test 单独模拟，改 `SKIP()` 而非删 test |
| `examples/riscv-mini` 编译失败 | 🟢 低 | grep 已验证只用 `_complete.h`，但要 `cmake --build build` 跑过 `riscv-mini` targets |
| ADR-037 写了 6-month deadline 但提前落地 = 偏离 ADR 自身 | 🟡 中 | 在 ADR-037 §5 Status 加 note "executed 2026-10-18, 6 months ahead of schedule (rationale: 0 production callers, no downstream migration window)" |
| Cache 失去"pass-through smoke" 覆盖 = 真实 LRU 行为未测 | 🟡 中 | 新增 `test_cache_complete_bypass.cpp` (optional)；ADR-037 §6 已记录 "test_cache_complete_lru.cpp" 为 follow-up |

### 5.2 工作量估算
- 7 步骤（5 tests + 2 header git rm + 3 doc） = 1 builder change
- TDD 5-step per step = ~10 commits
- 工作量: **小**（纯删除 + 已知 API 重写，无新设计）

### 5.3 利益
- 删 ~350 行代码（~200 行 stub + ~150 行 pragma wrapper test）
- 消除 1 条 include path 歧义
- root AGENTS.md §KNOWN LIMITATIONS 少 1 个 bullet
- 3 个 test 真正测 `_complete.h`（现在它们其实是 stub 测 stub = tautology）

### 5.4 不在范围内
- **不**新增 `BYPASS` mode 到 `_complete.h`（architect 决策 §3.1 解释：会污染 cache config 语义 + 等价于 stub 内嵌）
- **不**实现 `test_cache_complete_lru.cpp`（真实 LRU 行为测试，标记为 follow-up per ADR-037 §6）
- **不**删 `examples/riscv-mini/src/{i_cache_prefetch,d_cache_write_buffer}.h`（它们封装 `_complete.h`，与 stub 无关）
- **不**改 `ICacheConfig` / `DCacheConfig` 数字（保持 geometry 不变）

---

## 6. 决策 checklist (architect 推荐路径)

下面是从 architect 报告里精选的决策项，**逐项 yes/no 后切 planner**：

| # | 决策 | architect 推荐 | 用户决定 |
|---|------|----------------|----------|
| 1 | 何时做 | 今天（deadline 提前 6 个月）| ? |
| 2 | 加 BYPASS mode 到 `_complete.h`？| **不做**（架构污染）| ? |
| 3 | `test_cache_pipeline_integration.cpp` 5 个 test_case | 全部改用 `_complete.h` | ? |
| 4 | `test_cache_simple.cpp` 改而非删 | 改 | ? |
| 5 | `test_phase4_integration.cpp` 改而非删 | 改 | ? |
| 6 | 新增 `test_cache_complete_bypass.cpp`？| **可选**（加 1 个 = 160 tests；不加 = 159 tests 不变）| ? |
| 7 | ADR-037 §5 Status 标记 executed | 标 | ? |
| 8 | 走 openspec change 流程？| **走**（触及 header 删除 + 公共 header 树；按 root AGENTS.md OPENSPEC §3）| ? |
| 9 | 走 rdd-workflow 完整 rdd-arch → rdd-planner → rdd-builder → rdd-verifier？ | **跳过 rdd-arch/planner**（已有 ADR-037 = 已是 arch 决策；本 change 是 ADR-037 §4 Implementation Order 执行；走 rdd-builder 即可）| ? |

---

## 7. 调研备忘录指针

详见 `.rddf/improvements/cache-stub-physical-removal.research.md`（本目录），含：
- grep 结果（0 production 调用方）
- stub vs complete 语义对比表
- 3 个测试文件逐行分析
- ADR-037 §4 路径分析 + 为什么建议不采用 BYPASS mode
- OpenSpec change 包结构建议
