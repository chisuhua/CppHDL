# Design: input-port-assignment-antipattern-fix-2026-10

> 本 change = **4 处 `io().X = value`（落在 `ch_in` 上）改成 `set_input_value`** + 文档。
> **不改框架、不改契约** ⇒ `skip_specs: true`。
> ⚠️ 实施期发现该 example 的 FSM **另有更深缺陷** ⇒ 本文件 §4 给出完整取证链，
> 并裁决「**不在本 change 内修**」（独立立项）。

## 0. 一句话结论

`io().X = value` 落在 `ch_in` 上是一条**静默失效**路径（`ch_in` 无 `operator=`，
只有 `CHERROR` 分支 ⇒ 端口保持未驱动）⇒ 实测该 example **每次运行打 4 条
`[ERROR] Cannot assign value to input port!`**，而这些端口（`frame_start` / `data`）
从未被驱动 ⇒ 改成 `set_input_value` 后 **0 条 ERROR**，且 4 个端口真正上了 `data_map_`。

⚠️ 但 **stdout 不变**（`uart_tx` 恒 1 / `busy` 恒 0）—— 原因**不是**本 change 没生效，
而是这个 example 的**状态机本身从未工作过**（§4 有生成的 Verilog 硬证据）
⇒ ⇒ 该缺陷**独立立项**，不在本 change 内修（scope 纪律）。

## 1. 缺陷机制（为什么 `=` 会静默失效）

`include/core/io.h:236-240` —— `port<T, input_direction>` 的 `operator=` **只报错**：

```cpp
template <typename U> void operator=(const U &value) {
    static_assert(is_input_v<input_direction>, "Cannot assign to input port!");
    CHERROR("Cannot assign value to input port!");
    // ← 没有任何赋值动作 ⇒ 端口保持未驱动
}
```

三件事叠加成「**静默**」：

1. **编译通过**（`static_assert` 的谓词恒真 ⇒ 不触发）；
2. **运行不崩**（只 `CHERROR`，`CHLOG_SIMPLE` 打到 stderr，**不中止**）；
3. **程序 exit 0** ⇒ `run_all_ported_tests.sh` 只查退出码 ⇒ **判 PASS**。

⇒ ⇒ 这就是「未驱动输入端口」这一类缺陷能长期存活的原因
（`AGENTS.md §ANTI-PATTERNS` 已登记该义务，但**无自动化护栏**）。

## 2. 为什么改成 `set_input_value` 而不是 `<<=`

| 写法 | 机制 | 对本场景正确？ |
|---|---|---|
| `set_input_value(port, v)` | 写 `Simulator::data_map_[node_id]`（`include/simulator.h:177-207`）⇒ **TB 驱动** | ✅ **正确** |
| `sub.io().X <<= value` | `inputimpl::set_driver()` ⇒ 建 **src 边** ⇒ 变成「**有内部驱动**」 | ❌ 错误 |
| `io().X = value` | `CHERROR` + 无动作 | ❌ **本 change 要修的** |

⚠️ **为什么 `<<=` 是错的**：该 example 的 `UartTxTop` 把 `frame_start` / `data` 声明为
**顶层 `ch_in`**（`uart_tx.cpp:155-156`），`describe()` 只把它们接线给**子模块**
（`:171-172`）⇒ 顶层那两个端口**没有内部驱动** ⇒ 框架把它们当「**顶层端口**」
（`src/codegen_verilog.cpp:350-357` 的判据 = `type_input && driver() == nullptr`）
⇒ 若在 TB 里用 `<<=`，就会给它们**加上内部驱动** ⇒ 它们**不再是顶层端口**
⇒ 反而把「TB 该驱动它们」这件事**删掉**了 ⇒ ⇒ 是**反向**的修法。

⚠️ **值侧 MUST 用纯整数**：`ch_bool::operator uint64_t()` 是 **`explicit`**
（`include/core/bool.h:47`）⇒ `set_input_value(port, ch_bool(false))` **编译不过**。
仓内既有写法的样例：`tests/test_axi4_full_burst.cpp:187` `sim.set_input_value(io.rst_n, 1);`

## 3. 改动清单（4 处，均在 `examples/spinalhdl-ported/uart/uart_tx.cpp`）

| 行 | 改前 | 改后 |
|---|---|---|
| 193 | `device.instance().io().frame_start = ch_bool(false);` | `simulator.set_input_value(device.instance().io().frame_start, 0);` |
| 194 | `device.instance().io().data = ch_uint<8>(85_d);` | `simulator.set_input_value(device.instance().io().data, 85);` |
| 203 | `device.instance().io().frame_start = ch_bool(true);` | `simulator.set_input_value(device.instance().io().frame_start, 1);` |
| 205 | `device.instance().io().frame_start = ch_bool(false);` | `simulator.set_input_value(device.instance().io().frame_start, 0);` |

