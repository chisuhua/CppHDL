---
name: cache-stub-physical-removal.research
priority: P0
source: cache-stub-physical-removal.md 的配套调研备忘录
phase: phase-5
category: cache
type: research
created_at: 2026-10-18
---

# Cache Stub 物理删除 — 调研备忘录

**日期**: 2026-10-18
**作者**: build-agent (architect role)
**父 improvement**: `cache-stub-physical-removal.md`（同目录）

---

## A1. 调用面 grep (2026-10-18)

### A1.1 stub 头文件自身
```
$ grep -rln "i_cache\.h\|d_cache\.h\|i_cache_stub\|d_cache_stub\|chlib::ICache\|chlib::DCache" \
    --include="*.cpp" --include="*.h" 2>/dev/null
include/cpu/cache/d_cache_stub.h
include/cpu/cache/i_cache_stub.h
tests/chlib/test_cache_pipeline_integration.cpp
tests/chlib/test_cache_simple.cpp
tests/chlib/test_phase4_integration.cpp
```

→ 5 个匹配 = **2 个 stub 自身 + 3 个测试文件**。**0 个生产代码调用方**。

### A1.2 riscv-mini 实际消费
```
$ grep -n "ICache\|DCache\|i_cache\|d_cache" examples/riscv-mini/src/*.h
examples/riscv-mini/src/d_cache_write_buffer.h:29:#include "d_cache_complete.h"
examples/riscv-mini/src/i_cache_prefetch.h:23:#include "i_cache_complete.h"
```

→ riscv-mini **只用 `_complete.h`**，封装在 `DCacheWriteBuffer` / `ICachePrefetch` 两个 wrapper 里。

---

## A2. Stub vs Complete 语义差异

| 行为 | stub (`*_stub.h`) | complete (`*_complete.h`) |
|------|-------------------|---------------------------|
| `ready` | `:= req` (立即 ready) | `:= hit ? true : axi_ready` |
| `miss` | `:= false` (恒为 0) | `:= !hit`（首次访问 = 1）|
| `data` / `rdata` | `:= axi_data` (直通) | `:= hit ? cache_data : axi_data` |
| SRAM 读写 | 不写（初始化为 0 后死）| 真实读写 + LRU 更新 |
| AXI 触发 | `axi_valid := req` (恒发起) | `axi_valid := miss && req` (仅 miss) |
| Miss 填充 | 无 | `axi_resp_valid && miss` 时 fill 选 LRU victim way |
| 适合场景 | smoke test / 早期 pipeline debug | 真实 SoC 集成 |

---

## A3. 3 个测试文件逐行分析

### A3.1 `test_cache_simple.cpp` (35 行)

```cpp
// L21-26
TEST_CASE("ICache Type Check", "[cache][icache]") {
    ch::core::context ctx("icache_test");
    ch::core::ctx_swap swap(&ctx);
    ICache icache{nullptr, "icache"};
    REQUIRE(true);   // ← tautology
}
```

**分析**: 这是**纯 instantiate smoke**，没断言 stub 任何特定行为。改写极简 — 把 `ICache` 换成 `ICacheComplete`，断言保持 `REQUIRE(true)`。**这是 stub 行为到 complete 行为的最简单迁移**。

### A3.2 `test_cache_pipeline_integration.cpp` (172 行)

| Line | test_case | 状态 | 关键断言 |
|------|-----------|------|----------|
| L30-43 | `Type Check` | active | `STATIC_REQUIRE(is_base_of_v)` + `REQUIRE(true)` |
| L45-72 | `I-Cache Integration` | SKIP after L71 | `ready==1, miss==0, data==0xDEADBEEF, axi_addr==0x1000, axi_valid==1` |
| L74-104 | `D-Cache Integration` | SKIP after L103 | `ready==1, miss==0, rdata==0x12345678, axi_*=相应写值` |
| L106-120 | `Dual Cache Access` | SKIP | 只检查 context 隔离 |
| L122-170 | `AXI Interface` | SKIP after L168 | 2 周期 `axi_valid` 持续挂起、然后 `data==0xABCD0001` |

