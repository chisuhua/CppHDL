# examples 的 RTL 仿真覆盖：**落选名单 + 未覆盖项**（`examples-rtl-sim-coverage` **E4′**）

> **为什么必须有这份文档**：E4 的核心风险是「**先做 10 个 ⇒ 变成永远只有 10 个**」
> ⇒ ⇒ 「没做」必须**留痕**：谁没做、为什么、什么条件下归位。
> **权威名单** = `.rddf/checks/examples-rtl-sim-targets.tsv`（E4 的载体 · **已覆盖**）·
> **落选名单（机器可读）** = `.rddf/checks/examples-rtl-sim-exclusions.tsv`（**没做的**）·
> **本文件** = 入选准则 + 首批的未覆盖项 + 墙钟实测。

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

⚠️ **TSV 里另有两个不计入「首批 10」的条目**（各自独立桶 · 见 tsv 的 `REGISTERED_SOC` /
`REGISTERED_HIER` 锚点）：`axi_soc_example`（`kind=soc` · SoC 层产物）与
`riscv-mini-rv32i-soc-hier`（`kind=hier` · **同一设计的层次化形态** ⇒ 复用同一份独立 TB）。
⇒ ⇒ 守恒等式 `28 == 10 ∪ 18` **只**作用于「首批 10」⇒ ⇒ 这两个桶不参与 ✅

## 2. 落选名单（**机器可读** = `.rddf/checks/examples-rtl-sim-exclusions.tsv`）

`examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` 共 **28** 项 ⇒ ⇒
**已覆盖 10** 项（覆盖表里 `kind ∈ {target, soc}`：首批 9 个 `target` + SoC 1 个 `soc`）
⇒ ⇒ **落选 18** 项 ⇒ ⇒ 逐行的**理由 + 归位条件**登记在
**`.rddf/checks/examples-rtl-sim-exclusions.tsv`**。

⚠️ **本文不复制那张表** ⇒ ⇒ 落选名单**只有一份**（单一真相）⇒ ⇒ 杜绝两份漂移。

> ⚠️ **落选 ≠ 不做**：该表是「这批不做」的登记，**不是**永久排除。
> 每行都带**归位条件** ⇒ ⇒ 「新增即漏」被堵住。
> ⚠️ **守恒是机械判据**（不再靠本文的散文承诺）：`scripts/verify_examples_rtl_sim_coverage.sh`
> ①（`28 == 10 ∪ 18` 且三者互斥）⇒ ⇒ **漏登记即 CI 红**。
> ⚠️ **2026-11-17 订正**：本文此前写「共 **19** 项 ⇒ 落选 **10**」且表里只有 **9** 行 ⇒ ⇒
> 被跟踪集合从 19 涨到 28 后，**9 个 example 既没进覆盖表、也没进落选表** ⇒ ⇒
> 「新增即漏」**当时已经失效** ⇒ ⇒ 本次按 28 项重算并补登
> （变更 = `examples-rtl-sim-registration-hygiene-2026-11`）。

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
