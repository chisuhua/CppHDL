---
id: decision-request-rv32i-ext-clock-dead-port-2026-11
日期: 2026-11-12
提出者: builder（B2 / E3 · `riscv-mini` 的独立 RTL TB）
状态: ✅ **已裁决并落地**（2026-11-14 · 用户裁决 **A2** = 删端口）
关联: .rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc
---

# 决策请求单：`ext_clk` 是**死端口**（顶层头声明的意图未在产物里兑现）

---

## 1. 事实（产物级 · 可复现）

`examples/riscv-mini/src/rv32i_pipeline_top.h` 写着：

```cpp
pipeline.io().rst <<= io().ext_rst;
pipeline.io().clk <<= io().ext_clk;
```

⇒ ⇒ 但生成的 RTL 里：

- **所有**流水线寄存器都锁在**框架时钟**：`always_ff @(posedge default_clock or posedge default_reset)`
  （产物里 59 个 `always_ff` 全部是 `default_clock` / `default_reset`，**没有一处**用 `ext_clk`）。
- 流水线的 `clk` 端口**被赋值但无消费者**：产物中 `top_pipeline__unnamed_input_1`
  只有一条赋值（`= top_unnamed_input`），**没有任何使用点**。

⇒ ⇒ ⇒ **`ext_clk` 对行为零影响** ⇒ ⇒ ⇒ 「SoC 由 `ext_clk` 驱动」这个直觉**是错的**
⇒ ⇒ ⇒ 真正的驱动源是框架的 `default_clock`（TB 里接 `clk` 即可）。

## 2. 为什么值得裁决

E3 这批工作的**核心价值**就是「**按名字驱动外部端口**的独立 Verilog 仿真」
⇒ ⇒ 用户拿到产物后**按端口名**接线 ⇒ ⇒ 一个**名字存在但不起作用**的端口
⇒ ⇒ 就是外部用户最先踩的坑之一（而且**没有任何报错**）。

⚠️ 仓内已有先例：`mem-port-arg-cross-layer` 那一轮的 ADR-066 就是为「端口/实参的语义归属」
做的范围裁定 ⇒ ⇒ 本条属**同类**问题，但**范围小得多**（只影响一个 example 顶层的 1 个端口）。

## 3. 三个选项

| 选项 | 内容 | 代价 |
|---|---|---|
| **A** | **对齐意图**：让流水线的寄存器真的由 `ext_clk` 驱动（或删掉 `ext_clk` 端口并在头注释里说明「流水线用框架时钟」） | A1 改 codegen 的时钟选择 ⇒ **影响面大**（全仓 codegen）⇒ ⇒ 应独立立项；A2 只删端口 + 改注释 ⇒ **零行为变更** ⇒ ⇒ 便宜 |
| **B** | **登记为已知偏离**（残差台账两侧同批） | 债留在仓里 ⇒ ⇒ 用户仍会被误导 |
| **C** | **不动**（只在本仓登记文档里说明） | 与 B 差别在「是否进残差台账」⇒ ⇒ 台账有 fail-closed 护栏 ⇒ ⇒ 会主动催办 |

## 4. 建议

**建议 A2**（本轮可做、零行为变更）：**删掉 `ext_clk` 端口**（或保留但明确注释「不影响行为」）
⇒ ⇒ 用户的直觉不再被误导 ⇒ ⇒ 「真让 `ext_clk` 生效」（A1）**另独立立项**（那是 codegen 层的改动）。

⇒ ⇒ **未裁决前**：TB 照常把 `ext_clk` 接上（端口存在 ⇒ MUST 连接，否则 `PINMISSING` **致命**）
⇒ ⇒ 但**不依赖它**（⇒ ⇒ 判据与 `ext_clk` 无关 ⇒ ⇒ 无论将来 A/B/C 怎么裁，TB 都不受影响）。

---

## ✅ 裁决结果（2026-11-14 · 用户裁决 **A2** = 删端口）

> ⚠️ **改判记录**：本单 2026-11-13 的 R8 裁决曾采纳「**A2 的注释半**」（保留端口 + 注释），
> 理由是「删它改 SoC 顶层端口契约 ⇒ 公开面变更 ⇒ 而它不造成任何行为差异 ⇒ 成本收益不划算」。
> ⇒ ⇒ **2026-11-14 用户改判 = A2 的「删端口半」** ⇒ ⇒ 即：用户接受公开面变更的成本，
> 换取「端口面上不留会骗人的名字」⇒ ⇒ 本节记录**改判**而非首次裁决。

### 落地内容（2026-11-14）

| # | 落点 | 内容 |
|---|---|---|
| 1 | `examples/riscv-mini/src/rv32i_pipeline_top.h` | **删** `ch_in<ch_bool> ext_clk{"ext_clk"}` + `pipeline.io().clk <<= io().ext_clk;` |
| 2 | `include/cpu/pipeline/rv32i_pipeline.h` | **删** `ch_in<ch_bool> clk;`（**零消费者** ⇒ ZERO-DEBT ⇒ 不删则变悬空输入 ⇒ undriven wire ⇒ verilator **UNDRIVEN 致命**） |
| 3 | `examples/riscv-mini/tb_rv32i_soc.sv` | **删** `.top_ext_clk(ext_clk)` 连线 + 信号声明 + 端口映射注释（32 → 31 · 114 → 113） |
| 4 | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | **删** 2 处 `sim.set_input_value(io.ext_clk, 0u)` |
| 5 | `examples/riscv-mini/tests/{test_pipeline_wrapper,test_riscv_tests_pipeline,test_wrapper_pipeline}.cpp` | **删** 3 处 `pipeline.io().clk <<= ch_bool(true);` |
| 6 | `scripts/verify_no_duplicate_rv32i_headers.sh` | C6 端口快照 **32 → 31** |
| 7 | `.rddf/checks/port-naming-exempt.tsv` + `scripts/verify_port_naming.sh` | 豁免表删 1 行（`rv32i_pipeline.h` 的 `clk`）+ 锚点 1450 → **1449** |
| 8 | `.rddf/checks/public-api-surface.md` + `public-api-removals.tsv` | 公开面移除登记 **2 行** + 探针 **2 条**（34 → **36**）· 表行 20 → **22** |
| 9 | `docs/CHANGELOG.md` | 用户可见变更 1 条 |

### 判据与证据

| 判据 | 证据 |
|---|---|
| **零行为影响**（删前） | `ext_clk` **0/1 交替翻转** ⇒ **60 拍 × 27 观测逐拍全等**（1351 断言）⇒ ⇒ 针对性实测，非推断 |
| **删后等价** | `ctest -LE perf` **253/253**（含 SoC 双后端 golden 全绿）· `verify_examples_rtl_sim.sh` **10/10**（严格 verilator ⇒ 无 UNDRIVEN / PINMISSING） |
| **产物面** | 顶层端口 **114 → 113**（6 输入 + 107 输出）· `ext_clk` 在产物里**零命中** |
| **端口快照** | C6 PASS（31 个与快照一致）· 命名棘轮 PASS（1449 双向一致） |

### 遗留

- 「**真让 `ext_clk` 生效**」（改 codegen 时钟选择 ⇒ **全仓影响**）= **另独立立项**。
