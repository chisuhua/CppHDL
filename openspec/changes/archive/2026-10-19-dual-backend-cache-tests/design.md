# Design: dual-backend-cache-tests

> 实施级设计。Scope: 新建 ICacheSync/DCacheSync runtime test + 双后端 dual 化。

## 1. 文件结构

```
openspec/changes/dual-backend-cache-tests/
├── proposal.md               # 已写
├── design.md                 # 本文件
├── tasks.md                  # builder 填
└── specs/
    └── dual-backend-test-coverage/
        └── spec.md           # ADDED Requirements 段(与 verilator-parity-all-tests 一致)
tests/
└── chlib/
    └── test_cache_sync.cpp   # 新建: 9 个 DUAL_BACKEND_TEST_PAIR
tests/CMakeLists.txt
└── add_dual_backend_test(test_cache_sync SOURCES test_cache_sync.cpp)
```

## 2. timing 契约推导（ICacheSync 1-cycle sread）

`include/cpu/cache/i_cache_sync.h:60-62` 文档契约：

```
cycle N:    请求提交 + sread 采样 addr
cycle N+1:  sread 输出寄存器 = mem[addr_N], miss-fill 触发 → ch_mem::write 完成
cycle N+2:  hit/miss 决策生效 → data 输出（命中命中 cache_data;miss 命中 AXI path）
```

总延迟 = **2 cycle** (req 提交到 data 输出) per ICacheSync。

但这个延迟依赖于 latch 边界（verilator `always_ff @(posedge clk)`）。不同测试需要分类讨论：

### 2.1 Hit 后返回值

```cycle 0:  req=1, addr=X (cache 已被 fill with X by axi_resp in earlier cycle)
       data 是哪个 cycle 的值?
```

仔细看 i_cache_sync.h 第 60-62 注释：cycle N **提交 addr 后**，**cycle N+2** 才有 valid data。但 hit 决策需要 valid 位，valid 位是从 valid_mem.sread 出来——sread 是 1-cycle registered。

**实际周期细分**：

| cycle | signal action | 物理 |
|-------|--------------|------|
| 0 | req=1, addr=X, tag=tagX | inputs latched |
| 1 | tag_mem.sread 取到 tagX_at_cycle0; valid_mem.sread 取到 valid_at_cycle0; 评估 hit/miss | sread out → eval |
| 2 | data_mem.sread 输出 data0_at_cycle1; ch_mem::write(do_fill) 完成 | sread out → data + fill latch |
| 3 | ready/miss decision 生效 | output registered |

实际从 req 提交到 ready=1（hit case）= **3 cycle**。

### 2.2 Hit 后读 cached data 的 latency

`io().data <<= select(hit, bits<31, 0>(cache_data), io().axi_data);`

`cache_data` 是 `data_mem_way*.sread(index)` 的 registered output。 `sread` 输出对应 cycle **N+1** 的 sread 结果（采样 cycle N 的 addr）。

所以 cycle 0 提交 req，**cycle 2** data 是 cache_data[address=cycle0]。**ready 信号（hit case）也是 cycle 2**。

### 2.3 设计测试时序

**ICacheSync Hit-after-fill 测试时序（目标 hit latency 约 2 cycle total）**：

```
cycle 0: req=0                              (初始化)
cycle 1: req=1, addr=0x1000                 (cold req)
   → data = X (X-implementation-defined = AXI fallback)
   → miss = 1, axi_valid = 1
   → ready = axi_ready

cycle 2: req=0, axi_resp_valid=1, axi_data=0xDEADBEEF, axi_ready=1
   → cache fill: do_fill=1 if axi_resp_valid & miss (registered from cycle 1)
   → cache_data output from data_mem.sread = data at cycle_1 indexed addr (initial mem = 0)

cycle 3: req=1, addr=0x1000                 (refilled req)
   → sread tag_mem → tag=0x1000_tag (filled in cycle 2)
   → sread valid_mem → valid=1 (filled in cycle 2)
   → hit=1, miss=0
   → data = ?  这里需要看 sread 的 registered vs combinational
```

**真实时序需要实测**。第一次写测试先按"`req=1; sim.tick(); 验证 miss=1`" 把单 cycle 行为摸清楚，逐步扩展到 2-cycle / 3-cycle 验证。

### 2.4 测试策略

**保守策略**：每个 TEST_CASE 跑足够 cycle 验证 + 用 `sim.tick(N)` 而不是单 cycle。每 cycle 后 `REQUIRE` 不变的输出（如 `miss` 下降沿）当事件触发器。

**避免 fragile timing**：测试**不**断言"cycle N data 一定是 X"这种"硬时序"，而断言"cycle M data 命中 cache 后 N cycle 必然 = cached value"。给定的 M 和 N 会按 verilator-parity-all-tests R12 修订成 parity 形式。

### 2.5 Async variant 排除理由

- `ICacheAsync` 是 0-cycle aread，对接 `verilator-e2e-coverage` spec §R1 invariant 3 的"imem 必须 sread"约定
- `dual-backend-test-coverage` spec R6 #3（"aread-based 设计"）显式排除
- 0-cycle aread 在 simulator 是 posedge-sampled，在 verilator 是 combinational，必然 1-cycle skew，破坏 parity

→ Async variant 留待 `cache-stub-physical-removal` Phase 5 时做 golden-value 测试（手写 verilator half，参照 `test_verilator_trace_parity.cpp` 的 pattern）。

