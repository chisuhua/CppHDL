# Proposal: stream_demux 进 RTL 产品面（`stream-demux-rtl-product`）

- **状态**: 实施中（2026-11-20 · builder）
- **用户诉求**（2026-11-20）: 「参考 examples/axi4 的改进，继续改进 examples/stream 下的示例产品化」⇒ ⇒ 先做 stream_mux 试点（已完成），本 change = **同型铺开**到 stream_demux
- **参考实现**: `examples/axi4/`（`phase3a-rtl-product-2026-11` · `axi4-verif-driven-cpp-2026-11`）· 同型先例 = `stream-mux-rtl-product-2026-11`
- **相关**: ADR-074（验证单一真相源）· ADR-072（产品端口面 · 四件套）· ADR-075（端口具名 E1）· ADR-056（demux ready 方向）· capability `chlib-stream-ready-ownership` / `examples-rtl-sim-coverage`

---

## Why

`examples/stream/stream_demux_demo.cpp` 是「`main()` + 内联激励 + `std::cout` 比对」的形态：

1. **没有设计定义与激励的分离** ⇒ ⇒ 同一份设计无法被 `.verif` / 独立 Verilog TB 复用；
2. **没有 RTL 产物** ⇒ ⇒ 「同一份设计在 Verilog 产物上的行为」**零证据**（只有解释器路径）；
3. **期望值散落在 `main()` 里** ⇒ ⇒ 换个后端就要重写一份激励。

⇒ ⇒ 本 change 把 stream_demux 做成与 mux 同型的「产品单元 = 四件套」
（设计定义 · 独立 TB · 产物 · 登记）⇒ ⇒ 后续 arbiter / fork 按同一形态铺开。

⚠️ **demux 与 mux 的语义面差异**（⇒ ⇒ 判据面**不重叠**）：
mux 判「多选一」，demux 判「一分发 + **反压方向**」⇒ ⇒
`in.ready` 由**被选中输出**的 ready 驱动（ADR-056 D1）⇒ ⇒ 与 mux 的
「`in[i].ready` 由 `out_ready` 扇出」**方向相反** ⇒ ⇒ 独立判据面（非重复覆盖）。

## What Changes

### ① `examples/stream/stream_demux_top.h`（**新增** · 设计定义）

`chlib::stream_demux<ch_uint<8>, 4>` 的具名端口包装：输入流 {payload, valid, ready}
+ `select[1:0]` + 4 路输出流 {payload, valid, ready} = **16 根根自有端口**（全部具名 ⇒
产物端口名 = `top_<成员名>`）。

### ② `stream_demux_demo.cpp` → `tb_stream_demux_demo.cpp`（**改名 + 双模式**）

- **默认**（无参）= 验证模式：读 `.verif` → IR 解释器 → `RESULT: checks=<N> fails=0`
- **`--emit-rtl <dir>`** = 发射模式：复用库层 `strip_mode_flags` / `require_out_dir` /
  `run_emit_cli`（与 mux 同型）

### ③ `tb_stream_demux_demo.verif` + `tb_stream_demux_demo.sv`（**新增**）

4 个 scenario / **38** 条断言（`reset_defaults` 6 · `payload_follows_select` 23 ·
`backpressure_reaches_upstream` 4 · `no_valid_all_outputs_idle` 5）⇒ ⇒
`.sv` 由 `.verif` 渲染（`generated=yes` · 禁手改）。

### ④ 登记面

| 项 | 前 | 后 |
|---|---|---|
| `.rddf/checks/examples-rtl-sim-targets.tsv` | 无 `stream_demux` 行 | 新增 `tb_stream_demux_demo`（`kind=target` · 自发行 · `interp_checks=38` · `min_checks=38` · `generated=yes` · `scenarios=4` · `spec_checks=32`） |
| `# REGISTERED_TARGETS` | 12 | **13** |
| `.rddf/checks/examples-rtl-sim-exclusions.tsv` | 有 `stream_demux_demo` 行 | **删该行**（落选 15 → 14）· 守恒订正为 `27 == 13 ∪ 14` |
| `examples-rtl-expectations.md` | 无 stream-demux 段 | 新增 `## tb_stream_demux_demo {#stream-demux-demo}`（手推 · `VERIF-CHECKS: 38`） |

### ⑤ 文档与构建

- `examples/stream/AGENTS.md` 进度行 · 根 `AGENTS.md` 的示例二进制路径
- `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS`（改名）·
  `examples/stream/CMakeLists.txt`（`CPPHDL_SOURCE_DIR` 注入 + include dir）

