# Tasks: cleanup-250198a-aread-combinational-debt

> **TDD 流程**: 每个 Task 遵循 RED → GREEN → REFACTOR
> **依赖关系**: Task 1 → Task 3、Task 4（Tasks 3+4 依赖最终语义措辞）; Task 2 独立
> **总工时**: 3-4 小时

---

## Task 1: 修复死代码 + 注释矛盾（Issue 1+2）

**优先级**: P0
**依赖**: 无
**预计工时**: 30 min
**关联文件**: `src/codegen_verilog.cpp`

### 1.1 删除 unreachable enable 分支（Issue 1）

- [ ] **GREEN**: 删除 `if (enable && node_names_.count(enable)) { ... } else { ... }` 二分支结构
      直接替换为单条 `assign proxy = mem[addr];` 发射

**当前代码位置**: `src/codegen_verilog.cpp:1097-1105`

**改动后应该是**:
```cpp
if (node->port_type() == ch::core::mem_port_type::async_read) {
    // aread: combinational read in generated Verilog (0-cycle latency).
    // The simulator evaluates aread at posedge (sequential list); the
    // simulator-vs-Verilog drift for reg-driven addresses is documented
    // in ARCHITECTURE.md and tracked separately.
    out << "    assign " << proxy_name << " = " << mem_name
        << "[" << addr_name << "]; // Comb read " << proxy_name << "\n";
    return;
}
```

### 1.2 重写注释为真实机制（Issue 2）

- [ ] **GREEN**: 重写整段 `print_mem_read_port` 顶部注释（行 1071-1082），反映：
      - aread 在 simulator 中位于 `sequential_instr_list_`（`simulator_init.cpp:116-121`）
      - aread 在 Verilog 中是连续 `assign`（0-cycle）
      - 同 cycle write+read race 与 reg-driven address skew 是已知 drift（详 ARCHITECTURE.md）

### 1.3 验收（Task 1 完成时）

```bash
# 编译零错零警
cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
# 期望: 空

# mem 测试 5/5（按可执行文件名匹配）
cd build && ctest -R "test_memory|test_mem$|test_mem_connection|test_mem_timing|test_mem_comb_read" --output-on-failure
# 期望: 5/5 pass
```

---

## Task 2: 修复测试文件（Issue 3 + Oracle 新发现 #1）

**优先级**: P0
**依赖**: 无
**预计工时**: 45 min
**关联文件**: `tests/test_mem_comb_read.cpp`

### 2.1 重命名 4 个 TEST_CASE（含 test 4 矛盾修复）

- [ ] **GREEN**:
  - 行 102: `"isread 0-cycle latency in Simulator"` → `"aread 0-cycle latency in Simulator"`
  - 行 150: `"isread does NOT register: synchronous latency 1-cycle preserved"` → `"sread retains 1-cycle latency (regression)"`
  - 行 185: `"isread Verilog codegen emits assign (no always_ff)"` → `"aread Verilog codegen emits assign (no always_ff)"`
  - 行 243: `"isread read with no write: stable init value"` → **`"aread cross-cycle write-then-read consistency"`**（与 2.4 加入 write 一致）

### 2.2 修正 fixture 隐式转换

- [ ] **GREEN**: 行 70 改为
  ```cpp
  auto sync_port = mem.sread(ch_uint<4>(io().addr_sync.impl()), "mem_sread");
  ```
  并在 fixture 上方加注释："`mem.sread(addr, name)` 签名中第二参数隐式转换为 `ch_bool enable`（const char* → bool → ch_bool）。fixture 故意保留此隐式转换以匹配 codegen 测试断言 `mem_sread_data_proxy`。"

### 2.3 修正文件 header 注释

- [ ] **GREEN**: 重写文件 header 注释（行 1-29），反映：
      - aread 在 Simulator 中位于 sequential list（posedge 阶段求值）
      - "0-cycle" 观察来自 tick 末尾的 eval_combinational 传播
      - 同 cycle write+read race + reg-driven address skew 是已知 simulator-vs-Verilog drift
      - 删除 "Simulator: aread evaluates during the combinational phase" 等错误描述

### 2.4 修复 TEST_CASE 4 overpromise（选择 A：实现 write+read）

