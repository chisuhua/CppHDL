# fixture：`hierarchical_tree_extract`（层次化 RTL 输出 · C0-0 spike）

## 用途

「结构同构」判据（ADR-058 §5）MUST 能**独立于 emitter** 读出 RTL 的 module 树
⇒ ⇒ 本目录提供**手写**的 3 层 SV + 其**期望提取结果**，供
`scripts/verify_hierarchical_tree_extract.sh` 核验提取器。

## 文件

| 文件 | 说明 |
|---|---|
| `three_layer.v` | **手写**的 3 层 SV（`top_mod` → `mid_mod` → `leaf_mod`；2 个实例） |
| `expected.tsv` | 期望提取结果，每行 `<parent_module>\t<inst_name>\t<module_name>`（按父模块名排序） |

⚠️ **为什么必须手写**：若 fixture 由 CppHDL 的 emitter 生成 ⇒ 提取器与 emitter
共享同一套假设 ⇒ 判据**自证**（⇒ 分析层算错 ⇒ 两边一起错 ⇒
「计数对但树形错」这个 Scenario **永远不可触发**）。

## 提取机制（实测，verilator 5.048）

```
verilator --json-only --Mdir obj_dir three_layer.v
  ⇒ obj_dir/Vthree_layer.tree.json（+ .tree.meta.json）
python3 scripts/lib/verilog_tree_extract.py obj_dir/Vthree_layer.tree.json
  ⇒ mid_mod\tu_leaf\tleaf_mod
    top_mod\tu_mid\tmid_mod
```

- `.tree.json` 的 `modulesp` 是**模块列表**；每项带 `addr`（如 `'(M)'`）与 `name`；
- 每个模块的 `stmtsp` 里 `type == 'CELL'` 的项 = 一个**实例**，带 `name`（实例名）
  与 `modp`（指向被实例化模块的 `addr`）；
- ⇒ ⇒ **join = 用 `modp` 在 `addr → name` 表里查**（不需要 `--xml-only`，
  该选项在 5.048 已被删除 ⇒ `third_party/verilator/Changes:228`）。
