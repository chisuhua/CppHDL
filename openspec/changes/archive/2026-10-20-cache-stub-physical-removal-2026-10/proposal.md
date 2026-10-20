# Change: cache-stub-physical-removal-2026-10 — 执行 ADR-037 cache stub 物理删除（variant b：不做 BYPASS mode）

## Why

### 现状

`include/cpu/cache/{i,d}_cache_stub.h` 是 ADR-037 标记的 pass-through smoke fixture
（类名 `chlib::ICache` / `chlib::DCache`）：

```
ready := req, miss := false, data := axi_data
axi_addr := addr, axi_valid := req      # DCache 另有 axi_wdata := wdata, axi_we := we
```

按 ADR-037 §3 的 6 个月 deadline 是 **2027-04-17**，但以下 blocker 已解除，**可提前执行**：

1. **dual-backend-test-coverage Phase 2 已落地**（commit `aed33be`，archive
   `2026-10-19-dual-backend-cache-tests/`）：`tests/chlib/test_cache_sync.cpp` 用
   `ICacheSync` / `DCacheSync` 完整 dual 覆盖（cold-miss / fill / refilled-hit /
   cross-set-miss / ready-mux 等，双后端 interpreter + Verilator）。
2. **fix-codegen-ch-reg-no-next-hold-state**（commit `7d69f56`）保证 cache init value
   在 VerilatorBackend 应用。
3. **fix-riscv-mini-cache-prefetch-sigsegv**（commit `b987bb7`）保证 riscv-mini wrapper
   instantiate 成功，已切到 `ICacheSync` / `DCacheSync`。

### 真实代价（不退化）

| 历史痛点 | 暴露点 |
|---|---|
| `test_cache_pipeline_integration.cpp` 只测 stub 自身 | 5 TEST_CASE 全部断言 stub 的 pass-through 语义 —— "stub 测 stub" = tautology（上游 improvement §5.3） |
| `test_cache_simple.cpp` / `test_phase4_integration.cpp` 仅 smoke instantiate | 2 / 7 TEST_CASE 全 `REQUIRE(true)`，但同时是 HazardUnit / cache 共存实例化的 smoke 锚点 |
| `*_stub.h` 持续被 deprecated 标注 | 根 AGENTS.md §KNOWN LIMITATIONS 长期 debt 占位 |
| Stub 用户误用：以为 stub = 真实 cache | 缺明确术语区分（`ready := req, miss := false` ≠ 真实 Hit/Miss） |

### 预期收益

1. **cache-complete 重构闭环**：stub 物理删除，`include/cpu/cache/` 只剩
   `{i,d}_cache_{sync,async,complete}.h` 4 个真实现 + 2 个兼容 shim。
2. **根 AGENTS.md §KNOWN LIMITATIONS 减 1 项**：cache stub 段删除。
3. **3 个 test 文件去掉 deprecated-suppress**：不再有 pragma 包裹。
4. **stub 的 pass-through 断言改为真实 cache 语义断言**（fill-then-hit），
   覆盖从 tautology 变成有效回归。

### 预期成本

- 修改 8 文件：3 个 test 文件（改写）+ 2 个 stub 头（`git rm`）+ 2 份 AGENTS.md（doc sync）+ ADR-037（追加 note）
- ~1 人天（per ADR-037 §4 的 3 个 atomic commit 结构，每个 commit 独立可构建）

---

## 关键决策：否决 variant (a)（BYPASS mode），采用 variant (b)

**用户决议（2026-10-20）：variant (b) —— 不做 BYPASS mode。**

ADR-037 §2 原本给出两条路径，(a) 加 BYPASS mode、(b) 直接删 stub 并把测试迁移到
真实 cache。经复核，(a) 在本仓库当前形态下**不可行**，且与上游 improvement 的
architect 决策相反：

