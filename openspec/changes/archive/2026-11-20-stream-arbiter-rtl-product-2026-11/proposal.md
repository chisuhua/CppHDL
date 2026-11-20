# Proposal: stream_arbiter 进 RTL 产品面（`stream-arbiter-rtl-product`）

- **状态**: 实施中（2026-11-20 · builder）
- **用户诉求**（2026-11-20）: 「参考 examples/axi4 的改进，继续改进 examples/stream 下的示例产品化」⇒ ⇒ 本 change = **同型铺开**到 stream_arbiter（最后一个）
- **参考实现**: `examples/axi4/`（`phase3a-rtl-product-2026-11` · `axi4-verif-driven-cpp-2026-11`）· 同型先例 = `stream-mux-rtl-product-2026-11` / `stream-demux-rtl-product-2026-11` / `stream-fork-rtl-product-2026-11`
- **相关**: ADR-074（验证单一真相源）· ADR-072（产品端口面 · 四件套）· ADR-075（端口具名 E1）· capability `chlib-stream-arbitration`（轮转 / 无饿死）· `chlib-stream-ready-ownership`（ready 归属）· capability `examples-rtl-sim-coverage`

---

## Why

`examples/stream/stream_arbiter_demo.cpp` 是「`main()` + 内联激励 + `std::cout` 比对」的形态：

1. **没有设计定义与激励的分离** ⇒ ⇒ 同一份设计无法被 `.verif` / 独立 Verilog TB 复用；
2. **没有 RTL 产物** ⇒ ⇒ 「同一份设计在 Verilog 产物上的行为」**零证据**；
3. **期望值散落在 `main()` 里** ⇒ ⇒ 换个后端就要重写一份激励。

⇒ ⇒ 本 change 把 stream_arbiter 做成与前三个同型的「产品单元 = 四件套」
（设计定义 · 独立 TB · 产物 · 登记）⇒ ⇒ **`examples/stream/` 至此 4/4 全部产品化**。

⚠️ **落选表的归位条件已满足**：该行的 `return_condition` = 「待 `chlib-stream-arbitration`
的判据口径稳定后补」⇒ ⇒ 该 capability（2026-11-01 / 11-03 两轮收口后）已把判据口径
写清（**效果属性** + **相位不入契约**）⇒ ⇒ 条件成立。

⚠️ **与前三个示例的本质差异**：arbiter 是**时序**的（内部 `last_grant_reg` 轮转反馈寄存器）
⇒ ⇒ 判据 MUST 用 `TICK` / `WAIT` 推进拍 ⇒ ⇒ 且**相位**（首拍授予谁）取决于实现的
状态初值 ⇒ ⇒ capability 明文「MUST NOT 要求某一具体相位」⇒ ⇒ 判据 MUST 以
**效果属性**为主（见下）。

## What Changes

### ① `examples/stream/stream_arbiter_top.h`（**新增** · 设计定义）

`chlib::stream_arbiter_round_robin<ch_uint<8>, 4>` 的具名端口包装：4 路输入流
{payload, valid, ready} + 输出流 {payload, valid, ready} + `selected[1:0]` =
**16 根根自有端口**（全部具名 ⇒ 产物端口名 = `top_<成员名>`）。

### ② `stream_arbiter_demo.cpp` → `tb_stream_arbiter_demo.cpp`（**改名 + 双模式**）

- **默认**（无参）= 验证模式：读 `.verif` → IR 解释器 → `RESULT: checks=<N> fails=0`
- **`--emit-rtl <dir>`** = 发射模式：复用库层 `strip_mode_flags` / `require_out_dir` /
  `run_emit_cli`（与前三者同型）

### ③ `tb_stream_arbiter_demo.verif` + `tb_stream_arbiter_demo.sv`（**新增**）

5 个 scenario / **38** 条断言（`single_valid_channel_held` 13 · `two_channels_alternate` 6 ·
`four_channels_all_served` 8 · `backpressure_stalls_upstream` 5 · `no_valid_no_grant` 6）
⇒ ⇒ `.sv` 由 `.verif` 渲染（`generated=yes` · 禁手改）。

### ④ 登记面