- [ ] **GREEN**: 在 TEST_CASE 4 增加 write+read 验证：
  ```cpp
  // Tick 2: write mem[5]=0xFF via write port (addr_sync + wdata + we=1)
  simulator.set_input_value(device.instance().io().addr_sync, static_cast<uint64_t>(0x5));
  simulator.set_input_value(device.instance().io().wdata, static_cast<uint64_t>(0xFF));
  simulator.set_input_value(device.instance().io().we, static_cast<uint64_t>(1));
  simulator.tick();

  // Tick 3: read with aread at addr=5 (we=0) — sees 0xFF (post-write)
  simulator.set_input_value(device.instance().io().addr_async, static_cast<uint64_t>(0x5));
  simulator.set_input_value(device.instance().io().we, static_cast<uint64_t>(0));
  simulator.tick();
  auto rdata_after_write = simulator.get_port_value(device.instance().io().rdata_async);
  REQUIRE(static_cast<uint64_t>(rdata_after_write) == 0xFF);
  ```

### 2.5 修复 hardcoded `/tmp` 路径

- [ ] **GREEN**:
  - 添加 `#include <filesystem>` 和 `#include <unistd.h>`（如未包含）
  - 路径改为 `std::filesystem::temp_directory_path() / ("test_comb_read_codegen_" + std::to_string(::getpid()) + ".v")`
  - 测试结束处 `std::filesystem::remove(out_file)`

### 2.6 简化 magic window scan（含否定断言）

- [ ] **GREEN**: 改用行 split + 精确字符串匹配：
  ```cpp
  // aread 应 emit `assign proxy_name =` 单行
  std::istringstream iss(verilog);
  std::string line;
  bool found_assign = false;
  while (std::getline(iss, line)) {
      if (line.find("assign " + async_proxy + " =") != std::string::npos) {
          found_assign = true;
      }
  }
  REQUIRE(found_assign);

  // sread 应仍 emit `always_ff` 块（通过查找 "Read port <proxy>" 注释行）
  REQUIRE(verilog.find("Read port " + sync_proxy) != std::string::npos);
  REQUIRE(verilog.find("Comb read " + sync_proxy) == std::string::npos);
  ```

### 2.7 文件末尾添加 newline

- [ ] **GREEN**: 在最后 `}` 后添加 `\n`

### 2.8 验收（Task 2 完成时）

```bash
# 新测试按 Catch2 filter 直接调用
./build/tests/test_mem_comb_read "[comb-read]"
# 期望: 4 个 TEST_CASE 全部运行并 pass

# 无遗留 "isread" typo
grep -rn "isread" tests/
# 期望: 空

# 文件末尾有 newline
tail -c 1 tests/test_mem_comb_read.cpp | od -c | head -1
# 期望: \n
```

---

## Task 3: 文档同步（Issue 4 + Oracle 新发现 #2/#3）

**优先级**: P1
**依赖**: Task 1 完成（最终语义措辞）
**预计工时**: 1-2 h
**关联文件**: `docs/simulation/ARCHITECTURE.md`, `AGENTS.md`, `samples/fifo_example.cpp`, `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`

### 3.1 在 ARCHITECTURE.md 加 Memory read port timing 小节

- [ ] **GREEN**: 添加新小节，包含:
      - aread: Verilog `assign` (0-cycle), Simulator posedge 评估
      - sread: Verilog `always_ff` (1-cycle), Simulator posedge 评估
      - Known drift: reg-driven address skew + same-cycle write+read race

### 3.2 root AGENTS.md CONVENTIONS 加一行（措辞调整）

- [ ] **GREEN**: 添加:
  ```
  - **Mem read port**: `aread` emits combinational `assign` in Verilog (0-cycle); `sread` emits `always_ff` (1-cycle). In Simulator, both are evaluated at posedge in the sequential list; downstream registers in the consumer provide the required latency. imem must use sread to maintain pc_lag alignment.
  ```
  （避免 "registered downstream" 强约束——两个现有 FIFO 示例无下游 register）

### 3.3 FIFO 示例加注释（真实描述，无虚构）

- [ ] **GREEN**: 在 `samples/fifo_example.cpp:197` 和 `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:168` 附近加注释:
  ```cpp
  // NOTE: This FIFO reads combinationally via aread (dout = mem[rd_ptr] with no
  // downstream register). Generated Verilog now emits `assign` (0-cycle
  // combinational read, first-word-fall-through style) instead of `always_ff`
  // (1-cycle). The Simulator still evaluates aread at posedge; visible behavior
  // in the Simulator matches the Verilog `assign` because tick() runs
  // eval_sequential + a trailing eval_combinational that propagates the proxy
  // to dout within the same tick.
  ```