| 否决理由 | 证据 |
|---|---|
| **实现落点不存在**：`i_cache_complete.h` / `d_cache_complete.h` 现在只是 `using ICacheComplete = ICacheSync;` 别名（ADR-041 shim），文件内无 `describe()`、无实现 | `include/cpu/cache/i_cache_complete.h:24` |
| **config 通道不存在**：`ICacheConfig` / `DCacheConfig` 是纯 `static constexpr` 常量 struct（无实例字段），`ICacheSync` ctor 只有 `(parent, name)`，全库无 component 级运行时 config 惯例；`ch_module` 不转发 ctor 参数（只有 `ch_device<T, Args...>` 转发） | `include/cpu/cache/i_cache_sync.h:43-52, 87-89`；`include/device.h:8-24` |
| **会破坏 Sync 变体的 API 面**：bypass 只能落进 `*_sync.h`，而现行 spec 把 `ICacheComplete` 定义为 `ICacheSync` 的 legacy alias；加模板参数或 config 成员都要改这条 spec 契约 | `openspec/specs/cpu-cache-subsystem/spec.md:188` |
| **stub 语义不构成合法 cache 行为**：`ready := req` 无视 `axi_ready`（吞 backpressure）、`data := axi_data` 无视 `axi_resp_valid`（把残留数据当有效数据）、`axi_addr` 未 block 对齐、`miss` 恒 false | `include/cpu/cache/i_cache_stub.h:143-147`；`include/cpu/cache/d_cache_stub.h:149-155`；对比 `i_cache_sync.h:165, 195-197` |
| **上游 improvement 明确不推荐** | `.rddf/improvements/cache-stub-physical-removal.md:151`（§5.4 "不新增 BYPASS mode"）、`:165`（决策 #2 architect 推荐"不做"） |

ADR-037 §2 的 fallback 条款 "If (1) turns out to be infeasible ... fall back to
variant (b)" 由此成立。

---

## What Changes

### 1. 重写 `tests/chlib/test_cache_pipeline_integration.cpp`（5 个 TEST_CASE 全部）

- `#include "cpu/cache/{i,d}_cache_stub.h"` → `#include "cpu/cache/{i,d}_cache_sync.h"`
- `ch::ch_device<ICache>` → `ch::ch_device<ICacheSync>`；`ch::ch_device<DCache>` → `ch::ch_device<DCacheSync>`
- 删 `#pragma GCC diagnostic push/ignored "-Wdeprecated-declarations"/pop` 包裹
- 断言从 stub 语义改为**真实 cache 时序**（端口名完全兼容，改写是机械的）：

  | TEST_CASE | 新断言（`ICacheSync` / `DCacheSync` 真实语义） |
  |---|---|
  | Type Check | `STATIC_REQUIRE(is_base_of_v<ch::Component, ICacheSync>)` 等 |
  | I-Cache Integration | cycle1 冷访问：`miss==1`、`axi_valid==1`、`axi_addr==0x1000`；cycle2 `axi_resp_valid=1` + `axi_data=0xDEADBEEF` 填充；cycle3 同址再访问：`miss==0`、`data==0xDEADBEEF` |
  | D-Cache Integration | 冷读：`miss==1`、`axi_valid==1`、`axi_addr==0x2000`；写通道：`axi_wdata==wdata`、`axi_we==1`；填充后再读 `rdata==axi_rdata` |
  | Dual Cache Access | 保持双 device 独立 context 断言（不改语义） |
  | AXI Interface | 改真实握手语义：miss 时 `ready==axi_ready`（`axi_ready=0` → `ready==0`），不再断言 stub 的 `ready==1` |

  时序模板直接取自 `tests/chlib/test_cache_sync.cpp`（已 dual-backend 验证通过），
  不新造时序。

- 2 个依赖 `Rv32iPipelineWithCache` 的 SKIP 保持 SKIP（该类仍不存在）。

### 2. 改而非删 `tests/chlib/test_cache_simple.cpp` / `tests/chlib/test_phase4_integration.cpp`

- 仅把 stub 类型换成 `ICacheSync` / `DCacheSync`（端口名兼容），保留 smoke 语义
- `test_phase4_integration.cpp` 中 HazardUnit 相关的 4 个 case **原样保留**
- 删两处的 `#pragma GCC diagnostic` 包裹

> 决策：改而非删。理由：删除会连带丢失 HazardUnit / cache 共存实例化的 smoke 锚点，
> 且会改动 CTEST_TOTAL 锚点（`scripts/verify_test_counts.sh` 是 CI 护栏）。
> 代价是 3 个 test 文件继续存在 —— 但它们已不含 deprecated 引用，debt 已消除。

### 3. `git rm` stub 头文件（Atomic Commit 3）

```
git rm include/cpu/cache/i_cache_stub.h
git rm include/cpu/cache/d_cache_stub.h
```

`tests/CMakeLists.txt` **不需要改动**（3 个 test 文件都保留，仅改内容）。

### 4. Doc sync

- `include/cpu/AGENTS.md` **STRUCTURE 表**：删 `i_cache_stub.h / d_cache_stub.h` 行
- `include/cpu/AGENTS.md` **TESTS 表**：`test_phase4_chlib` 的 source 路径描述更新为
  "cache 使用 `ICacheSync` / `DCacheSync`"
