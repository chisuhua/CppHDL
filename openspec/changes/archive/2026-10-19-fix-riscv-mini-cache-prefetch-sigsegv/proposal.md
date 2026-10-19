# Change: fix-riscv-mini-cache-prefetch-sigsegv — ICachePrefetch ch_reg array → ch_mem

## Why

**现状**（截至 commit `91a0833` cache-complete-rewrite）：
- `examples/riscv-mini/src/i_cache_prefetch.h:79` 用 `ch_reg<ch_bool> prefetch_valid[PrefetchConfig::PREFETCH_BUFFER_SIZE]` = C-style ch_reg 数组
- 配套 line 82-84 runtime for-loop `prefetch_valid[i] = ch_reg<ch_bool>(ch_bool(false))` 在 Context 还未就绪时初始化 → SIGSEGV
- `examples/riscv-mini/tests/test_cache_smoke.cpp:38-45` instantiation 用 `SKIP()` 标记已知 bug（不阻塞 ctest，但 codegen 路径完全没 e2e 验证）
- 同样问题影响 `prefetch_data` 和 `prefetch_addr`（line 77-78）
- 这是 Phase 4（RV32I riscv-mini dual 化）的**前置阻塞**——per design.md §11 "blocked by `i_cache_prefetch.h` wrapper SIGSEGV"
- cache-complete-doa-note §6.1 R1 显式要求 C-style 数组架构用 ch_mem 重写

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| Phase 4 RV32I dual 化 hang | wrapper 不能 instantiate → 整个 RV32I + ICache Sync integration 被阻断 |
| `riscv_mini_cache_smoke` 2 SKIP-only test 永久占位 | 没法 ctest verify ICachePrefetch build probe 之外的实质行为 |
| `d_cache_write_buffer.h` 是否同 bug 待查 | root AGENTS.md 标注 "同根架构 bug, 未实例化验证" |

**预期收益**：

1. **Phase 4 unblock**：ICachePrefetch 能 instantiate 后，RV32I riscv-mini 真集成可走通 dual backend
2. **riscv-mini 真集成 codegen 信心**：wrapper codegen 路径有 e2e 验证
3. **`test_cache_smoke.cpp` 2 SKIP 改 2 PASS** —— test count +3（1 instantiation + 1 dual verification）

**预期成本**：

- 修改 1 header (`i_cache_prefetch.h`) + 1 test (`test_cache_smoke.cpp`)
- 改造工作量 ~0.5 人天（纯 ch_reg → ch_mem 替换 + 测试改 SKIP→PASS）
- 维护成本：0（fix 写法过 ch_mem-based，符合 ADR-039 决策）

**Out of Scope**：
- ❌ `d_cache_write_buffer.h` —— review 未发现 SIGSEGV bug，结构是普通 struct 数组非 ch_reg 数组
- ❌ Phase 4 RV32I 真集成 —— 后续 change `dual-backend-rv32i-integration` 处理
- ❌ cache-stub-removal —— 后续 Phase 5

## What Changes

### 1. `examples/riscv-mini/src/i_cache_prefetch.h` line 77-84 改写

```cpp
// 旧（line 77-84）:
ch_reg<ch_uint<32>> prefetch_data[PrefetchConfig::PREFETCH_BUFFER_SIZE];
ch_reg<ch_uint<32>> prefetch_addr[PrefetchConfig::PREFETCH_BUFFER_SIZE];
ch_reg<ch_bool> prefetch_valid[PrefetchConfig::PREFETCH_BUFFER_SIZE];

for (unsigned i = 0; i < PrefetchConfig::PREFETCH_BUFFER_SIZE; i++) {
    prefetch_valid[i] = ch_reg<ch_bool>(ch_bool(false));
}

// 新（ADR-039 ch_mem-based）:
ch_mem<ch_uint<32>, PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_data;
ch_mem<ch_uint<32>, PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_addr;
ch_mem<ch_uint<1>, PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_valid;

// 初始化改成 ch_mem::write enable=false（默认值 0）
// 删除 line 82-84 的 for-loop 初始化
```

后续访问：
- 旧: `prefetch_data[i]`
- 新: `prefetch_data.sread(ch_uint<clog2(PREFETCH_BUFFER_SIZE)>(i))` 或基于 `i` index 的 ch_mem::write

### 2. `examples/riscv-mini/tests/test_cache_smoke.cpp` 2 SKIP → PASS

```cpp
TEST_CASE("ICachePrefetch instantiation (FIXED)", "[riscv-mini][cache][smoke][fixed]") {
    ch::core::context ctx("icache_prefetch_smoke");
    ch::core::ctx_swap swap(&ctx);
    ch::ch_device<ICachePrefetch> dev;
    // 简单 instantiate smoke
    REQUIRE(dev.context() != nullptr);
}
```

### 3. spec `cpu-cache-subsystem` 增量

新增 `## ADDED Requirements`：riscv-mini wrapper 不允许 C-style ch_reg 数组。

## Acceptance

#### Scenario: ICachePrefetch instantiate 通过

- **WHEN** `cmake --build build --target riscv_mini_cache_smoke && cd build && ./examples/riscv-mini/riscv_mini_cache_smoke "[fixed]"`
- **THEN** instantiation 通过，0 SIGSEGV

#### Scenario: test_cache_smoke.cpp 2 SKIP → 2 PASS

- **WHEN** `ctest -L riscv-mini -R riscv_mini_cache_smoke --output-on-failure`
- **THEN** 3 tests 通过（原 1 SUCCEED + 2 改 PASS）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 errors