| 项 | 前 | 后 |
|---|---|---|
| `.rddf/checks/examples-rtl-sim-targets.tsv` | 无 `stream_arbiter` 行 | 新增 `tb_stream_arbiter_demo`（`kind=target` · 自发行 · `interp_checks=38` · `min_checks=38` · `generated=yes` · `scenarios=5` · `spec_checks=37`） |
| `# REGISTERED_TARGETS` | 14 | **15** |
| `.rddf/checks/examples-rtl-sim-exclusions.tsv` | 有 `stream_arbiter_demo` 行 | **删该行**（落选 13 → 12）· 守恒订正为 `27 == 15 ∪ 12` |
| `examples-rtl-expectations.md` | 无 stream-arbiter 段 | 新增 `## tb_stream_arbiter_demo {#stream-arbiter-demo}`（手推 · `VERIF-CHECKS: 38`） |

### ⑤ 文档与构建

- `examples/stream/AGENTS.md`（进度 4/4 + 四个示例的语义面对照表）· 根 `AGENTS.md` 的示例路径
- `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS`（改名）·
  `examples/stream/CMakeLists.txt`（`CPPHDL_SOURCE_DIR` 注入 + include dir）

### ⑥ 🔴 库侧 codegen 缺陷修复（`src/codegen_verilog.cpp` · **本产品的前置条件**）

arbiter 是**第一个被发射**的轮转组合子 ⇒ ⇒ 它的 RTL 产物**编译不过**
（Verilator `WIDTHTRUNC` × 5）⇒ ⇒ 根因 = hygiene 模式**只把操作数宽度**写成显式 cast，
**结果宽度没对齐**：

- 框架按「`mod` 的 RHS 是字面量 ⇒ 结果宽 = `compute_bit_width(N-1)`」定宽（= 2 bit）
- 而 Verilog 的运算自然宽 = `max(两侧)` = 3 bit ⇒ ⇒ 赋值处**隐式截断** ⇒ ⇒ 告警

⇒ ⇒ 修法 = **把截断写成显式 cast**（`assign x = 2'(a % 3'h4);`）⇒ ⇒
**语义逐位不变**（显式截断 ≡ 隐式截断）⇒ ⇒ 只在「运算自然宽 > 节点声明宽」时补
⇒ ⇒ **其余算子的产物文本一字不变**（比较族 / 位运算 / 加减的自然宽 == 声明宽）。

## Capabilities

**无 spec 变更** ⇒ ⇒ 归档用 `openspec archive … --yes --skip-specs`。
既有 capability `examples-rtl-sim-coverage`（四件套）与 `chlib-stream-arbitration`
（轮转 / 无饿死 / 唯一有效通道 / 相位不入契约）+ `chlib-stream-ready-ownership`
（ready 归属）**已覆盖**本 change 的判据面 ⇒ ⇒ 不新增也不修改 Requirement。

⚠️ **本 change MUST NOT 把「相位」写进产品契约**（capability 明文）⇒ ⇒
期望源里的逐拍字面量只出现在**判别力用例**（`tests/chlib/test_stream.cpp` 的
`[rotation]`）⇒ ⇒ 产品面 TB 用**效果属性**表达（见 ③）。

## Impact

| 面 | 变化 |
|---|---|
| 代码 | `examples/stream/stream_arbiter_top.h`（新）· `tb_stream_arbiter_demo.cpp`（由 `stream_arbiter_demo.cpp` 改造）· `tb_stream_arbiter_demo.{verif,sv}`（新） |
| **库** | `src/codegen_verilog.cpp`（hygiene 的**结果宽度**对齐 · 语义零变化） |
| 构建 | target 改名 `stream_arbiter_demo` → `tb_stream_arbiter_demo`（总数不变 · 仍 27） |
| 清单 | 覆盖表 +1 行 · 落选表 −1 行 · 锚点 14 → 15 |
| 文档 | `examples/stream/AGENTS.md` · 根 `AGENTS.md` 一行 · 期望源一段 · `docs/CHANGELOG.md` |

## Out of Scope

