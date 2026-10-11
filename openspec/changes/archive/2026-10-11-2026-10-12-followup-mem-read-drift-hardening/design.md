# Design: 2026-10-12-followup-mem-read-drift-hardening

## Context

Issue #27 在 cleanup `2026-10-11-cleanup-250198a-aread-combinational-debt`（已 archive）的执行层把 simulator↔Verilog drift **文档化**为既有 `mem-read-timing` capability 的 R4 Scenario（"Register-driven read address skew" + "Same-cycle write+read race"），并已在 `tests/test_mem_comb_read.cpp` 添加了**跨周期** write+read 测试 + 一个 named-proxy codegen pin 测试。

执行本 change 前重新审计 cleanup 产出，发现：

- **Cleanup #3 已 pin `assign` 发射**：TEST_CASE `"aread Verilog codegen emits assign (no always_ff)"`（tag `[mem][comb-read][codegen]`，`tests/test_mem_comb_read.cpp:216-277`）走 **proxy-name 行内匹配**：抓 `async_port_data_proxy` 行 → 断言同时含 `assign` + `Comb read` + 不含 `always`。这是 spec R5.first 的**功能性等价品**——契约已 fence，只是 tag 名不严丝合缝（要求的是 `[mem][aread][golden]`，给出的是 `[mem][comb-read][codegen]`）。
- **Same-cycle race fence 缺失**：现有 `"aread cross-cycle write-then-read consistency"`（tag `[mem][comb-read][sim][write-read]`，`tests/test_mem_comb_read.cpp:286-331`）是 **write at tick N、aread at tick N+1** 的跨周期用例。spec R4 Scenario "Same-cycle write+read race" 要求是**同 tick 内**写+aread——这条 fence 当前**完全缺失**。
- **branch_predictor_v2.h 5 处零文档**：与 3 个 FIFO sample 的内联 note 不一致。

故本 change 把 scope 重新圈定为：

1. **Spec R5.first（柔性）**：把 cleanup #3 引用为本契约的实现；如未来 #3 被改名/移除，须有等效 `[mem][aread][golden]` 或 `[mem][comb-read][codegen]` TEST_CASE 接力
2. **Spec R5.second（硬性）**：新增同 cycle race 测试 `[mem][aread][race]`
3. **TASK 1**：落地 R5.second 的 race 测试（实质性新工作）
4. **TASK 2**：加 5 处 `branch_predictor_v2.h` 内联 note（doc parity）
5. **TASK 1.5（可选/可弃）**：substring 级 alias `[mem][aread][golden]` 测试——仅当 R5.first 想严格按 tag 而非"引用既有 #3"对齐时；本 change **默认跳过此 task**（避免重复 fence）
6. **TASK 3**：GitHub issue #27 body 同步

### 已知事实

| 事实 | 证据 |
|------|------|
| `aread` 永远 `enable=nullptr` | `include/core/mem.h:145-147` |
| Simulator 把所有 read ports 放入 `sequential_instr_list_` | `src/simulator_init.cpp:116-121` |
| aread 的 "0-cycle 观察" 来自 tick 末尾 `eval_combinational()` | `src/simulator.cpp:328` |
| Verilog codegen 路径：`port_type==async_read` → 单 `assign` 块（cleanup 删 dead enable 后） | `src/codegen_verilog.cpp:1063-1105`（post `331e812`） |
| **Cleanup #3 用 `CombReadFixture` 的 `async_port` aread proxy**（不是 cleanup #1 用的 `CombReadFixture::async_port` 同名字，新 TEST_CASE 也可复用同一 fixture） | `tests/test_mem_comb_read.cpp:97-98, 216-277` |
| **`CombReadFixture` 的 `mem.write(addr_sync, wdata, we)` 把 waddr 与 sread addr 共享同一 `addr_sync` 输入** | `tests/test_mem_comb_read.cpp:102-105` —— race 测试要用 `addr_sync=2` 作为 waddr |
| Fixture init list 默认 `{0xA1..0x90}`，可直接对照 | `tests/test_mem_comb_read.cpp:75-78` |
| FIFO samples 无下游 `rd_data_reg`，是组合读 FIFO | `samples/fifo_example.cpp:75-76`、`samples/fifo_bundle.cpp:75-77`、`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:60-61` |
| `branch_predictor_v2.h` BHT/BTB 5 站点全部由寄存器索引驱动 | `examples/riscv-mini/src/branch_predictor_v2.h:88/103/104/108/115` |
| 既有 FIFO sample 内联 note 措辞 | `samples/fifo_example.cpp:197-200`（verbatim，复制粘贴） |
| CTest 按**可执行文件名**匹配（`test_mem_comb_read`），不感知 Catch2 `[tag]` | `tests/CMakeLists.txt:15` |

