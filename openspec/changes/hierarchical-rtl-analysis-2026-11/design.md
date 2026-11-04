# 设计：分析层（`hierarchical-rtl-analysis-2026-11`）

> 决策在 ADR-058 / 060 / 062；本文件承载**分析算法**与**前置 spike 结论**。
## 3. 分析层（伪码）

```
analysis(root):
  ctx  = root.context()
  all  = ctx->get_nodes()
  order= ctx->get_eval_list()                 # 拓扑序（regs 不穿透 next ⇒ 单独处理）

  # ① 成层判定（ADR-061）：类级默认 + 实例级覆盖 + emitter 侧 Granularity
  layered(c) = decl(c) 或 override(c) 或 granularity==ForceLayer
  (ForceInline ⇒ layered(c)=false for all c)

  # ② 组件树 → 模块清单（自底向上）
  walk(c):
      for ch in c.children(): walk(ch)
      if not layered(c): return                # inline ⇒ 不产生 module
      ports[c] = { n ∈ all | owner(n)==c ∧ n.type ∈ {input,output} }
      body[c]  = { n ∈ order | owner(n)==c ∧ n ∉ ports[c] }

  # ③ inline 的组件：其节点并入**最近的成层祖先**
  for c in tree: if not layered(c): owner'(n) = nearest layered ancestor of c

  # ④ 跨模块边 → 端口提升（ADR-060 的方向矩阵）
  for each edge (u→v):
      cu, cv = owner'(u), owner'(v)
      if cu == cv: continue
      if cv is cu 的祖先:      # 父→子
          if v ∈ ports[cv]: 例化映射
          else:              合成子侧 input 隐式端口（+CHWARN）
      elif cu is cv 的祖先:    # 子→父
          if u ∈ ports[cu]: 父层 wire 中转
          else:              合成子侧 output 隐式端口（+CHWARN）
      else:                   # 兄弟→兄弟
          父层 wire 中转（两端都必须是端口）
  # ⚠️ 第三格（子端口由父层表达式驱动）⇒ **发射期虚拟重定向**：把驱动搬进子模块
  #    （⚠️ **MUST NOT** 改 IR —— ADR-060 §2.1；`set_src` 会让**扁平产物也变** ⇒ 破产）

  # ⑤ 悬空输入冒泡（ADR-060）
  for each 子端口 p with driver(p)==nullptr: 逐级向上冒泡 ⇒ 顶层 input（名字 = ctx 级名）

  # ⑥ 跨模块 ch_mem ⇒ 响亮拒绝（ADR-060；⚠️ 按 **inline 折叠后** 的 module 归属判，
  #    否则被 inline wrapper 包住的 chlib::memory 会被误拒 —— oracle R16）

  # ⑦ 跨模块边的第三轴（ADR-060 v1.4）：driver 的 owner 在更上层（含 nullptr ⇒ 顶层）
  #    ⇒ 沿祖先链逐级合成 pass-through input 端口（否则父 module 引用不到顶层网）
  # ⑧ 前置校验（三条响亮拒绝，MUST 在分析**之前**跑 —— 诊断要指向根因）
  for n in all:
      ow = owner'(n)
      if ow != nullptr and not in_subtree(ow, root):
          fatal("组件 <ow> 未登记进组件树；请 top.add_child(std::move(unit))")   # B2
  for c in tree:                       # 已登记但从未 build ⇒ 有节点、无端口（ADR-059 D6③）
      # ⚠️ 判据是 `not built()` —— build() 先置 built_ 再调 create_ports()（src/component.cpp:99-140）
      # 若写成 `built() and not ports_created()` ⇒ 恒假 ⇒ 这条拒绝永不触发（critic R2 B1）
      if not c.built():
          fatal("未 build 的子组件 <path>：它没有端口，无法成为 module")
  for c in tree:                       # 成层组件自己驱动自己的 ch_in ⇒ 无定义（ADR-062）
      for p in c.ports(ch_in) where driver(p) in c.body:
          fatal("成层组件自驱动 ch_in <path>：子 module 内对 input 赋值非法，父层又引用不到")
  for n in all:                        # 归属空洞（ADR-058 §2.1）
      if owner'(n) == nullptr and n not in {default_clock, default_reset}:
          warn("节点 <name> 不属于任何组件 ⇒ 归顶层 module")                     # clock/reset 不告警
```