- **不动 `chlib::stream_arbiter_round_robin` 的实现**（本 change 只做示例层产品化；
  codegen 的修复是「产物能编译」的前置，**不改**仲裁语义）。
- **不进 `--check-products`**（本设计无 `ch_layer` ⇒ 层次化 = 扁平 ⇒ 该自检无判别力）。
- **不改 capability `chlib-stream-arbitration`**（相位口径**已**明确，本 change 只是遵守它）。
- **不修 `verify_hierarchical_p8.sh` 的既有红**（该 guard 的活账引用了
  `include/axi4/axi4_lite_example_top.h` —— 该头在 `example-file-boundary-2026-11`
  （commit `08298a0`）已移走 ⇒ ⇒ **本次会话前既有**，与本 change 无关 ⇒ ⇒ 只登记）。

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `stream_arbiter` 示例的 RTL 产物 | 不存在（只有解释器 demo） | `--emit-rtl` 产出 `top.v` + `manifest.json`，并被独立 Verilog TB 验证（`RESULT: checks=38 fails=0`） |
| 示例二进制名 | `build/examples/stream/stream_arbiter_demo` | `build/examples/stream/tb_stream_arbiter_demo` |
| `stream_arbiter` 的激励/期望 | 散落在 `main()` 的 `std::cout` 比对里 | 收敛到 `.verif`（唯一语义来源）⇒ ⇒ 解释器 / Verilog 两侧同源 |
| **库**：hygiene 产物的宽度错位 | 隐式截断 ⇒ ⇒ Verilator 报 `WIDTHTRUNC` ⇒ ⇒ 卫生产物编译失败 | 显式截断 ⇒ ⇒ 零告警（**语义不变**） |

---

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `stream_arbiter_top.h` 的 **16 根端口全部具名** ⇒ 产物端口名 = `top_<成员名>`（无 `top_unnamed_*`） | 发射 + 护栏 ⑧⁗ |
| AC2 | `tb_stream_arbiter_demo --emit-rtl --out <dir>` ⇒ exit 0 + 产出 `top.v` + `manifest.json` | shell |
| AC3 | 产物 **module 集 == {top}**（1 个）· **顶层端口 == 18**（框架 2 + 根自有 16 · 手推） | 护栏 ⑧ |
| AC4 | 独立 TB `tb_stream_arbiter_demo.sv` 编译通过 · 跑通 ⇒ `RESULT: checks=38 fails=0` | 护栏 ④⑤⑥ |
| AC5 | 清单新增该行 · `REGISTERED_TARGETS == 15` · coverage 守恒 `27 == 15 ∪ 12` | 护栏 + awk |
| AC6 | 期望源段存在且含 **module 集 / 端口数 / 38 条**三项手推登记 · `VERIF-CHECKS: 38` | 读文件 + 护栏 ⑥'' |
| AC7 | `interp_checks == 38`（C++ 侧）== `min_checks`（TB 侧）⇒ 两侧同源 | awk + 护栏 ⑨ |
| AC8 | 默认模式（无参）只跑 `.verif` ⇒ `RESULT: checks=38 fails=0`；未知参数 ⇒ exit 2 | shell |
| AC9 | `verify_examples_rtl_sim.sh` PASS · coverage PASS · port_naming PASS · `run_all_ported_tests.sh` PASS · `ctest` 全绿 | shell |
| AC10 | `openspec validate <change> --strict` valid（无 spec 变更 ⇒ `--skip-specs` 归档） | openspec |
| AC11 | **判别力自证 ≥ 3 条**变异全部转红（含 M3 = 轮转反馈恒 0 ⇒ **历史缺陷形态**） | 临时产物 + Verilator |
| AC12 | **相位不入契约**：期望源 + `.verif` **不锁**首拍授予谁 ⇒ ⇒ 判据写成效果属性（`WAIT` + 紧界） | 读 `.verif` + 期望源 |
| AC13 | **codegen 修复**：hygiene 产物**零 `WIDTHTRUNC`** ⇒ ⇒ 其余算子的产物文本**一字不变**（既有 16 个产品护栏全绿 + `verify_flat_rtl_emit.sh` PASS） | Verilator 编译 + 护栏 |
