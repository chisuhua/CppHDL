# examples 的 RTL 仿真覆盖：**落选名单 + 未覆盖项**（`examples-rtl-sim-coverage` **E4′**）

> **为什么必须有这份文档**：E4 的核心风险是「**先做 10 个 ⇒ 变成永远只有 10 个**」
> ⇒ ⇒ 「没做」必须**留痕**：谁没做、为什么、什么条件下归位。
> **权威名单** = `.rddf/checks/examples-rtl-sim-targets.tsv`（E4 的载体）· **本文件** = 落选理由与未覆盖项。

---

## 1. 入选准则（E4）

| # | 准则 |
|---|---|
| ① | 有**可观测语义**（输出随输入变化） |
| ② | 覆盖**不同结构类别**（组合 / 时序 / 流控 / 总线 / CPU） |
| ③ | C++ 侧已有**断言质量**合格的测试（用于交叉参考，**不作为** RTL 侧期望源） |
| ④ | RTL 规模适中（verilator 编译 ≲ 10 s） |

⇒ ⇒ **首批 10 项**（TSV 的权威名单）覆盖类别：
组合（`counter` · `counter_simple` · `pwm` · `gpio`）·
时序状态机（`uart_tx` · `uart_rx`）· 流控（`fifo_stream`）·
计数/相位（`quadrature_encoder`）· 总线（`axi4_lite`）· CPU（`riscv-mini` smoke）。

## 2. 落选名单（`examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` 共 **19** 项 ⇒ 首批选 9 项 + 1 个 `kind=dut` ⇒ **落选 10** 项）

| 落选 target | 类别 | 落选理由（有据） | 归位条件 |
|---|---|---|---|
| `spi_controller_example` | 时序状态机 | **类别已覆盖**（`uart_tx`/`uart_rx` 同为时序 FSM）· 且它是 **SPI 主控 + 完整帧协议**（源码头注释）⇒ 期望源面比 UART 大 | 首批扩到第二批时补（协议面独立 ⇒ 需单独推导帧格式） |
| `i2c_controller_example` | 时序状态机 | 同上 · 源码头自述「**简化验证版**」⇒ ⇒ 其行为**不是完整 I2C** ⇒ ⇒ 按它写期望源会把「简化」固化成契约 | 若该 example 被补成完整 I2C（START/STOP + ACK）后再补 TB |
| `ws2812_controller_example` | 时序（LED 编码） | 类别已覆盖（时序 FSM）· 且 WS2812B 的**位时序 + 颜色打包**是另一套外部协议 ⇒ 期望源需另推 | 第二批（与 `samples/` 的独立立项一起评估） |
| `vga_controller_example` | 时序（640x480@60Hz） | 类别已覆盖 + **规模大**（整帧时序生成 ⇒ 违反准则 ④「编译 ≲ 10 s」）⇒ ⇒ 编译成本会挤占 CI 墙钟 | 需要单独测墙钟预算后再评估 |
| `timer_example` | 时序（预分频 + reload） | 类别已覆盖（时序）· 且它的语义**已被** `quadrature_encoder`（计数/相位）部分覆盖 | 一般不补（增量收益低） |
| `sigma_delta_dac_example` | 组合 + 滤波器 | 类别已覆盖（组合）· 且 σ-Δ 的判据本质是**噪声/精度统计**⇒ ⇒ 不适合「离散拍级」断言 | 若要补 ⇒ 需另立「统计型判据」的规范口径 |
| `width_adapter_example` | 流控（4×8→32 位拼接） | 类别已覆盖（流控：`fifo_stream`）· 语义面最窄（一个移位寄存器）⇒ ⇒ 边际收益最低 | 一般不补 |
| `assert_example` | **框架自身** | 它验证的是**断言机制**（源码头：「断言系统使用示例」）⇒ ⇒ 不是「被测设计」⇒ ⇒ RTL 侧的期望源是**框架语义**而非设计语义 | 若断言机制需要**独立 RTL 面** ⇒ 另立立项 |
| `axi4_full_example` | 总线（AXI4 **Full**） | AXI4-Lite 已被 `axi4_lite` 覆盖；**Full** 的 burst / 拆包 / DECERR 语义由**独立 capability** `axi4-full-slave-burst` 承载 ⇒ ⇒ 期望源面显著更大 ⇒ ⇒ 首批准备不足 | 待 `axi4-full-slave-burst` 的判据口径稳定后补 |
| `axi_soc_example` | SoC 集成 | 与 `riscv-mini`（SoC）**同类重复** · 且它是「AXI 矩阵 + GPIO + UART + Timer」的**集成体** ⇒ ⇒ 规模更大、失败定位更难 | 若 `riscv-mini` 的 SoC 已被覆盖 ⇒ 一般不补 |

