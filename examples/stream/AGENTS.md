# AGENTS.md - Stream Component Examples

Child of root `AGENTS.md`.

## OVERVIEW
4 个流控组合子示例。**产品化进度 = 1/4**（`stream_mux` 已进 RTL 产品面；
demux / arbiter / fork 仍是纯 demo —— 见 §PATTERNS 的归位条件）。

## FILES
| File | Focus | CTest |
|------|-------|-------|
| `stream_mux_top.h` | Stream Mux **设计定义**（4 选 1 · **无 `main`** · 16 根具名端口） | — |
| `tb_stream_mux_demo.cpp` | Stream Mux **激励驱动**（默认由 `.verif` 驱动 · `--emit-rtl` 发射 · `RESULT: checks=24`） | ✅（`run_all_ported_tests.sh`） |
| `tb_stream_mux_demo.verif` | **验证源**（`.verif` · 4 场景 / 24 断言 · **单一语义来源**） | — |
| `tb_stream_mux_demo.sv` | **由 `.verif` 生成**的独立 TB（`generated=yes` · 禁手改） | — |
| `stream_demux_demo.cpp` | Demux 演示（1 → 4 · **未产品化**） | ✅ |
| `stream_arbiter_demo.cpp` | Round-robin 仲裁演示（**未产品化**） | ✅ |
| `stream_fork_demo.cpp` | Fork 演示（1 → 3 · **未产品化**） | ✅ |

## PATTERNS
- **三角色分离**（`example-file-boundary-2026-11`）：设计定义 `*_top.h`（**无 `main`**）
  → 激励驱动 `tb_*.cpp`（`main`：加载 `.verif` + `--emit-rtl` 发射）→ 激励源 `tb_*.{verif,sv}`
- **产品面登记**（`stream-mux-rtl-product-2026-11`）：`tb_stream_mux_demo` 已进
  `.rddf/checks/examples-rtl-sim-targets.tsv`（`kind=target` · 自发行 · `generated=yes`）
  ⇒ ⇒ 期望源 = `.rddf/checks/examples-rtl-expectations.md#stream-mux-demo`（**手推**）
  ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`（16 根根自有端口全部具名）
- **组合子契约**（capability `chlib-stream-ready-ownership`）：调用方 MUST 先
  `as_master()` 建字段节点再传给组合子；ready 链 MUST 用 `<<=`（禁用 `operator=`）
- **归位条件**（demux / arbiter / fork 进产品面）：各自需要一份**手推**的
  ready 扇出 / 轮转公平性期望源 ⇒ ⇒ arbiter 还额外要求轮转相位口径稳定
  （`.rddf/checks/examples-rtl-sim-exclusions.tsv` 逐行登记）

## RUN
```bash
ctest -L spinalhdl-ported -R "stream" 2>/dev/null || true
./run_all_ported_tests.sh  # includes all 4

# 默认模式 = `.verif` 驱动（解释器路径）
./build/examples/stream/tb_stream_mux_demo

# 重新生成 TB（改 `.verif` 之后）
build/examples/stream/tb_stream_mux_demo --emit-rtl --out <dir> --emit-hygiene \
    --emit-tb-verif examples/stream/tb_stream_mux_demo.verif
# ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh` 会**逐字节**比对仓内 TB（⑥' 新鲜度）
```