## Goals / Non-Goals

### Goals
- 在 `tests/test_mem_comb_read.cpp` 追加 1 个新 TEST_CASE `[mem][aread][race]`——断言 same-cycle write+aread 返回 pre-write 值
- 在 `examples/riscv-mini/src/branch_predictor_v2.h` 5 处 `.aread()` 旁加单行 inline note，与 3 个 FIFO sample 的现有 note **措辞完全对齐**
- 在既有 `openspec/specs/mem-read-timing/spec.md` 追加 R5（两个 ADDED Requirement：first 是柔性（引用 cleanup #3）、second 是硬性（要求新 race 测试））
- 既有 R1-R4 文字**完全不动**（仅追加）
- 不引入新依赖、不改公开 API、不影响既有 142 ctest 测试通过率、不修 cleanup #3
- Issue #27 body 同步勾选状态（M1-M4 完成）；删除冗余 M5（原 M4.b）

### Non-Goals
- **不**新增"重复" golden 测试——cleanup #3 已是 R5.first 引用源，加 alias 是可选项，本 change 默认不做
- **不**修改 `ch_mem::aread()` API（加 enable / 加 latency 等）——API 扩展须先 design review
- **不**改 `comb_read` 的 codegen 输出文本
- **不**实现 Issue #27 body 中的 M5（原 M4.b 独立 `CHANGELOG.md` 文件）——与项目既有 AGENTS.md 习惯冲突
- **不** re-introduce `rd_data_reg` 等"修复" drift-1 的下游 latch——这是用户设计意图

## Detailed Design

### TASK 1 — `[mem][aread][race]` Same-cycle write+read race fence

#### 测试骨架

复用既有 `CombReadFixture`（cleanup 后已有同名 fixture，**不**需新建）。fixture 的 `addr_sync` 在 write path 与 sread path 共享，所以 race 测试用 `addr_sync=2` 同时作为 waddr 与 sread addr——而我们关心的是 `aread` 在 `we=1` 写 `addr_sync` 同地址时的行为。

```cpp
TEST_CASE("mem_comb_read same_cycle write_plus_aread returns pre_write value",
          "[mem][aread][race]") {
    ch::core::context ctx("race_test");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<CombReadFixture> dut("race_dut");
    ch::Simulator sim(dut.context());

    // setup 一次 tick 0，让 init 状态稳定
    sim.set_input_value(dut.instance().io().we,
                        static_cast<uint64_t>(0));
    sim.set_input_value(dut.instance().io().addr_sync,
                        static_cast<uint64_t>(0x0));
    sim.set_input_value(dut.instance().io().addr_async,
                        static_cast<uint64_t>(0x0));
    sim.set_input_value(dut.instance().io().wdata,
                        static_cast<uint64_t>(0x00));
    sim.tick();  // tick 0: 不做任何操作，仅触发 eval 流程稳定

    // ===== 关键 tick: 同 tick 内 we=1 写 + aread 读同一地址 =====
    sim.set_input_value(dut.instance().io().we,
                        static_cast<uint64_t>(1));   // enable write
    sim.set_input_value(dut.instance().io().wdata,
                        static_cast<uint64_t>(0xEE)); // 写 0xEE
    sim.set_input_value(dut.instance().io().addr_sync,
                        static_cast<uint64_t>(0x2)); // waddr=2 (init=0xC3)
    sim.set_input_value(dut.instance().io().addr_async,
                        static_cast<uint64_t>(0x2)); // aread_addr=2
    sim.tick();

    // spec R4 Scenario "Same-cycle write+read race" 第三条:
    //   "Simulator aread returns pre-write value (mem[X] at tick entry)"
    auto rdata = sim.get_port_value(dut.instance().io().rdata_async);
    constexpr uint64_t kPreWriteValue = 0xC3;  // init value at addr=2
    REQUIRE(static_cast<uint64_t>(rdata) == kPreWriteValue);
}
```