**断言的语义要求**:
- `miss==0`（首次访问即 hit）→ **只有 stub 满足**
- `data==axi_data`（不经 SRAM）→ **只有 stub 满足**

**改写策略**:

#### A3.2.1 Type Check（L30-43）—— 改 `_complete.h`
```cpp
STATIC_REQUIRE(std::is_base_of_v<ch::Component, ICacheComplete>);
STATIC_REQUIRE(std::is_base_of_v<ch::Component, DCacheComplete));
```

#### A3.2.2 I-Cache Integration（L45-72）—— 改语义为 fill-then-read
```cpp
// 周期 1: 触发 miss fill
sim.set_input_value(icache.io().addr, 0x1000u);
sim.set_input_value(icache.io().req,  1u);
sim.set_input_value(icache.io().axi_data,       0xDEADBEEFu);
sim.set_input_value(icache.io().axi_resp_valid, 1u);
sim.set_input_value(icache.io().axi_ready,      1u);
sim.tick();

// 周期 2: 同一地址再读，应 hit
sim.set_input_value(icache.io().req, 1u);
sim.set_input_value(icache.io().axi_resp_valid, 0u);  // 关 AXI 应答
sim.tick();
REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss)) == 0u);
// data 现在应来自 cache_data(63,32) — 注意: 完整填回 axi_data,
// data_sram[index][victim_way] = ch_uint<DATA_WIDTH>(axi_data) 后
// cache_data(63,32) = axi_data 的高 32 位
// 假设 0xDEADBEEF 的高 32 位是 0 (因 DATA_WIDTH=128, axi_data 是 32 位会被零扩展)
// → 期望 cache_data(63,32) = 0
```

⚠️ **真实行为断言需要核对 DATA_WIDTH=128 时 axi_data 32 位零扩展成 128 位的 [31:0]=0xDEADBEEF, [127:32]=0**。这部分需 builder 跑测试确认。

#### A3.2.3 D-Cache Integration（L74-104）—— 同上策略
#### A3.2.4 Dual / AXI —— 保持 SKIP，理由不变

### A3.3 `test_phase4_integration.cpp` (170 行)

7 个 test_case 全部 instantiate + `REQUIRE(true)` + SKIP。改写极简 — 4 个用 `ICache`/`DCache` 的地方换成 `ICacheComplete`/`DCacheComplete`。

---

## A4. ADR-037 §4 默认路径 (a) vs 建议路径 (b)

### A4.1 ADR-037 §4.1 (BYPASS mode) 不可取

**问题 1 — 架构污染**

`ICacheConfig` 当前是纯数字常量:
```cpp
struct ICacheConfig {
    static constexpr unsigned CAPACITY = 4096;
    static constexpr unsigned ASSOCIATIVITY = 2;
    static constexpr unsigned LINE_SIZE = 16;
    // ...
};
```

加 `bool bypass` 等于把 "runtime mode" 塞进 "static geometry"，破坏 config 语义分离。**正确做法**应该是把 `ICacheConfig` 改成 `template <bool Bypass>` 或加 `ICacheMode` enum（ADR-037 §6 已标记为 follow-up，但本 change 不该承担）。

**问题 2 — bypass 跨 Hit/Miss 状态机**

`i_cache_complete.h` L130-194 的 hit/miss/LRU/AXI 逻辑全部是相互依赖的:
```cpp
ch_bool hit = hit_way0 || hit_way1;
ch_bool miss = !hit;
io().data <<= select(hit, cache_data(63,32), io().axi_data);
io().ready <<= hit || io().axi_ready;
io().miss <<= miss;
io().axi_valid <<= miss && io().req;
// ...
if (miss && io().axi_resp_valid) { /* fill */ }
```

要实现 `ready:=req, miss:=0, data:=axi_data`，每条 `io().xxx <<=` 都要 `if (BYPASS) ... else ...`，**纯加复杂度，零新功能价值**（AXI 协议自己就是直通，何必中间加一层 cache mock？）

**问题 3 — 新测试本身要重新覆盖 stub 语义**