- 根 `AGENTS.md` §KNOWN LIMITATIONS：删 cache stub 段
- `docs/adr/ADR-037-cache-stub-removal.md` §5 Status：**追加** dated "Executed" note
  （append-only，不改写历史 Decision 章节）

### 5. Spec 治理

- `specs/cpu-cache-subsystem/spec.md` 用 `## REMOVED Requirements` 退役现行
  "stub_stub.h deprecation is preserved"（`openspec/specs/cpu-cache-subsystem/spec.md:172-180`）
- `## ADDED Requirements` 声明 stub 物理移除 + 行为覆盖由 Sync 变体承接
- archive **不使用** `--skip-specs`（根 AGENTS.md §OPENSPEC WORKFLOW：本 change 有 spec 变更）

---

## Out of Scope（明确划线）

- ❌ `ICacheComplete` / `DCacheComplete` 的 Hit/Miss 行为修改
- ❌ `include/cpu/cache/{i,d}_cache_{sync,async}.h` 的任何修改（本 change 全部改动止于 test 层）
- ❌ 新增 BYPASS mode / 运行时 config 机制 / `make_passthrough_*` factory
- ❌ 真实 LRU 行为测试（`test_cache_complete_lru.cpp`，ADR-037 §6 已记为 follow-up）
- ❌ ADR-039 ch_mem 重写（已完成）/ ADR-041 Sync/Async 拆分（已完成）
- ❌ riscv-mini wrapper 修复（已完成于 `b987bb7`）
- ❌ 把 `test_cache_pipeline_integration.cpp` 改造成 dual-backend（`test_cache_sync.cpp`
  已提供 `ICacheSync` / `DCacheSync` 的双后端覆盖）

---

## Acceptance

#### Scenario: ctest 全量不退化

- **WHEN** `ctest -LE perf --output-on-failure`
- **THEN** **158/158 PASS**（基线 158，删除/新增的 test 可执行数为 0 → 计数不变）

#### Scenario: compile 无错无新警告

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误
- **AND** 3 个改写的 test 文件 0 个 `-Wdeprecated-declarations` 警告

#### Scenario: stub 头文件物理不存在

- **WHEN** `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 空（0 stubs）

#### Scenario: 代码树中无 stub 引用

- **WHEN** `grep -rn "#include.*_cache_stub\.h" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
- **AND** `grep -rn "chlib::ICache\b\|chlib::DCache\b" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
- **THEN** 两者均为 0 行（`ICacheSync` / `ICacheAsync` / `DCacheSync` / `DCacheAsync` 不被误伤）

#### Scenario: 活文档中无 stub 引用

- **WHEN** `grep -rn "cache_stub" AGENTS.md include/cpu/AGENTS.md`
- **THEN** 0 行
- **AND** `docs/adr/ADR-037-cache-stub-removal.md` 保留历史记录，但 §5 Status MUST 含
  `Executed` 标记（历史 ADR 用状态标记断言，不做 absence 断言）
- **AND** `openspec/specs/` 不在本检查范围内（Requirement 按设计需指名被删路径）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `include/cpu/cache/{i,d}_cache_{sync,async}.h`
- **AND** MUST NOT 改 `include/cpu/cache/{i,d}_cache_complete.h`（shim 别名不变）
- **AND** MUST NOT 改 `tests/chlib/test_cache_sync.cpp`（dual-backend Phase 2 已 archive）
- **AND** MUST NOT 改 `examples/riscv-mini/src/{i_cache_prefetch.h,d_cache_write_buffer.h}`

#### Scenario: 测试计数锚点一致

- **WHEN** `bash scripts/verify_test_counts.sh`
- **THEN** exit 0
- **AND** 根 AGENTS.md `CTEST_TOTAL: 169` 与 `ctest -N` 实测一致（不变）
- **AND** `tests/chlib/test_*.cpp` 文件数保持 29（改而非删）

#### Scenario: 双后端不退化

- **WHEN** `ctest -L verilator_e2e --output-on-failure`
- **THEN** 全部 PASS（`test_cache_sync` 不受本 change 影响）

#### Scenario: openspec 校验通过

- **WHEN** `openspec validate cache-stub-physical-removal-2026-10 --strict`
- **THEN** valid
- **AND** `openspec validate --specs --strict` 通过（archive 合并后无互斥 Requirement）

---

## Capabilities

### REMOVED Requirements

- `stub_stub.h deprecation is preserved` —— 见 `specs/cpu-cache-subsystem/spec.md`

### ADDED Requirements

- `cache stub 物理删除且行为覆盖由 Sync 变体承接` —— 见 `specs/cpu-cache-subsystem/spec.md`