（行号是**改动后**的行号；改动前的行号为 189 / 190 / 199 / 201 —— 差值来自新增的注释块。）

## 4. 实测证据

### 4.1 A/B 对照（本 change 的收益）

复现方式：`git show HEAD:<file>` 取出改动前源文件，用**同一份**编译/链接命令
（`build/examples/spinalhdl-ported/uart/CMakeFiles/uart_tx_example.dir/flags.make`
+ `link.txt`）编成 `/tmp/opencode/uart_tx_old` ⇒ 与改动后的二进制对照。

| 指标 | 改动前（`HEAD` 版） | 改动后 |
|---|---|---|
| stderr 里 `Cannot assign value to input port!` | **4 条** | **0 条** |
| `frame_start` / `data` 是否被 TB 驱动 | ❌ 否（`CHERROR` 后无动作） | ✅ 是（写 `data_map_`） |
| stdout `uart_tx` / `busy` | 恒 `1` / 恒 `0` | **相同**（原因见 §4.2） |
| exit code | 0 | 0 |

⇒ **本 change 的收益是可实测的**（4 → 0 条 ERROR）；但**不能**用 stdout 证明，
因为这个 example 的输出**本来就不反映任何逻辑**（§4.2）。

### 4.2 ⚠️ 实施期发现：该 example 的 FSM **从未工作过**

生成的 Verilog（`toVerilog("uart_tx.v", ...)`，在 `/tmp/opencode/uart/` 下跑，不污染仓库）
里，两个输出是**常量**：

```verilog
assign top_uart_tx_inst_bool_lit   = 1'b1;
assign top_uart_tx_inst_unnamed_output = top_uart_tx_inst_bool_lit;      // uart_tx ≡ 1
assign top_uart_tx_inst_bool_lit_1 = 1'b0;
assign top_uart_tx_inst_unnamed_output_1 = top_uart_tx_inst_bool_lit_1;  // busy   ≡ 0
```

状态寄存器**自环**（永远停在 entry 状态）：

```verilog
always_ff @(posedge default_clock or posedge default_reset) begin
    if (!default_reset) top_uart_tx_inst_reg_3 <= top_uart_tx_inst__reg_3;  // __reg_3 = reg_3 ⇒ 自环
    else                top_uart_tx_inst_reg_3 <= 2'h0;
end
assign top_uart_tx_inst__reg_3 = top_uart_tx_inst_reg_3;
```

而 `is_in(状态)` 的比较节点（`eq_eq_4`..`eq_eq_7` = `reg_3 == 0/1/2/3`）
**被发射了但零消费者** ⇒ 死节点。

#### 4.2.1 根因：`if (ch_bool)` / `if (ch_uint == literal)` 在 `describe()` 期求值

`ch_bool` / `ch_uint` 的 `operator uint64_t()` **只对 const 节点返回真值**：

```cpp
// src/lnode/bool.cpp:44-49
ch_bool::operator uint64_t() const {
    if (this->node_impl_ && this->node_impl_->is_const()) return lit value;
    return 0;          // ← 非 const 节点（任意运算结果）恒 0
}
```

⇒ `if (io().frame_start == ch_bool(true))` 里的 `==` 是**非 const 节点**
⇒ 建图期恒为 **false** ⇒ 走 **else 分支**。

⇒ ⇒ **两个后果**（都被 §4.2 的 Verilog 证实）：

1. **无 else 的 `if`**（IDLE 的 `if (...) { load + transition_to(START) }`）
   ⇒ 分支体**从不发射** ⇒ `frame_start` 的比较是死节点、`transition_to` **从未注册**；
2. **有 else 的 `if`**（START/DATA/STOP 的 `if (baud==433) {…} else {baud+1}`）
   ⇒ **只发射 else** ⇒ 计数器**永远自增、永不归零**（实测 `reg <= reg + 1`）。

#### 4.2.2 为什么 `uart_tx` 是「常量 1」

`ch_state_machine::build()`（`include/chlib/state_machine.h:292-304`）会
**遍历所有状态**执行各自 `on_active` 一次。而 4 个状态都**无条件**写
`io().uart_tx` / `io().busy`，且 `ch_out::operator=` 是 `set_src(0, src)`
（**覆盖**，不是合并）⇒ **最后发射的 STOP 获胜**：

| 发射顺序 | 状态 | `uart_tx` | `busy` |
|---|---|---|---|
| 1 | IDLE | `1` | `0` |
| 2 | START | `0` | `1` |
| 3 | DATA | `(shift_reg & 1) != 0` | `1` |
| 4 | **STOP**（最后 ⇒ **生效**） | **`1`** | **`0`** |