⚠️ **`owner == nullptr` 的节点**（context 级 clock/reset + 用户在 `main()` 里不经组件直接建的节点）
⇒ **已裁决**（ADR-058 §2.1 v2.3）：归**顶层 module** + `CHWARN` ⇒ **MUST NOT** 拒绝
⇒ ⇒ 分析层把它们放进根 `ModuleDef` 的 `body`（clock/reset 则进端口表）。
⚠️ 这条**不是**「兜底」而是**必要条件**：拒绝它们 ⇒ 任何含 `ch_reg` 的设计都不可层次化；
且扁平产物把它们作顶层端口发射 ⇒ 不归顶层就无法满足「默认全 inline 逐字节一致」。

---

## 前置 spike（阻塞项 · oracle R1）—— ✅ **已完成（2026-11-02）**

**结论：可行** ⇒ 「结构同构」判据**成立**，C3/C6 的排期与成本**不变**。

**实测机制**（verilator **5.048**，`build/verilator-install/bin/verilator`）：

| 步骤 | 实测 |
|---|---|
| ① 提取 | `verilator --json-only --Mdir obj_dir three_layer.v` ⇒ `.tree.json` + `.tree.meta.json` |
| ② 结构 | `.tree.json` 的 **`modulesp` 是「模块列表」**（不是单指针）；每项带 `addr`（如 `'(M)'`）与 `name` |
| ③ 实例 | 每个模块的 `stmtsp` 里 `type == 'CELL'` 的项 = 一个**实例**，带 `name`（实例名）与 `modp`（指向被实例化模块的 `addr`） |
| ④ join | **`modp` 在 `addr → name` 表里查** ⇒ ⇒ 直接得到 `(parent_module, inst_name, module_name)` |

⚠️ **比 oracle 预估的更简单**：oracle 曾担心「实例→模块名只能靠 `m_modp` 指针 join 到
`.tree.meta.json` 的 `ptrNames` 表」⇒ ⇒ **实测不需要**：`modulesp` 本身就是列表，
每个 CELL 的 `modp` 直接指向列表项的 `addr` ⇒ ⇒ **一次查表即可**。

**实测输出**（手写 3 层 fixture ⇒ 提取器产出）：

```
mid_mod	u_leaf	leaf_mod
top_mod	u_mid	mid_mod
```

**已固化为可复算资产**（⇒ 不再是「一次性探针」）：

| 资产 | 作用 |
|---|---|
| `scripts/lib/verilog_tree_extract.py` | 提取器（`--json` 可选输出形态） |
| `tests/fixtures/hierarchical_tree_extract/{three_layer.v,expected.tsv,README.md}` | **手写** 3 层 fixture + 期望结果（⚠️ 手写 ⇒ 不自证） |
| `scripts/verify_hierarchical_tree_extract.sh` | 护栏 **R0** fail-closed（verilator/提取器/fixture 缺一即失败）· **R1** 逐字节相等 · **R2** 负向对照（改坏实例名 ⇒ 结果 MUST 变 ⇒ 非恒绿） |
| `.github/workflows/ci.yml`（`pr-verilator-fast`，`if: matrix.shard == 1`） | CI 挂载点（该 job **已有** verilator 构建 ⇒ 零额外成本） |

⚠️ **`--xml-only` 在本仓不可用**（verilator **5.048** 已删除，`Changes:228`）⇒ ⇒
判据的提取源**钉死为** `--json-only`（`--dump-tree` 备选路径**未**采用）。
