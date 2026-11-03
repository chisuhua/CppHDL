---
kind: registry
subject: input-port-required-registry
evidence: probe:scripts/verify_input_port_required_registry.sh
---

# 登记表：**必需端口**（未驱动 ⇒ 静默失效）的分诊表（D2-A · 2026-11-03 裁决）

> ⚠️ **为什么要有它**：根 `AGENTS.md §ANTI-PATTERNS` 登记的义务「**新增**组件输入
> 端口 ⇒ 全部实例化点 MUST 显式驱动」只覆盖**新增**端口 ⇒ ⇒ **存量**端口里那些
> 「未驱动就静默失效」的**没有**任何护栏 ⇒ ⇒ 而 2026-10-28 的 spike 结论是
> 「**通用状态型护栏不可行**」（运行期无法识别端口身份 ⇒ 诊断不可行动）⇒ ⇒
> 剩下的可行路线 = **先分诊，再对「已知必需端口」立护栏**。
>
> ⚠️ **本表只收两类风险**（**裁决 D2-A** ⇒ 只收**已有实测证据**的两类 ⇒ ⇒
> **不做全仓普查**：B8 的教训 = 「全仓普查义务不可达成 ⇒ 落地即全红 ⇒ 必被放弃」）。
>
> ⚠️ **本表是组件清单的**唯一真相源** ⇒ ⇒ `scripts/verify_rst_n_wiring.sh`
> 的组件清单**从本表读**（不再硬编码）⇒ ⇒ 消除「两处清单漂移」这个**静默失效**面。

## 风险等级（两类）

| 等级 | 未驱动的后果 | 实测证据 |
|---|---|---|
| **永久复位** | `ch_in<ch_bool> rst_n` 读回 **0** ⇒ 组件**恒在复位** ⇒ 写永不落地、读恒 0 ⇒ ⇒ **整个测试空洞**（编译过、仿真过、exit 0）| `tests/test_axi_rst_n_guard.cpp`（双后端）—— 故意不驱动 `rst_n` ⇒ 断言「无 B/R 响应 + 寄存器保持初值」|
| **写入放大** | `ch_in<ch_uint<4>> strbe` 读回**全 1** ⇒ 写路径按**整字**落盘 ⇒ ⇒ **碰巧通过**（断言只看最终字）⇒ ⇒ 跨后端行为可能分叉 | `include/cpu/pipeline/rv32i_tcm.h` 的 `strbe` 注释 + `include/cpu/cache/i_cache_prefetch.h:104` 的登记 + capability `rv32i-isa-semantics` |

## 登记表（`组件` · `端口` · `风险等级` · `判别性用例`）

| 组件 | 端口 | 风险等级 | 判别性用例 |
|---|---|---|---|
| `AxiLiteSlave` | `rst_n` | 永久复位 | `tests/test_axi_lite_slave_conformance.cpp` |
| `Axi4Slave` | `rst_n` | 永久复位 | `tests/test_axi4_full_burst.cpp` |
| `AxiToAxiLiteConverter` | `rst_n` | 永久复位 | `examples/axi4/phase3a_complete.cpp` |
| `AxiLiteGpio` | `rst_n` | 永久复位 | `tests/test_axi_gpio_behavior.cpp` |
| `AxiLiteUart` | `rst_n` | 永久复位 | `tests/test_axi_uart_behavior.cpp` |
| `AxiLiteTimer` | `rst_n` | 永久复位 | `tests/test_axi_timer_behavior.cpp` |
| `AxiLitePwm` | `rst_n` | 永久复位 | `tests/test_axi_pwm_behavior.cpp` |
| `AxiLiteI2c` | `rst_n` | 永久复位 | `tests/test_axi_i2c_behavior.cpp` |
| `AxiLiteSpi` | `rst_n` | 永久复位 | `tests/test_axi_spi_behavior.cpp` |
| `AxiDma` | `rst_n` | 永久复位 | —（**已知缺口**，见下） |
| `AxiInterconnect4x4` | `rst_n` | 永久复位 | `tests/test_axi_interconnect_4x4.cpp` |
| `AxiLiteMatrix2x2` | `rst_n` | 永久复位 | `tests/test_axi_lite_matrix_2x2.cpp` |
| `DataTCM` | `strbe` | 写入放大 | `tests/test_rv32i_timing_dual.cpp` |

## ⚠️ 已知缺口（**如实登记** ⇒ 不编造判别性用例）

| 组件 | 端口 | 现状 | 为什么不是笔误 |
|---|---|---|---|
| `AxiDma` | `rst_n` | **零行为覆盖**：`tests/test_axi_dma.cpp` 只有**构造 + 端口存在性**检查（`AxiDma<32,32,4,16> dma(nullptr,"dma");`）⇒ 该文件 **无 `Simulator` 实例**、**无任何 `rst_n` 驱动** | 这是 `grep` 实测（`Simulator` 命中 0 / `rst_n` 命中 0），**不是**「暂时没找到」。⇒ ⇒ 若将来补上行为用例，本行 MUST 改成真实指针（R3 会因「表已过时」而**转红**）|

⚠️ **本表拒绝「编造指针」**：把 `tests/test_axi_dma.cpp` 写成 `AxiDma` 的判别性用例会让 R3 **假绿**
（该文件确实提到组件名）⇒ ⇒ 那正是本表要治的病（`chlib` 的 `fwft_fifo` / `sequence` 同型）。

## 通用豁免机制（**窄 + 可见**）

形如：

```
// input_port_guard: intentionally-undriven <port>
```

- **窄**：文件名 MUST 含 `guard`（与既有 `rst_n_guard: intentionally-unwired` 同一纪律
  ⇒ ⇒ 往普通测试文件塞标记**不豁免**）；
- **可见**：CI MUST 打印 `::notice` 点名被豁免的文件（豁免**不是**静默的）；
- **旧标记兼容**：`rst_n_guard: intentionally-unwired` 继续有效（既有护栏在用）。

## 两条护栏的分工（**单一实现点**）

| 护栏 | 负责 | 不负责 |
|---|---|---|
| `scripts/verify_rst_n_wiring.sh` | **永久复位**类的**驱动检测**（per-TEST-CASE 粒度 + wrapper 转发 + helper 识别）| 不负责「哪些端口是必需的」（= 本表）|
| `scripts/verify_input_port_required_registry.sh` | **本表的双向校验**（登记 ⊆ 实际端口 · 实际端口 ⊆ 登记 · 判别性用例存在 · **写入放大**类的 file 级驱动网）+ **写入放大**类的驱动检测 | 不重复实现「永久复位」类的 per-case 检测（⇒ 避免两份实现漂移）|
