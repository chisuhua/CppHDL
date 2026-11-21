---
evidence: probe:scripts/verify_examples_rtl_sim.sh
---

# 产物等价记录 —— 头文件归位（`example-file-boundary-2026-11` · **E2 / E3** · AC16）

> **判据（AC16）**：把两个顶层设计头从 `include/axi4/` 搬到 `examples/axi4/` 之后，
> 两个产品的 4 件产物（`top.v` + `manifest.json` × 2）**sha256 必须相同**。
>
> ⚠️ **为什么它不是自动成立的**：搬家改的是 include 路径与 target 名 ⇒ ⇒ 若顺手
> 改动了设计图（例如漏搬一个成员、或换了根名），产物会变而**用例仍绿** ⇒ ⇒
> 必须逐字节对照。

## 基线（搬移前）

- **HEAD**：`55f3f5a`（change 2 之后、批次 A/B 之前）
- **取法**：`git worktree add <dir> 55f3f5a` + `cmake -B build -DBUILD_VERILATOR=OFF`
  （只发产物 ⇒ 不需要 verilator）⇒ `cmake --build build --target tb_axi4_lite_example emit_rtl_axi_soc`
- **前提**：同 build 配置（Release）· 同 `--emit-hygiene` · 发射命令见下

## 复算命令

```bash
# 搬移前（worktree @ 55f3f5a）
tb_axi4_lite_example --emit-rtl --out <A> --emit-hygiene
emit_rtl_axi_soc --hierarchical --out <B> --emit-hygiene

# 搬移后（当前树）
tb_axi4_lite_example --emit-rtl --out <C> --emit-hygiene
tb_axi_soc_example  --emit-rtl --out <D> --emit-hygiene

sha256sum <A>/top.v <A>/manifest.json <B>/top.v <B>/manifest.json
sha256sum <C>/top.v <C>/manifest.json <D>/top.v <D>/manifest.json
```

## 结果（2026-11-18 实测）

| 产物 | 搬移前 sha256 | 搬移后 sha256 | 逐字节 |
|---|---|---|---|
| AXI4-Lite `top.v` | `20e0f88e34d97e55051c4b4eb16f5b2099ccd7f8706bb44a4a7457abb8be8bbd` | 同左 | ✅ `cmp` 相同 |
| AXI4-Lite `manifest.json` | `cf7b40f6c49ba1780735ecd20e6c7aa962dbac76229a79d9c2baebdf9cdfab18` | 同左 | ✅ |
| SoC `top.v` | `dc4765566e34ad4fc7c340438cbbf2397e7979d7931165efc095d338d8a3acc7` | 同左 | ✅ |
| SoC `manifest.json` | `b0019abd22f1bfd86d477d4ec35bf59fb9c1baa32c8fb8f680d401bef89894c5` | 同左 | ✅ |

**结论**：4 件产物**逐字节相同** ⇒ ⇒ 头文件归位（含 target 改名 / tsv 四列 / 删除 `emit_rtl.cpp` /
`emit_rtl_axi_soc` / `axi4_full_example` 退役）**没有改变任何产物语义**。

⚠️ 交叉印证：Lite 的两个 sha256 与 change 2（`emit-cli-into-example`）记录的**逐字相同**
（`20e0f88e…` / `cf7b40f6…`）⇒ ⇒ 三批改动串起来看，产物链**全程未漂移**。
