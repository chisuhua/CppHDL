# 设计：rv32i-soc-dual-parity-2026-10

> 本文件是 P1 产物。所有「实测」结论都基于 2026-10-25 的 HEAD `0b8fa43`。

## D1. DUT 形态：扩展 `Rv32iPipelineTop`（不新建 fixture wrapper）

**裁决**：把 `Rv32iPipelineTop` 从空壳改成真 SoC 顶层。

**替代方案与否决理由**：

| 方案 | 否决理由 |
|------|----------|
| 测试内 fixture wrapper（先例 `Rv32iPipelineFixture`） | ① SoC 装配出现**两份**（`itcm`/`dtcm`/`pipeline` 的接法各写一遍）⇒ drift 风险；② `rv32i_pipeline_top.h` **永远零消费者** ⇒ 它的豁免条目永久残留，而 `rv32i-canonical-home` 主 spec 的「stale 豁免 MUST 失败」条款与 `AGENTS.md` 的「归位条件 = C3」都会变成**不可达成**的承诺 |
| 只做 pipeline 级 parity（不碰 TCM） | 偏离 `objective-rv32i-e2e` 的 SoC 级目标；`InstrTCM`/`DataTCM` 的 sread 语义永远不进 verilator e2e |

**ADR-046 D10-rev**：D10 原文「C2 允许改内部装配，但 MUST NOT 改端口」——约束
对象是 **C2**。C3 是 D10 自己指定的「归位事件」，必须扩端口才能观测。本 change
在 ADR-046 §5 追加 **D10-rev**（与 §5 已有的「D3 #14 落地形态修订」同型），
并把新端口集合交给护栏 C6 快照钉住 ⇒ 后来的改动仍然不可静默。

## D2. 端口集合（15 个，最大位宽 48）

| 端口 | 方向 | 位宽 | 来源 |
|------|------|------|------|
| `ext_clk` | in | 1 | 保留 |
| `ext_rst` | in | 1 | 保留 |
| `prog_we` | in | 1 | 新 → `itcm.write_en` |
| `prog_addr` | in | `ADDR_WIDTH` | 新 → `itcm.write_addr` |
| `prog_data` | in | 32 | 新 → `itcm.write_data` |
| `obs_instr_addr` | out | 32 | `pipeline.instr_addr` |
| `obs_data_addr` | out | 32 | `pipeline.data_addr` |
| `obs_data_write_data` | out | 32 | `pipeline.data_write_data` |
| `obs_data_write_en` | out | 1 | `pipeline.data_write_en` |
| `obs_perf_cycles` | out | 48 | `pipeline.perf_cycles` |
| `obs_perf_instructions` | out | 48 | `pipeline.perf_instructions` |
| `obs_perf_branch_count` | out | 32 | `pipeline.perf_branch_count` |
| `obs_perf_branch_mispredict` | out | 32 | `pipeline.perf_branch_mispredict` |
| `obs_perf_load_use_stalls` | out | 32 | `pipeline.perf_load_use_stalls` |
| `obs_perf_control_stalls` | out | 32 | `pipeline.perf_control_stalls` |

**删除**：`ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` / `ext_data_ready`
—— 在 `describe()` 里从未被引用（TCM 在内部直连 pipeline）。ZERO-DEBT 的
「无死代码」要求删除，而不是「接上」或「保留给未来」。

⚠️ `data_strbe`（`ch_out<ch_uint<4>>`，`rv32i_pipeline.h:48`）当前是**常量 15**
（`:178`），不是有信息量的观测点 ⇒ 不引出。

## D3. TCM 规模取 `ADDR_WIDTH = 12`（而不是 20）

`InstrTCM<20,32>` 的 `SIZE = 1 << 20` 字 ⇒ `ch_mem` 展开成 4 MB 数组；verilator
要把它变成一个 1,048,576 元素 `VlUnpacked` 并参与编译/运行。对「20 条指令的 parity
固件」毫无收益。

⇒ 取 `ADDR_WIDTH = 12`（4096 字 = 16 KB ROM/RAM）。

**约束（必须写进测试注释与断言）**：`pc` 与数据地址的高位被**截断**
（`itcm.addr` 只接低 12 位）。`pc_reg` 复位值是 `0x80000000`（`if_stage.h:66`），
其低 12 位为 0 ⇒ 程序从字 0 开始，地址按 +4 递增到字 N；数据地址必须落在
`< 0x1000`。⇒ 固件与 TB 都是这个约束的**共同持有者**。

## D4. 固件：20+ 条指令，**无数据冒险**构造

**关键约束（实测）**：lib 版 `HazardUnit` **不接数据前推**
（`hazard_unit.h:151-152`：`io().rs1_data = io().rs1_data_raw`），因为接上会在
真实流水线装配里形成组合环（ADR-046 D3 #14 / `git show bf78367`）。ADR-046 D5
对此仍**悬置未裁决**。