#### 设计决策点

**为什么先做一次空 tick（tick 0）**？
- 验证 fixture 在 default reset 后 init memory 已稳定（init list 已经写进 `data_map_`）
- 避免把"first tick 后 state 未稳"的伪问题归到 race fence 上

**为什么不直接 `tick()` 不先做空 tick**？
- 直接 tick 也可以：tick 入口会先读 init memory，然后触发 tick 内的 eval。但加了空 tick 后断言更清晰

**fixture 复用安全吗？**
- `CombReadFixture` 已被 cleanup #1、#2、#3、#4 共用 4 次——无任何 instancing 问题
- race 测试用 `addr_sync=2` 作为 waddr——这与 sread path 共享但 sread 我们不关心（fixture sread 默认也没读）

#### Adversarial Verification（TASK 1 完成后必跑）

```bash
# 反向验证: 把测试的 expected value 改成 post-write (0xEE)
sed -i 's/kPreWriteValue = 0xC3/kPreWriteValue = 0xEE/' tests/test_mem_comb_read.cpp
cmake --build build -j$(nproc)
./build/tests/test_mem_comb_read "[mem][aread][race]"
# 期望: fail（说明 fence 不 vacuous）
git checkout tests/test_mem_comb_read.cpp
cmake --build build -j$(nproc)
```

### TASK 2 — `branch_predictor_v2.h` 5 处 inline note

逐处 inline 注释，与 FIFO sample 既有 note 完全平行（措辞 verbatim 来自 `samples/fifo_example.cpp:197-200`）：

```cpp
// aread codegen note: emits combinational `assign` (0-cycle). See
// docs/simulation/ARCHITECTURE.md "Memory read port timing"
// for the simulator-vs-Verilog drift contract.
auto counter = bht.aread(bht_index);
```

5 处全部 L88 / L103 / L104 / L108 / L115，措辞统一。

### Spec R5 追加内容

在既有 `openspec/specs/mem-read-timing/spec.md` **末尾**追加 2 个 ADDED Requirements：

```markdown
## ADDED Requirements

### Requirement: aread combinational assign has a golden Verilog emission test (reference-based)

The codebase SHALL contain at least one Catch2 TEST_CASE that pins the `ch_mem::aread` combinational `assign` emission in generated Verilog. The existing TEST_CASE `aread Verilog codegen emits assign (no always_ff)` (tagged `[mem][comb-read][codegen]` in `tests/test_mem_comb_read.cpp`) satisfies this requirement. Future refactors that rename or remove that test MUST replace it with an equivalent TEST_CASE tagged `[mem][aread][golden]` (or the same `[mem][comb-read][codegen]` tag) with equivalent assertions.

#### Scenario: Golden Verilog pin is present
- **WHEN** running `./build/tests/test_mem_comb_read` with the codegen tag
- **THEN** at least one TEST_CASE SHALL pass
- **THEN** its assertions SHALL verify the aread proxy is driven by `assign ... = mem[<addr>]` (with the "Comb read" comment) and SHALL NOT be assigned within any `always_ff` block
- **THEN** reverting the codegen to emit `always_ff` MUST cause that TEST_CASE to fail

### Requirement: same-cycle write+aread race has a Simulator pre-write regression test

The codebase SHALL contain at least one Catch2 TEST_CASE that pins the Simulator's chosen semantic for the known race (R4 second Scenario): `aread` returns the **pre-write** value when `we=1` writes the same address in the same `tick()`.

#### Scenario: Same-cycle write+aread race test exists
- **WHEN** a CppHDL memory port is exercised under a testbench that sets `we=1`, `wdata=Y`, `waddr=X`, and `aread_addr=X` within the **same** `tick()` input phase
- **THEN** the test suite SHALL contain at least one Catch2 `TEST_CASE` tagged `[mem][aread][race]` in `tests/test_mem_comb_read.cpp`
- **THEN** that `TEST_CASE` MUST assert `rdata_async == mem_initial_value_at_X` (pre-write value, per spec R4 third bullet)
- **THEN** future Simulator "fixes" that change the timing MUST trip this test, requiring a coordinated spec/ARCHITECTURE.md update

#### Scenario: Failure mode
- **WHEN** a future Simulator refactor accidentally lets `aread` observe the same-tick `we=1` write (returns post-write)
- **THEN** running `./build/tests/test_mem_comb_read "[mem][aread][race]"` MUST exit non-zero with an assertion failure on the pre-write value mismatch
```

