# AGENTS.md - AXI4 Bus Examples

Child of root `AGENTS.md`.

## OVERVIEW
3 AXI4 examples at increasing integration levels: Lite timing → SoC → Interconnect.
（Full-burst 示例已于 2026-11-18 退役 ⇒ 行为覆盖在 `tests/test_axi4_full_burst.cpp`）

## FILES
| File | Focus | CTest |
|------|-------|-------|
| `tb_axi4_lite_example.cpp` | AXI4-Lite slave 验证（**由 `.verif` 驱动** · `RESULT: checks=113`） | ✅ |
| `tb_axi_soc_example.cpp` | SoC 激励 + 三模式（默认 / --emit-rtl / --check-products） | ✅ |
| `phase3a_complete.cpp` | Phase 3A: 4x4 Interconnect + AXI-to-Lite Converter | ✅ |
| `tb_axi4_lite_example.verif` | **验证源**（`.verif` · 7 场景 / 113 断言 · **单一语义来源**） | — |
| `tb_axi4_lite_example.sv` | **由 `.verif` 生成**的独立 TB（`generated=yes` · 禁手改） | — |
| `tb_axi_soc_example.sv` | SoC 层独立 TB（**手写** · `generated=no` · UART 帧解码超出封闭原语集） | — |

## PATTERNS
- **三角色分离**（`example-file-boundary-2026-11`）：设计定义 `*_top.h`（**无 `main`** · 只做实例化与连接）
  → 激励驱动 `tb_*.cpp`（`main`：加载激励 + `--emit-rtl` 发射）→ 激励源 `tb_*.{verif,sv}`
- `ch_module<T>` instantiation inside `Component::describe()` (see root AGENTS.md)
- Peripheral headers: `include/axi4/peripherals/axi_gpio.h`, `axi_uart.h`
- ⚠️ **顶层组件住在 `examples/axi4/`**（`axi4_lite_example_top.h` / `axi_soc_example_top.h`）
  ⇒ ⇒ 库 `include/` **不再**承载 example 专属设计（2026-11-18 搬出 ⇒ 见
  `.rddf/checks/public-api-surface.md` 的移除登记）；`examples/**` 与 `tests/**` **共用同一份定义**
  （禁复制 —— 那会变成双权威）
  ⇒ ⇒ 顶层端口**已具名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`（未具名编号形态已退役）

## RUN
```bash
ctest -L spinalhdl-ported -R "axi4"
./run_all_ported_tests.sh  # includes all 3

# 重新生成 AXI4-Lite 的 TB（改 `.verif` 之后）
build/examples/axi4/tb_axi4_lite_example --emit-rtl --out <dir> --emit-hygiene \
    --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif
# ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh` 会**逐字节**比对仓内 TB（⑥' 新鲜度）
```