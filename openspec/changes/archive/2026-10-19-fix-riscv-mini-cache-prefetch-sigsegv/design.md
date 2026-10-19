# Design: fix-riscv-mini-cache-prefetch-sigsegv

> 实施级设计。Scope: ICachePrefetch ch_reg 数组 → ch_mem 重写。

## 1. 修复点

`examples/riscv-mini/src/i_cache_prefetch.h`：

| Line | 旧 | 新 |
|------|----|----|
| 77 | `ch_reg<ch_uint<32>> prefetch_data[2]` | `ch_mem<ch_uint<32>, 2> prefetch_data` |
| 78 | `ch_reg<ch_uint<32>> prefetch_addr[2]` | `ch_mem<ch_uint<32>, 2> prefetch_addr` |
| 79 | `ch_reg<ch_bool> prefetch_valid[2]` | `ch_mem<ch_uint<1>, 2> prefetch_valid` |
| 82-84 | runtime for-loop init | 删除（ch_mem 默认 0） |

后续访问语义保持一致：
- read: `prefetch_data.sread(idx).idx()` 返回 ch_uint<32>
- write: `prefetch_data.write(idx, value, enable)` —— 1-cycle registered

## 2. test_cache_smoke.cpp 改动

`examples/riscv-mini/tests/test_cache_smoke.cpp:38-45`：

```cpp
TEST_CASE("ICachePrefetch instantiation (FIXED via ch_mem)",
          "[riscv-mini][cache][smoke][fixed]") {
    ch::core::context ctx("icache_prefetch_smoke");
    ch::core::ctx_swap swap(&ctx);
    ch::ch_device<ICachePrefetch> dev;
    REQUIRE(dev.context() != nullptr);
    SUCCEED("ICachePrefetch instantiation OK after ch_mem refactor");
}
```

`examples/riscv-mini/tests/test_cache_smoke.cpp:47-53`：
- `DCacheWriteBuffer instantiation` review 后发现**没有 SIGSEGV bug**（结构是 struct 数组非 ch_reg 数组）
- 但 test 当前 SKIP，保留原 SKIP（保守），加注释解释 review 结论

## 3. spec 增量

`openspec/changes/fix-riscv-mini-cache-prefetch-sigsegv/specs/cpu-cache-subsystem/spec.md` 新增：

```
### Requirement: riscv-mini cache wrapper MUST NOT use C-style ch_reg array

`examples/riscv-mini/src/i_cache_prefetch.h` 与 `d_cache_write_buffer.h` 不允许
使用 `ch_reg<T>[N]` C-style 数组（runtime init 触发 SIGSEGV）。
MUST 用 `ch_mem<T, N>` 替代（per ADR-039 + cache-complete-doa-note §6.1 R1）。
```

## 4. 实施 TDD 5 步

1. **改 i_cache_prefetch.h line 77-84** → ch_mem 重写
2. **编译 i_cache_prefetch.h** —— 后续访问可能需要小修（prefetch_data[i] → prefetch_data.sread(...)）
3. **改 test_cache_smoke.cpp** instantiation 改 PASS
4. **ctest 验证 riscv_mini_cache_smoke** 3 PASS（原 1 SUCCEED + 2 改 PASS）
5. **AGENTS.md / cpu AGENTS.md 同步**

## 5. 风险

| 风险 | 等级 | mitigation |
|------|------|------------|
| ch_reg → ch_mem 替换时 access pattern 改动引入编译错 | 🟢 | 仅 line 77-79 直接替换 + line 82-84 删除；后续访问 line 95-105 中未直接索引 prefetch_data (只发请求) |
| pre_addr_ch_reg type 与 prefetch_addr ch_mem 后续访问冲突 | 🟢 | describe() line 145 + 后续无 prefetch_data[i] 索引调用（已是简化框架）|
| test_cache_smoke instantiation 修复后 codegen path 暴露新 bug | 🟡 | instantiation smoke 只验 nullptr；不跑实质行为 |

## 6. 反向约束

- ADR-039 ch_mem-based cache rewrite 不破（fix 与 ADR 一致）
- cache-complete-doa-note §6.1 R1 不破（按其指示用 ch_mem）
- root AGENTS.md "ANTI-PATTERNS: No stubs" → 删 SKIP stub 改真验证（1 处）

## 7. 验收

```bash
cmake --build build --target riscv_mini_cache_smoke -j$(nproc)
ctest --test-dir build -L riscv-mini -R riscv_mini_cache_smoke --output-on-failure
```