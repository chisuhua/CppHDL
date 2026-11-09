# Tasks: cache wrapper 死代码清零 + 地址稳定性契约

## T1 删死数据（wrapper 实现）

- [x] **T1.1** `include/cpu/cache/d_cache_write_buffer.h`：删 `struct WriteBufferEntry` ·
      `struct WriteBufferConfig` · `buffer[4]` · `(void)buffer` ⇒ ⇒ 头注释同步
- [x] **T1.2** `include/cpu/cache/i_cache_prefetch.h`：删 `prefetch_data` · `prefetch_addr` ·
      `prefetch_valid` · `(void)` 抑制 · `struct PrefetchConfig` ⇒ ⇒ 头注释同步
- [x] **T1.3** `__io` 签名**零改动**（capability Requirement 约束）

## T2 P14 登记（公开符号移除）

- [x] **T2.1** `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章 +2 行
- [x] **T2.2** `.rddf/checks/public-api-removals.tsv` +6 行机器探针
- [x] **T2.3** `scripts/verify_public_api_visibility.sh` 的 `EXPECTED_REMOVAL_ROWS` /
      `EXPECTED_PROBES` 锚点同步
- [x] **T2.4** `docs/CHANGELOG.md` 一行（用户可见变更）

## T3 判别性探针 + 契约

- [x] **T3.1** `examples/riscv-mini/tests/test_cache_smoke.cpp` 新增用例
      「`miss` 期间改变 `addr` ⇒ `axi_addr` 跟随当拍地址」
- [x] **T3.2** capability `cpu-cache-subsystem` 的 ADDED Requirement（正/负 Scenario）

## T4 文档同步

- [x] **T4.1** `include/cpu/AGENTS.md` 债务段（① ②「已删除」· ③ 补契约 + 载体）
- [x] **T4.2** `include/cpu/AGENTS.md` 头部行数实测同步

## T5 验证

```bash
ctest -R riscv_mini_cache_smoke --output-on-failure
```
- [x] **T5.1** `ctest -LE perf` 全绿
- [x] **T5.2** `bash scripts/verify_public_api_visibility.sh` 绿
- [x] **T5.3** `bash scripts/verify_openspec_delta.sh` 绿
- [x] **T5.4** `bash scripts/verify_test_counts.sh` 绿（本 change 不新增 ctest 条目）