### 3.4 验收（Task 3 完成时）

```bash
# ARCHITECTURE.md 包含新小节
grep "Memory read port timing" docs/simulation/ARCHITECTURE.md
# 期望: 匹配

# AGENTS.md 包含 aread/sread 时序描述
grep -E "aread.*combinational|sread.*always_ff" AGENTS.md
# 期望: 匹配

# FIFO 注释真实描述（不虚构 rd_data_reg）
grep -E "combinational.*aread|dout.*mem\[rd_ptr\]" samples/fifo_example.cpp examples/spinalhdl-ported/fifo/stream_fifo_example.cpp
# 期望: 匹配

# 确认无虚构 rd_data_reg
grep -rn "rd_data_reg" samples/ examples/ include/
# 期望: 空（除 tasks.md 自身的提及）
```

---

## Task 4: Spec delta（已部分完成）

**优先级**: P0（OpenSpec 格式合规必需）
**依赖**: Task 1 完成
**预计工时**: 已完成（proposal/design/tasks 撰写时一并创建）

### 4.1 验证 spec delta 已创建

- [ ] **GREEN**: `openspec/changes/cleanup-250198a-aread-combinational-debt/specs/mem-read-timing/spec.md` 已含 4 个 ADDED Requirements

### 4.2 验收

```bash
# OpenSpec validate 通过
openspec validate cleanup-250198a-aread-combinational-debt
# 期望: "Change 'cleanup-...' valid"

# Spec delta 含 SHALL
grep -c "SHALL" openspec/changes/cleanup-250198a-aread-combinational-debt/specs/mem-read-timing/spec.md
# 期望: >= 4（每条 Requirement 至少 1 个 SHALL）
```

---

## 整体验收（PHASE COMPLETION GATE）

```bash
# 1. OpenSpec 格式合规
openspec validate cleanup-250198a-aread-combinational-debt
# 期望: 通过

# 2. 编译零错零警
cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
# 期望: 空

# 3. mem 测试 5/5（按可执行文件名）
cd build && ctest -R "test_memory|test_mem$|test_mem_connection|test_mem_timing|test_mem_comb_read" --output-on-failure
# 期望: 5/5 pass

# 4. 新测试按 filter 直接调用
./build/tests/test_mem_comb_read "[comb-read]"
# 期望: 4 个 TEST_CASE 全部运行并 pass

# 5. 全量基线回归
cd build && ctest -L base --output-on-failure --timeout 300 2>&1 | tail -10
# 期望: base 标签测试全 pass

# 6. 文档同步
grep "Memory read port timing" docs/simulation/ARCHITECTURE.md
grep -E "aread.*combinational|sread.*always_ff" AGENTS.md
grep -E "combinational.*aread|dout.*mem\[rd_ptr\]" samples/fifo_example.cpp examples/spinalhdl-ported/fifo/stream_fifo_example.cpp
# 期望: 全部匹配

# 7. Spec delta 完整
grep -c "SHALL" openspec/changes/cleanup-250198a-aread-combinational-debt/specs/mem-read-timing/spec.md
# 期望: >= 4
```

---

## Commit Strategy

每个 Task 一个 commit（便于独立回滚）:

```
commit 1: fix(codegen): remove unreachable enable branch in print_mem_read_port
commit 2: fix(codegen): correct simulator classification comment in print_mem_read_port
(可选合并 1+2)

commit 3: test(mem): rename test cases (isread → aread), add write+read cross-cycle, fix fixture and overpromise

commit 4: docs(mem): document aread vs sread timing semantics and known drift
```

或合并为单个 cleanup commit:
```
commit: cleanup(250198a): resolve aread-combinational debt per 5-agent review
- src/codegen_verilog.cpp: delete dead enable branch, fix comment
- tests/test_mem_comb_read.cpp: rename cases, add cross-cycle test, fix fixture, simplify magic scan
- docs/simulation/ARCHITECTURE.md + AGENTS.md: document timing semantics
- samples/fifo_example.cpp + ...: add combinational-read FIFO note
```

## Rollback Tag

提交清理后:
```bash
git tag -a post-250198a-cleanup -m "Cleanup of HEAD 250198a aread-combinational debt"
```