## 3. test_cache_sync.cpp 设计

### 3.1 文件头

```cpp
// tests/chlib/test_cache_sync.cpp
//
// ADR-041 Sync/Async variant split — runtime test for ICacheSync / DCacheSync.
// Hits / miss-fill / refilled-hit / write-through paths, driven through
// InterpreterBackend + VerilatorBackend via DUAL_BACKEND_TEST_PAIR.

#pragma once

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "cpu/cache/i_cache_sync.h"
#include "cpu/cache/d_cache_sync.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace chlib;
```

### 3.2 ICacheSync fixtures (5 TEST_CASE_PAIRs)

每个 fixture 暴露一个对 `set_input_value` 接口合理的 minimal API；测试用例关注**测试现象**（行为契约）而不是**实现细节**（ch_mem 内部 wire）。

```cpp
// ICacheSync: Cold miss → AXI request
DUAL_BACKEND_TEST_PAIR(
    ICacheSync,
    "ICacheSync - Cold miss generates AXI request",
    "[cache][sync][icache][cold-miss]",
    [&]() {
        // cycle 1: cold read
        sim.set_input_value(dev.instance().io().addr, 0x1000_d);
        sim.set_input_value(dev.instance().io().req, 1_d);
        sim.tick();

        // Per ADR-035 64-bit cap: all ports ≤64 bits, dual backend OK
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().axi_addr)) == 0x1000_d);
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().miss)) == 1u);
    });
```

(其余 8 个 TEST_CASE 在 src/test_cache_sync.cpp 中实现)

### 3.3 macro 行为契约

`DUAL_BACKEND_TEST_PAIR` macro:
- 注入 `dev` (ch::ch_device<ICacheSync>&) + `sim` (ch::Simulator&) 
- interpreter half: `set_jit_enabled(false)` + `set_backend(InterpreterBackend)`
- verilator half: 工具可用时跑，否则 WARN-skip
- >64-bit port: macro R5 字段检查 + WARN-skip（ICacheSync port 远小于 64-bit，不会触发）

### 3.4 第三方依赖

**没有**。`ICacheSync` / `DCacheSync` 在 `include/cpu/cache/` 已实装；runtime test 只读 `set_input_value` / `get_port_value` 接口（与现有 `test_cache_pipeline_integration.cpp` 一致）。

## 4. CMakeLists.txt 注册

```cmake
add_dual_backend_test(test_cache_sync
    SOURCES test_cache_sync.cpp
    TIMEOUT 60   # 默认 60s; Sync variant codegen 较小
)
```

放到 `tests/chlib/test_cache_pipeline_integration.cpp` 后面，与 `test_phase4_integration.cpp` 并列区段（cache 主题）。

## 5. 测试文件实施 TDD 5 步

1. **写 1 个 ICacheSync 简单 cold-miss 测试**（interpreter 单跑） → 摸 timing
2. **把第一个测试套 DUAL_BACKEND_TEST_PAIR macro**（interpreter + verilator 双跑） → 摸 macro 边界
3. **加 4 个 ICacheSync 剩余测试** + **4 个 DCacheSync 测试** → 全场景覆盖
4. **编译 + ctest 验证**（interpreter + verilator 双后端 PASS）
5. **AGENTS.md / spec 同步**

每步独立 commit。

## 6. 风险与 mitigation

| 风险 | 等级 | mitigation |
|---|---|---|
| ICacheSync 实际 hit/miss 时序比注释描述复杂 | 🟡 | §2 保守策略 + 实测延迟先于断言；如时序变，先调对齐再升 assertion 强度 |
| R5 width check 误判（ICacheSync DATA_WIDTH=128）| 🟢 | DATA_WIDTH 是内部 SRAM，**顶层 ch_out 只暴露 ch_uint<32>** （per header review）。macro R5 检查的是顶层 ch_out，内部 sram 走 `sread().impl()` 不算顶层 port |
| verilator cold compile 60s budget 不够 | 🟢 | Sync variant fixture < `CounterFixture<32>`，参考 test_dual_backend_template 实测 0.11s |
| Async variant 用户期待 dual 化 | 🟢 | proposal 显式划线 + spec R6 引用 |
| `ChCache` 端口 ready 在 cycle N 还是 N+1 | 🟡 | §3.4 内第一个测试**先实测**，不预设；如 fail，先扩 cycle 数 |

## 7. 反向约束

- **`verilator-e2e-coverage` spec**：不破 9 个 reference impl 的 file-level contract（不动 test_verilator_*）
- **`dual-backend-test-coverage` spec R6**：本 design 严格遵守（只做 Sync variant，Async 留 Phase 5）
- **ADR-041**：cache Sync/Async variant split 不破，runtime test 与 variant 一一对应
- **ADR-039**：ch_mem-based SRAM 是 runtime test 的物理底座；本 test 不绕过 ch_mem

## 8. 验收命令清单

```bash
# 编译
cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"   # 0 errors

# Spec 验证
openspec validate dual-backend-cache-tests --strict            # 通过
openspec validate --specs --strict                              # 14/15 通过（+1 capability）

# 测试运行
ctest -LE perf --output-on-failure                              # 不退化
ctest -L verilator_e2e -LE perf -R test_cache_sync --output-on-failure   # test_cache_sync PASS

# 计数
cd build && ctest -N -L verilator_e2e | tail -1                  # 实测 verilator_e2e 数（+1）
bash scripts/verify_test_counts.sh                              # exit 0
```
