# Capability: cpu-cache-subsystem

## Purpose

定义 CppHDL cache 子系统的契约（详见主 spec `openspec/specs/cpu-cache-subsystem/spec.md`）。

## REMOVED Requirements

### Requirement: stub_stub.h deprecation is preserved

**Reason**: ADR-037 的 deprecation 阶段已结束，pass-through stub 由本 change 物理删除
（`git rm include/cpu/cache/{i,d}_cache_stub.h`）。stub 的语义（`ready := req`、
`miss := false`、`data := axi_data`、未 block 对齐的 `axi_addr`）不构成合法 cache
行为，保留在 spec 中会与真实 cache 契约冲突。

**Migration**: stub 的 3 个测试消费方（`tests/chlib/test_cache_pipeline_integration.cpp`、
`tests/chlib/test_cache_simple.cpp`、`tests/chlib/test_phase4_integration.cpp`）改用
`ICacheSync` / `DCacheSync`；断言从 pass-through 语义改为真实 cache 时序
（cold-miss → fill → refilled-hit）。生产代码调用方为 0（`grep -rn "cache_stub"
include/ src/ tests/ examples/` 为空）。

## ADDED Requirements

### Requirement: cache stub 物理移除且行为覆盖由 Sync 变体承接

`include/cpu/cache/{i,d}_cache_stub.h` MUST 从 repo 物理移除，cache 行为覆盖 MUST
由 `ICacheSync` / `DCacheSync` 变体承接（ADR-039 ch_mem 重写 + ADR-041 Sync/Async
双变体）。本能力 MUST NOT 引入运行时 config 通道或 BYPASS mode —— stub 的
pass-through 语义是测试夹具语义，随 stub 一并消失。

#### Scenario: stub 头文件物理不存在

- **WHEN** reviewer 执行 `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 输出为空（0 个 stub 头文件）

#### Scenario: 代码树无 stub 引用

- **WHEN** reviewer 执行
  `grep -rn "#include.*_cache_stub\.h" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
- **THEN** 输出为 0 行
- **AND** reviewer 执行
  `grep -rn "chlib::ICache\b\|chlib::DCache\b" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
  输出为 0 行
- **AND** `chlib::ICacheSync` / `chlib::ICacheAsync` / `chlib::DCacheSync` /
  `chlib::DCacheAsync` 引用 MUST NOT 被上述检查误伤
- **AND** 历史注释 MAY 提及被删除的 stub 路径（`tests/chlib/test_cache_sync.cpp`
  的历史说明），但该路径 MUST NOT 出现在任何 `#include` 或代码引用中

#### Scenario: cache 集成测试覆盖真实 cache 语义

- **WHEN** `ctest -R test_cache_pipeline --output-on-failure` 运行
- **THEN** 测试 MUST 实例化 `ICacheSync` / `DCacheSync`
- **AND** MUST 断言真实 cache 时序（cold miss → AXI fill → refilled hit），
  而非 stub 的 pass-through 语义
- **AND** 该测试 MUST NOT 依赖 `[[deprecated]]` 抑制（`-Wdeprecated-declarations`
  在该 TU 上为 0）

#### Scenario: 活文档无 stub 引用

- **WHEN** reviewer 执行
  `grep -rn "cache_stub" AGENTS.md include/cpu/AGENTS.md`
- **THEN** 输出为 0 行
- **AND** `openspec/specs/` 不在本检查范围内 —— 本 capability 的 Requirement
  按设计需要指名被删除的 stub 路径以保证可验证性
- **AND** `docs/adr/ADR-037-cache-stub-removal.md` SHALL 保留其历史 Decision 记录，
  但其 Status 章节 MUST 含 `Executed` 标记

#### Scenario: 既有 cache 变体契约不变

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** `include/cpu/cache/{i,d}_cache_{sync,async,complete}.h` MUST NOT 出现
  在 diff 中（`ICacheComplete` / `DCacheComplete` 继续是 `ICacheSync` /
  `DCacheSync` 的 legacy alias）
- **AND** `tests/chlib/test_cache_sync.cpp` MUST NOT 出现在 diff 中

#### Scenario: 回归不退化

- **WHEN** `ctest -LE perf --output-on-failure` 运行
- **THEN** 全部 PASS，测试可执行数与改动前一致（无 test 文件被删除）
