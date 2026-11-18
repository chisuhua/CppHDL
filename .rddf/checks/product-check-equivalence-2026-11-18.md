# F2 产物等价记录（`example-file-boundary-2026-11` · 「规则下沉」前后）

> **判据**：把 checker 的**规则侧**从 example 搬进 `ch::rtl_emit`（`include/rtl_emit/product_check.h`）
> 之后，**产物与结论 MUST 逐字节/逐字相同**（这是「只搬不改」的证据）。
>
> ⚠️ **复算命令**（前提：同 build 目录、verilator 在 `build/verilator-install/bin`）：
> ```
> rm -rf /tmp/A && build/examples/axi4/tb_axi_soc_example --check-products --out /tmp/A --emit-hygiene
> sha256sum /tmp/A/top.v /tmp/A/manifest.json /tmp/A/eq/flat/top.v /tmp/A/eq/hier/top.v
> ```
> ⚠️ 基线侧 = `git show 21a74ad:examples/axi4/tb_axi_soc_example.cpp` 单独编译出来的二进制（同一库、同一 verilator）。

| 侧 | 产物 | sha256（前 16） |
|---|---|---|
| 基线（F2 前 · HEAD 21a74ad 的 tb） | `top.v` | `dc4765566e34ad4f` |
| 基线（F2 前 · HEAD 21a74ad 的 tb） | `manifest.json` | `b0019abd22f1bfd8` |
| 基线（F2 前 · HEAD 21a74ad 的 tb） | `eq/flat/top.v` | `286ecd939707f0a4` |
| 基线（F2 前 · HEAD 21a74ad 的 tb） | `eq/hier/top.v` | `ede8e1698cbd8127` |
| F2 后（规则在库层） | `top.v` | `dc4765566e34ad4f` |
| F2 后（规则在库层） | `manifest.json` | `b0019abd22f1bfd8` |
| F2 后（规则在库层） | `eq/flat/top.v` | `286ecd939707f0a4` |
| F2 后（规则在库层） | `eq/hier/top.v` | `ede8e1698cbd8127` |

**结论**：4 件产物（含 `manifest.json`）在搬移前后 **逐字节相同**。

**同时逐字相同的结论行**（`--check-products` 的全部输出）：

```
CHECK 1 module 集: PASS (期望 5 个 · 实际 5 个)
CHECK 2 顶层端口: PASS (端口 14/14 · 后代未驱动 0)
CHECK 3 lint: PASS (verilator --lint-only --top-module top)
CHECK 4 双形态等价: PASS (ok=1 equal=1 变化量=2/3 自证=PASS [篡改被抓住（first_mismatch=cycle 0, observation 'top.rdata': flat=0x0 hier=0x4294967295)])
PRODUCTS-CHECK: PASS
```