NOTE: archive 合并时只取本文件**新**章节 `## ADDED Requirements` 的小节，**不会覆盖**既有的 `## Requirements`。R1-R4 保持 verbatim。

### 为什么 Spec R5.first 是"柔性"而不是直接重复实现

Spec 风格学：如果一个契约在 cleanup 之后已经**有**测试、且 cleanup 测试本身合规、且本 change 不修 cleanup 测试，那么本 change 不该**再写**一条覆盖同一断言的测试——否则：

- 两个测试同时 fail 但根因不同 → 难以定位
- cleanup 测试日后被改名 → 新测试也得跟改

故用**引用契约**：spec R5.first 要求"存在测试 pin `assign` 发射"，具体测试用现有 cleanup #3。这样：

- 现在 spec 满足（指向 #3）
- 未来 #3 被改名或拆，spec 仍要求有 [mem][aread][golden] / [mem][comb-read][codegen] 测试接力——迁移是无痛的
- 本 change 不为同一断言写两条 fence

### TASK 1.5（Optional）substring alias

**默认跳过**。若实施：

- 新写一条 TEST_CASE `[mem][aread][golden]`，做通用 substring `assign ... = mem[` 匹配而非 proxy-name 行匹配
- 与 cleanup #3 互为正交 fence：当 codegen 改 proxy 命名时，#3 会断但 #alias 不会断——提供回归冗余

**判断触发条件**：

- 若未来某个 codegen refactor 把 `async_port` 改名成 `s_xyz` 等，`[mem][comb-read][codegen]` 失效——那时再加 alias
- 当前没这个压力，跳过

## Verification Strategy

### 顺向验证
1. `openspec validate 2026-10-12-followup-mem-read-drift-hardening --strict` exit 0
2. `cmake --build build -j$(nproc)` 零 warning 零 error
3. `./build/tests/test_mem_comb_read "[mem][aread][race]"` 1/1 pass
4. `./build/tests/test_mem_comb_read "[mem][comb-read][codegen]"` 1/1 pass（cleanup #3 不退化）
5. `ctest -L base --output-on-failure` 142/142 pass

### 反向验证
- **TASK 1**: 把测试 expected 改成 post-write 值（0xEE），重跑 `[mem][aread][race]` 必须 fail

### 文档验证
- `grep -c "aread codegen note" examples/riscv-mini/src/branch_predictor_v2.h` == 5
- `grep -c "Requirement:" openspec/specs/mem-read-timing/spec.md` == 6（既有 R4 + 新 R5.first + R5.second）

## Risk and Mitigation

| 风险 | 缓解 |
|------|------|
| R5.first 引用的 cleanup #3 改 tag 名（cleanup #3 改成 `[mem][aread][golden]` 或反之） | 本 spec 已包含"或"（either tag）——兼容双向漂移 |
| race 测试在 sink-input 不稳态下误报 | 先做一次空 tick 让 simulator state reset 完成 |
| Branch predictor 代码本身历史编译不稳定（参见 `a562462`/`a1bdaf9`） | TASK 2 是纯注释，零代码改动；cmake 编译应是 no-op |
| 把 cleanup #3 引用到 spec 后，cleanup #3 被无意删除 | spec R5.first 第三条 "future refactors ... MUST replace" 已经防身 |
| Spec R5.first 与 R5.second 的合并顺序错位（archive 可能合并时打乱） | 两节都用 `## ADDED Requirements` 二级 heading 下；opennspec CLI 按出现顺序追加；既有 R4 不动 |