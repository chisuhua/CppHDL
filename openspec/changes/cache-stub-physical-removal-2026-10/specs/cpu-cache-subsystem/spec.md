# Capability: cpu-cache-subsystem

## Purpose

定义 CppHDL cache 子系统的契约（详见主 spec `openspec/specs/cpu-cache-subsystem/spec.md`）。

## ADDED Requirements

### Requirement: cache stub 物理删除 MUST 配 BYPASS mode 行为仿真

`include/cpu/cache/{i,d}_cache_stub.h` 物理删除后，stub 的 `ready := req, miss := false, data := axi_data` 行为 MUST 通过 `ICacheConfig::bypass_mode` 在 `ICacheComplete` / `DCacheComplete` 上保留仿真。

#### Scenario: stub header 物理不存在

- **WHEN** reviewer `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 空（0 stubs）

#### Scenario: bypass mode 行为等价 stub

- **WHEN** `ICacheComplete` with `bypass_mode=true`
- **THEN** MUST emit `ready := req, miss := false, data := axi_data`
- **AND** MUST 跳过 Hit/Miss state machine

#### Scenario: bypass mode factory helper

- **WHEN** caller 调 `make_passthrough_icache()` / `make_passthrough_dcache()`
- **THEN** MUST 返回 `ICacheComplete` / `DCacheComplete` with `bypass_mode=true` pre-set

#### Scenario: 6 个月 calendar deadline per ADR-037

- **WHEN** `date +%Y-%m-%d >= 2027-04-17`
- **THEN** stub MUST 已删除

#### Scenario: doc sync

- **WHEN** reviewer 阅读 root `AGENTS.md` §KNOWN LIMITATIONS
- **THEN** MUST NOT 含 "Cache stubs i_cache_stub.h / d_cache_stub.h are pass-through" 段