⇒ 与实测（`uart_tx=1, busy=0` 恒定）**逐字吻合**。

#### 4.2.3 框架侧**已有**这只坑的正式文档与 API

`include/chlib/state_machine.h:34-38`：

> `if (ch_bool)` conditions inside on_active closures are **NOT** hardware
> conditionals — ch_bool's contextual bool conversion silently evaluates false
> at describe time. For runtime (cycle-accurate) transitions use
> `transition_when(ch_bool, target)`

⇒ ⇒ 该 example 用的是**已文档化的反模式**；同型缺陷的先例是
`riscv-mini-cache-wrapper-live-logic-2026-10`（`i_cache_prefetch.h` 的
`if (io().req == ch_bool(true) && io().we == ch_bool(true))`）。

## 5. 裁决：为什么**不在**本 change 内修 FSM

| 理由 | 说明 |
|---|---|
| **scope** | 本 change 的提案与 AC 明确限定「4 处失效赋值」⇒ 修 FSM 是**重写逻辑**（状态转移改 `transition_when`、条件赋值改 `select`、输出改按 `is_in()` 多路选择 ⇒ 且必须**只驱动一次**） |
| **仓内先例** | 同型缺陷（C++ `if` 包裹 `ch_bool`）由 `riscv-mini-cache-wrapper-live-logic-2026-10` **单独**一个 change 修 |
| **验证方式不同** | 修 FSM 后必须**加自检断言**（否则仍然只是「打印更对」）⇒ 那是另一个 change 的 AC |
| **风险** | 重写 FSM 会改变该 example 的**生成 Verilog 结构**（当前是死图）⇒ 需要独立的变异验证 |

⇒ ⇒ 已立项 **`.rddf/improvements/uart-tx-fsm-live-logic-2026-10.md`**（含 §4 的全部取证）。

## 6. 范围外（**明确不做**）

- **6 处**在 `samples/`（`stream_component_example.cpp` 4 + `stream_handshake_example.cpp` 2）
  ⇒ 这两个 sample 在 `samples/disabled_samples.txt` 里、**当前连编译都不过**
  （归位条件 = `chlib::stream_fifo` 读使能外部化，需 **ADR**）
  ⇒ 其整体结构届时会重写 ⇒ 现在改这 6 处没有意义。
- **不新增**通用静态护栏 ⇒ 已由 `input-port-driving-guard-2026-10` 的 spike
  判定「通用状态型护栏关闭」（见该 change 的 `design.md §0`）。
- **不给 `ch_in` 加编译期诊断**（如 `operator=` 改成 `delete` + 更好的错误）
  ⇒ 那是**框架契约变更**（会让现有 235 处 `ch_out` 的 `=` 语义讨论重新打开）
  ⇒ 独立立项。

## 7. ⚠️ 顺带暴露的**覆盖缺口**（登记给 planner，不擅改契约）

`uart_tx_example` 是 **main() 型**程序（不是 Catch2 测试）⇒

- `run_all_ported_tests.sh` 只查**退出码** ⇒ 这个「输出是常量」的 example **判 PASS**；
- `example-assertion-quality` capability 管的是「**examples 内的测试** MUST 断言语义输出」
  ⇒ 它的适用范围**不含** main() 型 example ⇒ ⇒ **该类程序的语义输出无人校验**。

⇒ ⇒ 这是 `example-assertion-quality` 的**边界问题** ⇒ **planner** 议题
（build **不得**自行扩 capability 范围）。已在 §4.2 + `AGENTS.md` NOTES 留证。

## 8. 验收对照

| 项 | 结果 |
|---|---|
| `grep` 归零（该文件再无 `io().X =` 落在 `ch_in` 上） | ✅ 0（方向感知扫描：全仓仅剩 `samples/` 的 6 处） |
| 编译 0 error / 0 新增 warning | ✅（`logger.h:194 nonnull-compare` 是**既有噪声** —— 强制重编未改动的 `test_module` 同样出现 3 处） |
| `run_all_ported_tests.sh` | ✅ 28/28 |
| `scripts/run_all_samples.sh` | ✅ 35/35（**零** sample 改动） |
| `ctest -LE perf` | ✅ 191/191 |
| 计数锚点 | ✅ `CTEST_TOTAL` 202 / `verilator_e2e` 38 / non-perf 191 / `EXAMPLES_TEST_SOURCES` 17 |
| 9 个 `scripts/verify_*.sh` + 3 个 `--self-test` | ✅ 全 exit 0 |
| **A/B 证据** | ✅ 4 条 `CHERROR` → 0 条（§4.1） |
