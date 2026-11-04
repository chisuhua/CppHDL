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

---

## 4. 实施记录（**文档 MUST 与代码一致**）

落点：`include/hierarchy_analysis.h` + `src/hierarchy_analysis.cpp`（约 600 行）·
`include/codegen_naming.h`（**新增**，命名唯一真源）· `include/component.h`（`is_built()`）。

### 4.1 手推 golden（`tests/golden/hierarchy/three_layers.txt`）

DUT：`Top3 → Mid3 → Leaf3`，三者**都**用 `CH_HIERARCHY_LAYER` 声明成层；
每个组件恰好 1 个 `ch_in din`（8 bit）+ 1 个 `ch_out dout`（8 bit）；
连线 `Top.din→Mid.din` · `Mid.din→Leaf.din` · `Leaf.dout→Mid.dout` · `Mid.dout→Top.dout`。

**推导**（按 ADR-060 §2.1 与 ADR-061 决策②，**逐项人手做**，不是从实现抄的）：

| module | 端口 | 方向 | 宽度 | origin | 依据 |
|---|---|---|---|---|---|
| `top.mid.leaf` | `top_mid_leaf_unnamed_input` | in | 8 | self | 成层组件的 `ch_in` 恒为**本 module** 的 input 端口（**即使它有 driver** ⇒ emitter 忽略 driver；扁平的「有 driver ⇒ 顶层 wire」不适用于成层组件，否则子 module 就没有该输入了） |
| `top.mid.leaf` | `top_mid_leaf_unnamed_output` | out | 8 | self | `ch_out` 归**其所在 module** |
| `top.mid` | `top_mid_unnamed_input` | in | 8 | self | 同上（Mid 也是成层组件） |
| `top.mid` | `top_mid_unnamed_output` | out | 8 | self | 同上 |
| `top`（root） | `default_clock` | in | 1 | clock_reset | context 级节点（`owner == nullptr`）⇒ 归**顶层** + **不告警**（ADR-058 §2.1） |
| `top`（root） | `default_reset` | in | 1 | clock_reset | 同上 |
| `top`（root） | `top_unnamed_input` | in | 8 | self | 顶层的 `ch_in` |
| `top`（root） | `top_unnamed_output` | out | 8 | self | 顶层的 `ch_out` |

**顺序** = 节点 id 升序（= 创建序）⇒ 每个 module 内 `din` 在 `dout` 之前；
根 module 内 `default_clock` / `default_reset` 最先（ctx 初始化时建）；
**模块顺序** = post-order ⇒ `top.mid.leaf` → `top.mid` → `top`。
**名字** = 扁平发射器的全局命名表（sanitize 后 `top.mid.leaf.unnamed_input` →
`top_mid_leaf_unnamed_input`）。

⇒ 手写这份 golden 后跑测试，**逐字节吻合**（差一个字节都会红）。

### 4.2 实施期三处订正（决策未变）

| # | 设计稿（伪码 §3） | 定稿 | 原因 |
|---|---|---|---|
| 1 | 名字 = 「ctx 级节点名」 | 名字 = **扁平发射器的全局唯一表**（`sanitize` + 计数器去重），提到 `include/codegen_naming.h` 与 `verilogwriter` **共用** | `ch_in` 的默认名全是 `unnamed_input` ⇒ **同一 module 内端口重名**（实测：两个 8 位输入都叫 `top_helper_unnamed_input`）⇒ 无法发射。且契约要求顶层端口名与扁平产物**逐字相同**（ADR-061 决策④）⇒ 两处必须同一张表，否则判据是自己说了算 |
| 2 | 「父→子：`if v ∈ ports[cv]`；否则合成隐式端口」——把**两侧**都当端口 | 只要求**子侧**端点是端口：父→子要求 `v` 是子 module 的 **input 端口**；子→父要求 `u` 是子 module 的 **output 端口** | 父侧端点**不需要**是端口：被 inline 折叠的、有 driver 的 `ch_in` 正是**父 module 体内的 wire**（实测 `inline_instance_path` 用例）⇒ 按原判据会把**合法**形态误拒 |
| 3 | §3 ④ 之后没有「③ 前置校验」与「④ 跨 module 边」的先后约束 | 前置校验**全部**在端口表 / 连接表**之前**跑 | 诊断要指向根因；否则「组件未 build」会先表现为「端口表缺项」 |

### 4.3 范围界定（**不是欠账，是分派**）

- **隐式端口合成**（跨模块边上「子侧端点不是端口」· 兄弟边 · 跨层 pass-through · 第三轴）
  ⇒ 本层**识别并 `CHFATAL_EXCEPTION`**，**合成归 T3-④**。
  ⚠️ 理由：合成会**凭空新增端口**，其命名 / 方向 / 冒泡规则是发射期契约的一部分；
  在本层「猜」出来会让 golden 把分析层的缺陷**合法化**（而 golden 是后续所有段的基线）。
- `owner == nullptr` 的**非** clock/reset 节点 ⇒ 归顶层 + `CHWARN` + 记入
  `HierarchyAnalysis::warnings`（判据可断言，不靠抓 stderr）。

### 4.4 变异清单（**12/12 全抓**，每条先确认可编译，还原后复绿）

| # | 变异 | 结果 |
|---|---|---|
| M1 | `nearest_layer()` 不折叠（inline 组件各自成层） | RED（inline 用例） |
| M2 | 去掉悬空输入冒泡 | RED |
| M3 | context 级 clock/reset 当体内节点 | RED（三层 / inline） |
| M4 | inline 折叠的 `ch_out` 标成 `Self` | RED |
| M5 | 实例名派生方向反转（叶→根） | RED（`wrap_deep` 用例） |
| M6 | 去掉前置校验②（从未 build） | RED |
| M7 | 去掉跨 module `ch_mem` 拒绝 | RED |
| M8 | 去掉兄弟边 / 跨层边拒绝 | RED |
| M9 | 命名退回原始 ctx 名（不去重 / 不 sanitize） | RED（4 份 golden） |
| M10 | 去掉前置校验①（归属不在树里） | RED |
| M11 | 去掉前置校验③（自驱动 `ch_in`） | RED |
| M12 | 端口方向写反（`ch_in` 发成 output） | RED（**手推那份 golden 直接报 diff**） |

⚠️ M5 / M11 **首轮是绿的**（判别力缺口，已补 DUT）：
- M5 需要「成层组件藏在 **inline 包装层**里」才有**多段**相对路径
  （单段路径下方向反转不可观测）⇒ 补 `WrapTop → InlineWrap(inline) → DeepLayer`；
- M11 需要一个**真的自驱动 `ch_in`** 的成层组件 ⇒ 补 `SelfDriven`。

### 4.5 顺带发现的**框架级陷阱**（不在本 change 范围，已登记）

`CHWARN` / `CHERROR` / `CHFATAL` 宏内部声明 `std::string msg = printf_format(...)`
⇒ ⇒ **实参里若出现名为 `msg` 的变量，会绑定到宏内部那个尚未初始化的 `msg`**
⇒ ⇒ 读到悬垂指针：实测**SIGSEGV** / 打印乱码（`[WARN] (null)`）。
本次在分析层把 lambda 参数改名为 `text` 规避；**宏本身未改**（改它要动公共头，
且属于框架治理面）⇒ 建议独立立项（把内部变量改名成 `ch_log_message_` 一类不易撞的名字）。