## Capabilities

**无 spec 变更** ⇒ ⇒ 归档用 `openspec archive … --yes --skip-specs`。
既有 capability `examples-rtl-sim-coverage` 的「登记 + 期望源 + 三方等式 + 端口表四件套」
与 `chlib-stream-ready-ownership` 的「分流 / 分叉类」条**已覆盖**本 change 的判据面 ⇒ ⇒
不新增也不修改 Requirement。

⚠️ **「越界 ⇒ 挡住上游」条在本设计不可达**（`select` 2 bit 且 `N = 4` ⇒ 恒在范围内）
⇒ ⇒ **如实登记**在期望源段（不假装覆盖）。

## Impact

| 面 | 变化 |
|---|---|
| 代码 | `examples/stream/stream_demux_top.h`（新）· `tb_stream_demux_demo.cpp`（由 `stream_demux_demo.cpp` 改造）· `tb_stream_demux_demo.{verif,sv}`（新） |
| 构建 | target 改名 `stream_demux_demo` → `tb_stream_demux_demo`（总数不变 · 仍 27） |
| 清单 | 覆盖表 +1 行 · 落选表 −1 行 · 锚点 13 → 14 |
| 文档 | `examples/stream/AGENTS.md` · 根 `AGENTS.md` 一行 · 期望源一段 |

## Out of Scope

- **arbiter / fork 不在本 change 内**（各自独立 change）⇒ ⇒ 仍留在落选表。
- **不进 `--check-products`**（本设计无 `ch_layer` ⇒ 层次化 = 扁平 ⇒ 该自检无判别力）。
- **不动 `chlib::stream_demux` 的实现**（本 change 只做示例层产品化）。
- **不改 capability `chlib-stream-ready-ownership`**（其「越界」条在本设计不可达 ⇒
  **如实登记**，不是缺陷）。

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `stream_demux` 示例的 RTL 产物 | 不存在（只有解释器 demo） | `--emit-rtl` 产出 `top.v` + `manifest.json`，并被独立 Verilog TB 验证（`RESULT: checks=38 fails=0`） |
| 示例二进制名 | `build/examples/stream/stream_demux_demo` | `build/examples/stream/tb_stream_demux_demo` |
| `stream_demux` 的激励/期望 | 散落在 `main()` 的 `std::cout` 比对里 | 收敛到 `.verif`（唯一语义来源）⇒ ⇒ 解释器 / Verilog 两侧同源 |

---

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `stream_demux_top.h` 的 **16 根端口全部具名** ⇒ 产物端口名 = `top_<成员名>`（无 `top_unnamed_*`） | 发射 + 护栏 ⑧⁗ |
| AC2 | `tb_stream_demux_demo --emit-rtl --out <dir>` ⇒ exit 0 + 产出 `top.v` + `manifest.json` | shell |
| AC3 | 产物 **module 集 == {top}**（1 个）· **顶层端口 == 18**（框架 2 + 根自有 16 · 手推） | 护栏 ⑧ |
| AC4 | 独立 TB `tb_stream_demux_demo.sv` 编译通过 · 跑通 ⇒ `RESULT: checks=38 fails=0` | 护栏 ④⑤⑥ |
| AC5 | 清单新增该行 · `REGISTERED_TARGETS == 13` · coverage 守恒 `27 == 13 ∪ 14` | 护栏 + awk |
| AC6 | 期望源段存在且含 **module 集 / 端口数 / 38 条**三项手推登记 · `VERIF-CHECKS: 38` | 读文件 + 护栏 ⑥'' |
| AC7 | `interp_checks == 38`（C++ 侧）== `min_checks`（TB 侧）⇒ 两侧同源 | awk + 护栏 ⑨ |
| AC8 | 默认模式（无参）只跑 `.verif` ⇒ `RESULT: checks=38 fails=0`；未知参数 ⇒ exit 2 | shell |
| AC9 | `verify_examples_rtl_sim.sh` PASS · coverage PASS · port_naming PASS · `run_all_ported_tests.sh` PASS | shell |
| AC10 | `openspec validate <change> --strict` valid（无 spec 变更 ⇒ `--skip-specs` 归档） | openspec |
| AC11 | **判别力自证 ≥ 3 条**变异全部转红（valid 通路 / 反压方向 / payload 广播） | 临时产物 + Verilator |
