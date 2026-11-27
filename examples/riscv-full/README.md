# `examples/riscv-full/` —— **full 产品线**（IP 级目录 · ADR-078）

> ⚠️ **本目录当前是「空壳」**（S4c-0 · **零功能**）。
>
> **ADR-078**（`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md` · Accepted）
> 把平台形态裁为**双产品线**：
>
> | 产品线 | 成员目录（IP 级 / SoC 级） | 状态 |
> |---|---|---|
> | `riscv-mini` | `examples/riscv-mini` / `examples/riscv-soc` | **冻结保留**（旧名不改 · ADR-078 D1） |
> | `riscv-full` | **`examples/riscv-full`（本目录）** / `examples/riscv-full-soc` | **S4c-0 起**逐批填入 |
>
> **分批表（唯一权威）= ADR-078 的 D8**：
>
> | 批 | 内容 |
> |---|---|
> | **S4c-0**（本批） | 产品线抽象（marker + 元护栏 + 护栏参数化）+ **本线的空壳** |
> | S4c-1 | 取指接口 **fork**（D14）+ `i_cache_sync.h` 两处升级（burst fill + 读选 lane · D7）+ I-L1 接入 ⇒ ⇒ **本目录的核在此建立** |
> | S4c-2 | `AxiDmaLiteRegs` + DMA boot 闭环 |
> | S4c-3 | full 互连（`AxiInterconnect4x4` + `AxiToAxiLite`）+ L2 / DRAM |

## 本目录的 marker

`examples/riscv-full/.rddf-line.yaml` 是**该产品线的唯一真相**（标识 / 成员目录 / 关联 ADR /
关联 capability）⇒ ⇒ 中央登记面（`.rddf/checks/product-lines.md`）是**从它派生**的派生物。

⚠️ **改产品线的任何身份信息 MUST 改 marker**（不是改派生物）⇒ ⇒
`scripts/gen_product_lines.py --check` 会检出陈旧 ⇒ ⇒ 元护栏 C1 报红。

## 现在的状态

- 本目录**只有** marker（无源码）⇒ ⇒ **IP 级的核在 S4c-1 才建立**。
- 同产品线的 SoC 级目录 = `examples/riscv-full-soc/`（当前是**最小 SoC 顶层** + 1 个 Catch2 判据）。
- ⚠️ **零功能**：无核、无外设、无互连、无 RTL 发射。
