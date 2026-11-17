# AGENTS.md - AXI4 Bus Examples

Child of root `AGENTS.md`.

## OVERVIEW
4 AXI4 examples at increasing integration levels: Lite timing → Full burst → SoC → Interconnect.

## FILES
| File | Focus | CTest |
|------|-------|-------|
| `axi4_lite_example.cpp` | AXI4-Lite slave timing, VCD + assertions | ✅ |
| `axi4_full_example.cpp` | AXI4 Full 5-channel burst slave | ✅ |
| `axi_soc_example.cpp` | SoC: Matrix 2x2 + GPIO + UART（4 个 IP） | ✅ |
| `phase3a_complete.cpp` | Phase 3A: 4x4 Interconnect + AXI-to-Lite Converter | ✅ |
| `emit_rtl.cpp` | AXI4-Lite 层 RTL 发射入口（薄壳 · 库层 `run_emit_cli`） | — |
| `emit_rtl_soc.cpp` | SoC 层 RTL 发射入口（薄壳 + `--check-products` 产品级自检） | — |
| `tb_axi4_lite_example.verif` | **验证源**（`.verif` · 7 场景 / 113 断言 · **单一语义来源**） | — |
| `tb_axi4_lite_example.sv` | **由 `.verif` 生成**的独立 TB（`generated=yes` · 禁手改） | — |
| `tb_axi_soc_example.sv` | SoC 层独立 TB（**手写** · `generated=no` · UART 帧解码超出封闭原语集） | — |

## PATTERNS
- All use `Component` subclass → `Top` wrapper → `ch_device<T>` → `Simulator` in `main()`
- `ch_module<T>` instantiation inside `Component::describe()` (see root AGENTS.md)
- Peripheral headers: `include/axi4/peripherals/axi_gpio.h`, `axi_uart.h`
- ⚠️ **顶层组件住在 `include/axi4/`**（`axi4_lite_example_top.h` / `axi_soc_example_top.h`）
  ⇒ ⇒ `tests/**` 与 `examples/**` **共用同一份定义**（禁复制 —— 那会变成双权威）
  ⇒ ⇒ 顶层端口**已具名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`（不再是 `top_unnamed_input_N`）

## RUN
```bash
ctest -L spinalhdl-ported -R "axi4"
./run_all_ported_tests.sh  # includes all 4

# 重新生成 AXI4-Lite 的 TB（改 `.verif` 之后）
build/examples/axi4/emit_rtl_axi4_lite --out <dir> --emit-hygiene \
    --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif
# ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh` 会**逐字节**比对仓内 TB（⑥' 新鲜度）
```