ADR-037 §4.1 step 1 写的 `test_cache_complete_bypass.cpp` 干的事就是「断言 stub 行为」—— 本质是把 stub 的 assert 搬过去，没省事。

**问题 4 — 违反 ZERO-DEBT POLICY**

"don't add 'improvement' / 'for future use' code" 是 root AGENTS.md 原则。`BYPASS` mode 在 stub 删除后**没有用户**（grep 验证 0 production 调用），是 dead code 等着被删。

### A4.2 建议路径 (变体 b 强化版)

直接把 3 个测试文件**改用 `_complete.h`**（不增加新概念）：
- `test_cache_simple.cpp` — 纯 smoke，0 改造成本
- `test_cache_pipeline_integration.cpp` — 改 2 个 active test_case 的断言语义为 fill-then-read（**真实测 cache**）
- `test_phase4_integration.cpp` — 纯 instantiate 改名

**净结果**:
- 删 2 个 stub header（-200 行）
- 删 3 个 pragma wrapper（-30 行）
- 0 个 `_complete.h` 改动（0 行）
- 测试现在测的是 **真实 cache**（不是 stub 测 stub 的 tautology）

---

## A5. OpenSpec change 包结构建议

```
openspec/changes/cache-stub-physical-removal/
├── proposal.md
├── design.md
├── tasks.md
└── specs/
    └── cpu-cache-subsystem/
        └── spec.md   # 1 个新 requirement (header MUST NOT contain *_stub)
```

### proposal.md 草稿要点
- Why: ADR-037 6-month deadline 提前 6 个月落地 (0 production callers, no migration window)
- What: git rm 2 headers + rewrite 3 tests + sync 3 docs
- Impact: -350 lines, 0 public API change

### tasks.md 草稿要点 (TDD 5-step per step)
1. **Step 1**: 重写 `test_cache_simple.cpp` → 验证 build + ctest -R test_cache_simple 通过
2. **Step 2**: 重写 `test_phase4_integration.cpp` → 验证 build + ctest -R test_phase4_chlib 通过
3. **Step 3**: 重写 `test_cache_pipeline_integration.cpp` 5 个 test_case → 验证 build + ctest -R test_cache_pipeline 通过（这一步最复杂）
4. **Step 4**: (可选) 新增 `test_cache_complete_bypass.cpp` → 验证 ctest 通过
5. **Step 5**: `git rm` 2 个 stub header → 验证 build + ctest -L base 全绿
6. **Step 6**: 文档同步 (3 个文件) → grep 验证 + ci 跑过
7. **Step 7**: openspec archive + ADR-037 §5 status updated

---

## A6. 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| `_complete.h` 的某时序/状态机不可被 Catch2 单独模拟 | 🟡 中 | 中 | 改 `SKIP()` 保留 test 编号，不删 test |
| `examples/riscv-mini` 编译失败 | 🟢 低 | 高 | 改完测试先 build riscv-mini targets 再删 stub |
| AXI_DATA 32-bit 零扩展成 128-bit 行为与预期不符 | 🟡 中 | 中 | step 3 跑 ctest 验证，失败就调整断言（保留 0/1 数字判断而非精确值）|
| ADR-037 偏离自身 6-month deadline | 🟢 低 | 低 | ADR-037 §5 Status 加 "executed 2026-10-18, 6 months ahead" note |
| 失去 pass-through smoke 覆盖 | 🟡 中 | 低 | step 4 可选加 `test_cache_complete_bypass.cpp` 兜底 |

---

## A7. 不在范围内的项目 (再确认)

- ❌ 不实现 `BYPASS` mode 到 `_complete.h`
- ❌ 不实现 `test_cache_complete_lru.cpp`（follow-up per ADR-037 §6）
- ❌ 不删 `examples/riscv-mini/src/{i_cache_prefetch,d_cache_write_buffer}.h`（封装 `_complete.h`）
- ❌ 不改 `ICacheConfig` / `DCacheConfig` 数字
- ❌ 不动 JIT/Verilator/backend（无相关依赖）