⇒ 固件 MUST 按「无前推的 5 级流水线」构造：**生产者在 WB 写回之后**消费者才读。
实现手段 = 在每条生产者与消费者之间插入填充 `addi x0, x0, 0`（NOP），数量由
DD2 实测确定（初值 3），并把该约束写成固件构造处的注释。

**MUST NOT** 通过「把流水线接上前推」来解决 —— 那是 D5 的裁决对象，不在本 change。

## D5. 固件编码：测试内建最小编码 helper + 手算常量交叉校验

20+ 条机器码手写十六进制易错。方案：测试内建 `enc_r` / `enc_i` / `enc_s` / `enc_b`
四个 constexpr 函数，程序用结构化形式表达（`addi(1, 0, 10)`）。

⚠️ **避免自证（circular validation）**：helper 自身用**手算常量**交叉校验 ——
对每种类型取 1-2 条已知编码（如 `addi x1, x0, 1` = `0x00100093`，
`sw x3, 16(x1)` = `0x0030A823`）断言 `enc_*(...) == 常量`。
若 helper 与常量不一致 ⇒ 编译期/首条断言即失败。

## D6. 固件加载时序

`prog_we = 1` + `prog_addr = i` + `prog_data = word[i]`，逐拍写入，
**期间 `ext_rst` 保持 1**。释放复位后计数器从 0 起算（
`cycle_count` / `commit_count` 等在 `reset_active` 时清零 —— `rv32i_pipeline.h:218-230`）。

⇒ 加载期不污染 parity 观测向量。

## D7. 逐周期 diff：手工双实例（不用 `DUAL_BACKEND_TEST_PAIR`）

`DUAL_BACKEND_TEST_PAIR` 的语义是「同一 body 各跑一遍」，每次 run 只看得见**自己的**
后端 ⇒ 无法跨后端 diff。逐周期 diff MUST 用「同一 TEST_CASE 内 2 个 `ch_device` +
2 个 `Simulator`」的形态（先例：`tests/test_verilator_complex_e2e.cpp` 的 SpiFsm 用例，
以及 `read_port_output` / `make_temp_dir` helper）。

**三个 TEST_CASE**：

1. `Rv32iSoc - DualBackendTraceParity`：双路逐周期 diff + 语义 golden。
2. `Rv32iSoc - JitFallbackContract`：JIT 回落三件套（需 `#if __has_include`）。
3. （可选，同一 TEST_CASE 内 SECTION）固件自校验：编码常量、TCM 地址边界。

**verilator 缺席处理**：`WARN` + `return`（**不是** `SKIP`）——
`dual-backend-test-coverage` R4 的既有条款（全 SKIP ⇒ Catch2 exit 4 ⇒ CTest 判红）。

## D8. JIT 归因：在**所有**失败出口写原因

⚠️ **2026-10-26 实测订正**：本节原方案只改 `generate_ir` 的循环失败出口
（`if (r != JitResult::SUCCESS) { return r; }`）⇒ **实测不够**：
`JitCompiler::compile()` 里 **`allocate_buffer` 在 `generate_ir` 之前**，
而它的「节点宽度 > 64」检查**不设** `last_error_msg_` ⇒ **它才是本 SoC 的
首个命中点** ⇒ 回落日志的原因**是空的** ⇒ 完全不可归因。

⇒ **实施**（两个失败出口都写归因）：

```cpp
// allocate_buffer（实测的首个命中点）
last_error_msg_ = "unsupported node width: " + to_string(node->type()) +
                  " (node_id=" + ... + ", size=" + ... + " bits > 64)";

// generate_ir 的循环（类型不支持，如 mem read port）
last_error_msg_ = "unsupported node type: " + to_string(node_type) +
                  " (node_id=" + ... + ")";
```

**不改** `jit_ir_gen_lnode_state.cpp` 的签名（那是 free function，拿不到
`last_error_msg_`），也**不改** `JitResult` 枚举。

⇒ `CHINFO("JIT skipped: %s")` 从此打印**真实原因**。实测（RV32I SoC）：
`unsupported node width: mem (node_id=2460, size=131072 bits > 64)`
（D-TCM 的 `ch_mem<ch_uint<32>, 4096>`）⇒ 「宽度超限」与「类型不支持」与
「图太小」三者**可区分**。

⚠️ 连带结论：**任何含大 `ch_mem`（> 2 个字）的设计都必然回落**（节点宽度
> 64 bit）⇒ 这不是缺陷，而是 JIT 的**已知能力边界** ⇒ 回落契约 MUST 被
**显式断言**（而非当作意外）。

**测试如何读日志**：`CHINFO` → `log_message_simple` → **`std::cout`**（
`include/utils/logger.h:351`，注意**不是** `cerr`）⇒ 测试内 `std::cout.rdbuf(oss.rdbuf())`
临时捕获，断言含 `JIT skipped` 与 `mem_read_port`；析构恢复。