> ⚠️ **落选 ≠ 不做**：本表是「首批不做」的登记，**不是**永久排除。
> 每行都带**归位条件** ⇒ ⇒ 「新增即漏」被堵住。

## 3. 首批 10 项的**未覆盖项**（同样必须留痕）

| target | 未覆盖 | 原因 |
|---|---|---|
| `riscv-mini-rv32i-soc` | ① **minitest 固件全流程**（178 条退休、`sw` 落 tohost 的真实判定） | 外部 TB 装载 minitest 会让期望值依赖**仓内金标** ⇒ ⇒ B3 的 T5 要重算它 ⇒ ⇒ 硬判必在 B3 落地时转红 ⇒ ⇒ **改用 TB 自己的小程序**（判据全由 ISA 语义手推） |
| | ② **CSR / `ECALL` / `EBREAK` / `FENCE`** | 仓内 `AGENTS.md` KNOWN LIMITATIONS 明写「**不在任何仓内 capability 覆盖内**」⇒ ⇒ 需独立立项 |
| | ③ 分支预测 / mispredict 统计 | `obs_perf_branch_mispredict` 已暴露，但**期望源**（预测器契约）未定义 ⇒ ⇒ 待 ISA/预测器 capability 收口 |
| `uart_rx_example` | ① **停止位错误 ⇒ 不产生 `valid`**（framing check） | 本实现**不校验停止位** ⇒ ⇒ 判它 = 要求一个没有的能力 ⇒ ⇒ 已登记为已知偏离 |
| `uart_rx_example` | ② 与 `uart_tx` 的**真实 loopback**（tx→rx 直连） | tx 分频固定 433 ⇒ 与 rx 的 `prescale` 需匹配（`prescale=432`）⇒ ⇒ 已作为**备选判据**记录，未纳入（避免引入跨 example 的耦合） |
| `axi4_lite_example` | ① **AXI4 Full**（burst / 拆包） | 由独立 capability `axi4-full-slave-burst` 承载（见上表） |
| `fifo_stream_example` | ① **满后再写**（自保护） | 本手写 demo **无空/满门控** ⇒ ⇒ 那是库组件 `chlib::sync_fifo` 的契约（`chlib-stream-semantics` 的自保护条款）⇒ ⇒ **不适用**于本 example |
| `counter_example` / `counter_simple_example` | ① `enable`/`clear` 的**完整语义** | example 源码自认「`enable/clear may not fully work yet`」⇒ ⇒ 见「D4：interpreter 与 RTL 是否分叉」的独立调查 |
| `gpio_example` | ① **上游 Scala 的逐位边沿语义** | 上游 Scala 与 C++ 实现语义**实质不同**（1 级 vs 2 级同步 / 单 bit vs 逐位边沿 / 多两端口）⇒ ⇒ 只判 impl-contract ⇒ ⇒ `spec_checks = 0` ⇒ ⇒ **待用户裁决**（有意扩展 vs 移植缺陷） |

## 4. 墙钟（E6 的 CI 增量）

| 场景 | 实测（本地 · 2026-11-12） |
|---|---|
| **warm**（`~/.cache/cpphdl/rtl_sim` 命中） | **3 s** |
| **cold**（清缓存 ⇒ 10 个 TB 全量编译） | **61 s** |

⇒ ⇒ CI 的 E6 step（`pr-verilator-fast` 的一个 step · **不新建 job**）增量
⇒ ⇒ **cold ≈ 1 min** / **warm ≈ 3 s**（不含 build）⇒ ⇒ 远小于该 job 的 `timeout-minutes: 45`。
⇒ ⇒ 台账：`.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md`（E6 段）。