⚠️ 捕获窗口必须包住 `set_jit_enabled(true)` 调用（编译尝试发生在那里：
`src/simulator.cpp:519-521`）。

## D9. 护栏 C6 与豁免表联动

- `SOC_TOP_PORTS` 快照 → 新的 15 端口集合（`<名>:<方向>:<位宽>`，按名排序）。
- `CH_DEAD_ALLOWANCE` 删除该条目，`EXPECTED_DEAD_ALLOWANCES` `1 → 0`。
- **fixture 同步**：self-test 的 `mk_fixture` 会 `cat > examples/riscv-mini/src/rv32i_pipeline_top.h`
  造一个假顶层头；它的端口集合 MUST 与新快照逐字一致，否则 ⑭（改名）/⑮（翻方向）/
  ⑯（改位宽）三条 fixture 的基线会错位（它们靠 `sed` 在假头上做单点变异）。

## D10. 计数同步（单一真相）

| 锚点 | 变化 |
|------|------|
| root `AGENTS.md` `CTEST_TOTAL` | `199 → 200` |
| `tests/test_*.cpp` 计数（AGENTS.md NOTES） | `115 → 116` |
| `ctest -L verilator_e2e` | +1 |

## D11. 文档同步

- `include/cpu/AGENTS.md`：KEY FILES 里 `rv32i_pipeline_top.h` 的说明从
  「零消费者 / 归位条件 = C3」改为「C3 的 DUT（有消费者）」。
- `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md:279-320` 的
  `Rv32iPipelineTop` 示例（若展示端口列表）同步。
- root `AGENTS.md` 的 KNOWN LIMITATIONS：「JIT 回落」条目从「将来修」改为
  「已断言的契约」；并写明 RV32I 固件必须无数据冒险（D4）。

## 风险与验证顺序

1. **先验证 D3**（verilator 能否编译含 TCM 的设计、冷编译耗时）—— 这是最大的
   未知项。若 12 位 TCM 仍然编译失败，退回「fixture wrapper」方案并重开 P0。
2. 再验证 D7 的逐周期 diff 是否真的能跑（不是「编译通过」）。
3. 最后接 D8 的 JIT 归因（独立于 parity，可单独回滚）。

---

## 12. P2 实测反馈（2026-10-25）—— 本 change 挂起的原因

> 本节在 P2 起步后补写。结论：**D1（扩展 `Rv32iPipelineTop`）是对的，但 C3 的验收在
> 前置缺陷修复前不可达。**

### 12.1 D3（TCM 规模）的实测结论：**通过**

改造后的 DUT 在 verilator 下**冷编译 13.7 s**、`port_access_` 75 个条目、
`test_rv32i_soc_dual_parity` 整体 13.7 s 跑完 ⇒ `ADDR_WIDTH=12` 的选择成立，
D1 方案无需重开。

### 12.2 但 SoC 跑不动程序（→ 已拆出前置 improvement）

| 观测 | 实测（41 条固件 / 200 拍） |
|---|---|
| `obs_instr_addr` | 每 11 拍从 `0x80000028` 归 **0** |
| `obs_perf_instructions` | **18**（应为 41） |
| `obs_perf_branch_count` / `_mispredict` | **18 / 18**（固件只有 2 条分支） |
| `obs_data_write_en` | 53/200 拍置位，地址是 ALU 结果（10 / 13 / 0） |

逐层定位到 **5 个前置缺陷**（D1 已修、D2–D5 已定位）——
见 `.rddf/improvements/rv32i-pipeline-commit-path-2026-10.md`。

### 12.3 对 D7（逐周期 diff）的修订：**必须区分 don't-care**

原设计假设「10 个观测值逐 tub 相等」。实测发现 `obs_data_addr` /
`obs_data_write_data` 在**非 store 拍**是流水线寄存器的裸值（`exmem_alu_result` /
`exmem_rs2_data`），两后端可以合法地不同（52 处）⇒ 比对 MUST 按
`obs_data_write_en` 掩码，且**掩码本身要有非空断言**（本 change 的草稿测试里
`REQUIRE(store_cycles >= 3u)` 就是这条）。

⚠️ 但前置缺陷 D4（`write_en` 在非 store 拍置位）让这个掩码**当前不可靠**
⇒ 前置 change 修完后，掩码规则应重新评估（最好能取消掩码、直接全量比对）。

### 12.4 对既有测试的连带发现

`tests/test_verilator_complex_e2e.cpp` 的 `perf_instructions == 3`（3 条 ALU 指令）
在 `mem_stage.mem_valid_out = is_load & mem_valid` 的语义下**不可能是「3 条指令退休」**
⇒ 该断言被 spurious 事件满足（「绿而错」）。前置 change 的 §3.4 要求把它改成语义断言。
