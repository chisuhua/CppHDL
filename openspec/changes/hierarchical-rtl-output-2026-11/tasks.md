# 任务清单：`hierarchical-rtl-output-2026-11`

> ⚠️ **执行顺序是承重的**：C1（归属）MUST 先落 —— 归属错 ⇒ emitter 会把子模块状态
> 划进父模块 ⇒ 产出**看起来对、实际错**的 RTL。
> ⚠️ 每个 commit MUST 有 capability delta（归档**禁** `--skip-specs`）。
> ⚠️ 前置依赖：`component-framework-hardening-2026-10`（U-4 公共头 ODR）MUST 先落。

---

## C0 —— **P0 之前的 spike**（阻塞项 · oracle R1）

- [ ] 0.1 用**手写**的 3 层 SV 验证 `verilator --json-only` 能否提 `(parent_module, inst_name,
      module_name)`（`.tree.json` + `.tree.meta.json` 的 `ptrNames` join）；备选 `--dump-tree`
- [ ] 0.2 **出口判据**：能机械产出上述三元组 ⇒ 结构同构判据成立；**提不出** ⇒
      该判据 MUST 重新设计（C3/C6 排期与成本都不成立）⇒ ⇒ **先停，回到 ADR**

## C1 —— 归属与命名修正（ADR-059）

- [ ] 1.1 `Component` 加**配对构造作用域**（ctor 进 / ctor 出）+ `build_internal` 压 BUILD 作用域；
      `Component::node_owner()`（栈空回落 `current_`）；`lnodeimpl` 的 `parent_` 改用它
- [ ] 1.1b **`add_child()` MUST 补绑 `child->parent_ = weak_from_this()`**（ADR-059 D9，oracle R7）
      ⇒ ⇒ 判据：`ch_device<Rv32iPipeline>` 下每个成员子组件的 `parent().lock()` **非空**
      ⚠️ 不补 ⇒ 成员 `ch_module` 的 `parent_` 永久空（ctor 期 `weak_from_this` 为空）
      ⇒ 若 `subtree` 走 parent 链 ⇒ **旗舰 DUT 被误杀**
- [ ] 1.1c **`subtree` / 归属判定 MUST 只向下走 `children()`**（MUST NOT 用 parent 链）
- [ ] 1.1d **`context::init()` 的 clock/reset MUST 用「无归属」哨兵**（ADR-059 D10）
- [ ] 1.2 `ch_module` 的 parent 改取 `node_owner()`；**build context 改取环境 context**
      （`ch::core::ctx_curr_`）⇒ ⇒ **但 env ctx 为空时 MUST NOT 硬失败**：MUST 接受**显式 ctx
      来源**（构造实参或 `build(ctx)` 前绑定），取不到任何 ctx 才 `CHFATAL_EXCEPTION`
      （ADR-059 D2′；⚠️ 硬失败会让「含子模块的单元」无法在无 active parent 处装配）
      ⇒ ⇒ **「无 active parent」分支同批由 `CHERROR` 升格为 `CHFATAL_EXCEPTION`**（D8）
- [ ] 1.2b **跨 ctx 守卫**：`build(ctx)` 的 ctx MUST == 构造期 env ctx，不等 ⇒ `CHFATAL_EXCEPTION`
      （ADR-059 D2″；否则子模块节点留在另一 ctx ⇒ **整块逻辑静默消失**）
- [ ] 1.3 `build_hierarchical_name()` 改用 `parent->name()`；`hierarchical_name()` 一并修正
- [ ] 1.4 新增 `Component::local_name_` / `module_type_name()`（默认 sanitize(`typeid(*this).name()`）
- [ ] 1.5 **探针入仓**：`tests/test_component_ownership.cpp`（3 条断言：成员节点归属 /
      成员 `ch_module` 挂直接父 / 深度≥3 无重复前缀）+ 作用域配平断言（**用 `CHFATAL_EXCEPTION`**）
      + **构造抛出的栈回退**断言（⇒ 「栈非空 ⇒ 抛异常」在配对作用域下**不可触发**，勿写）
      + **`create_child` 未 build** 的形态断言（⇒ 有节点、无端口）
- [ ] 1.6 **归属 ↔ 语义分流回归**：`type_input` 节点的指令表归属（`simulator_init.cpp:95`）
      MUST 与变更前逐项一致；**`io.h` 四处方向守卫改用 `node_owner()`**
      ⇒ 判据改为「**改前改后均无触发**」（⚠️ 不是「触发面相同」—— 新误报会让 diff 永不相等）
- [ ] 1.7 **文本变化面登记**（≥ 4 处实测依赖：`tests/test_axi_spi.cpp:213-214` ·
      `tests/benchmark/verilator_harness_tc07.cpp:33` · `tc08.cpp:51` ·
      `tests/test_verilator_backend.cpp` 的 `module top` / 端口名断言）
- [ ] 1.8 验收：`ctest -LE perf` · `run_all_samples.sh` · `run_all_ported_tests.sh` 全绿 ·
      计数锚点同步；delta = `component-ownership-and-naming` ADDED ×1

## C2 —— 层级粒度声明（ADR-061）

- [ ] 2.1 `CH_HIERARCHY_LAYER` 类级宏 + `Component::is_layer()`（默认 false）
- [ ] 2.2 `ch_layer<T>` / `ch_inline<T>` 两个新 API 名（**`ch_module` 签名零变更**）
- [ ] 2.3 **无 active parent 处的装配点**支持（下游 N2：桥接 plugin 在 `setup()` 内装配）
      ⇒ ⇒ ⚠️ **名字 MUST 由装配方自己算成全路径**再传给 ctor（`父->name()+"."+局部名`）
      ⇒ ⇒ **MUST NOT** 依赖「`add_child` 会补前缀」（它只 `push_back`，不碰 `name_`；
      `name_` 由 **ctor 实参逐字**决定 ⇒ 只换顺序**补不回**前缀）
      ⇒ ⇒ `add_child` 仍 MUST **先于** `build()`（让 `parent_` 就位）+ 用返回的 `shared_ptr` 续存
- [ ] 2.4 判据：类级 × 实例级**三组合**的成层结果与声明一致 + 「无 parent 处装配仍能成层」
- [ ] 2.5 delta = `rtl-hierarchical-output` ADDED ×1（粒度）

## C2.5 —— 接口类型与端口完整性（ADR-062）

- [ ] 2.6 **成层且非根** module 的端口 **⊇** 自身 `ch_in` ∪ `ch_out`（driven 与否不影响），
      且每个端口可追溯到**五类**来源（自身端口 / inline 后代 `ch_out`（**仅根允许**）/
      隐式端口 / 冒泡端口 / **时序基础设施 clock·reset**）
      ⇒ ⇒ **根 module 不适用本条** —— 它 MUST 走扁平规则（否则与「全 inline ⇒ `cmp` 0」互斥）
- [ ] 2.7 **接口白名单**：只允许 `ch_uint` / `ch_bool`（禁 `ch_mem` / `ch_reg` /
      bundle / `ch_stream` / **clock·reset** / `inout`）；数组字段命名 `x_<i>`
- [ ] 2.8 **观测/跨层读取三段裁决**（ADR-062 §2.6 v1.3）：
      ① 父读**子内部节点** ⇒ **按 ADR-060 §2.1 行 2 合成子侧 output 隐式端口**（+`CHWARN`）
      ⇒ 这是 Verilog 的**唯一**路径，MUST NOT 拒绝（原「合成在父侧」**物理上不可能**）；
      ② **稳定交付观测面**（下游 `reg_*` / `memsig` / `tohost`）⇒ MUST 由**单元自己**声明成 `ch_out`；
      ③ MUST NOT 把「父层临时建 `ch_out` 读子内部」当**交付接口**（⇒ 那只是调试胶合）
      ⚠️ **②③ 是下游义务**（`reg_*` / `memsig` / `tohost` 由下游单元自声明）⇒
      **本仓无判据**（判据落在 ADR-062 §5）⇒ 不构成本 change 的未完成 AC
- [ ] 2.9 判据：把某单元**输入**改成「只从 host 侧键值通道读、不声明 `ch_in`」⇒
      「体内 MUST NOT 引用非自己/非祖先的端口节点」这条 **MUST 转红**
      （⚠️ **不用**「端口数变 0」—— 输出方向必须先有 `ch_out` 作源）

## C3 —— 分析层（+ golden）

- [ ] 3.1 `HierarchyAnalysis`：`ModuleDef` / `InstanceDef` / 端口表 / 体内节点集
      （按 `get_eval_list()` 拓扑序过滤）
- [ ] 3.2 **inline 归属**：被 inline 的组件节点并入**最近的成层祖先**
- [ ] 3.3 跨模块边扫描 ⇒ **按方向**识别隐式端口（父→子 input / 子→父 output /
      子端口由父层驱动 ⇒ 图重写标记）/ 跨模块 `ch_mem` ⇒ 拒绝标记
- [ ] 3.4 悬空输入识别（`driver()==nullptr`）⇒ 冒泡路径
- [ ] 3.5 **golden 文本**产出（模块清单 + 端口表 + 连接表）⇒ 与 `tests/golden/` 快照**逐字节比对**
- [ ] 3.6 `owner == nullptr` 的节点（context 级 clock/reset + `main()` 里直接建的节点）
      ⇒ **归顶层 module** + `CHWARN`（ADR-058 §2.1 v2.3，**已裁决**）⇒ MUST NOT 拒绝；
      判据：含 `ch_reg` 的设计 MUST 仍能产出层次化产物
      ⚠️ **clock / reset MUST NOT 告警**（按设计不归属组件 ⇒ 否则每个设计恒响）
- [ ] 3.6b **前置校验**（三条响亮拒绝 + 一条告警，MUST 在分析**之前**跑）：
      ① `owner(n) != nullptr` 但 `owner ∉ subtree(top)` ⇒ `CHFATAL_EXCEPTION` +
      `add_child` 修法（负向对照：`add_child` 后 MUST 正常产出）；
      ② 已登记但**从未 build** 的组件 ⇒ `CHFATAL_EXCEPTION`（有节点、无端口）；
      ③ 归属空洞节点 ⇒ 新增 `CHWARN`（clock/reset 除外）
- [ ] 3.7 delta = `rtl-hierarchical-output` ADDED ×1（结构）

## C4 —— emitter 拆分（扁平零变化）

- [ ] 4.1 `verilogwriter` → `ModuleEmitter`（接收**节点子集 + 端口表 + 名字表**）
      ⇒ ⚠️ **不是「原样复用」**（oracle R5）：可复用**算子 / 宽度 / `always_ff`** 的生成逻辑，
      但**端口表 / 声明角色 / module 头**三者 MUST 重新实现
      ⇒ ⇒ 因为 `type_output` 在 module 内有**三种角色**（自身端口 / 隐式输出端口 / 体内 wire），
      而现状 `print_decl`（`:432-499`）**只有端口一种** ⇒ MUST 新增 wire 分支
- [ ] 4.2 ⚠️ **时钟名参数化**（`default_clock` / `default_reset` 当前**硬编码**；实测发射点
      **恰好 4 处**：`:278`、`:755`、**`:1272`**（sread 的 `always_ff`）、
      **`:1310`**（mem **写**端口的 `always_ff @(posedge default_clock)`）
      ⇒ ⇒ ⚠️ 早期清单里的 `1255` / `1293` 是**注释行**、且**漏了 `1310`**（oracle R17）
      ⇒ ⇒ 漏 `1310` ⇒ 带 RAM 的层次化产物里写端口模块引用未声明 clock
- [ ] 4.3 命名表：**沿用 ctx 级全局表**（`node_names_`）⇒ MUST NOT 发明 per-module 命名
      ⇒ ⇒ ⚠️ MUST 以**ctx 级只读快照**传入所有 module（MUST NOT 逐 module 新建 writer
      —— 会毁掉 `_N` 后缀一致性；复用同一 writer 则 MUST 每 module 重置其余成员状态）
- [ ] 4.4 验收：扁平产物相对 **C1 之后**的基线**逐字节不变**

## C5 —— 层次发射

- [ ] 5.1 module 主体发射（端口表 + body + clock/reset 贯通）
      ⇒ ⇒ **根 module 无条件发 clock/reset**；非根按 `has_seq` ＝ 含 `reg` **或 mem 写端口**
      **或** `sread`（`aread` 不算）⇒ ⇒ ⚠️ mem 写端口的 `always_ff` **没有 reset 分支**
      ⇒ **clock / reset 分开判**（否则纯 RAM 写模块得到一根永不使用的 reset 端口）
- [ ] 5.2 实例化发射 + 父层 wire 中转（兄弟间）
- [ ] 5.3 隐式端口合成（**按方向** 4 行 + **第三轴：driver 的 owner 在哪个 module**）
      + `CHWARN` + 生成文件内注释 + `strict` ⇒ 抛异常
      ⚠️ 「子侧端点是子端口」时 **不合成端口** —— **复用**该端口，父层用实例端口映射
      ⚠️ driver 的 owner 在**更上层**（含 `owner == nullptr` ⇒ 顶层）⇒ 沿祖先链
      **逐级合成 pass-through input 端口**（否则父 module 引用不到顶层网）
- [ ] 5.4 悬空输入冒泡（名字 = ctx 级全局名 ⇒ 与扁平逐字相同；来源写注释）+ `CHWARN`
- [ ] 5.5 **跨模块 `ch_mem` ⇒ 响亮拒绝**（⇒ 按 **inline 折叠后** 的 module 归属判定）
      + 错误信息给出两条「mem 与使用者同层」的替代路径
- [ ] 5.5b **自驱动 `ch_in` ⇒ 响亮拒绝**（成层组件自己驱动自己的 `ch_in`）
- [ ] 5.6 **不复用**（ADR-062 §2.6）：同类型多实例 **各发一份** module，名 `<type>_<序号>`
      （序号按实例**路径排序**，确定性）；同名不同类型 ⇒ 抛异常
      ⚠️ 原「三要素全等 ⇒ MUST 复用」已**改判**（⇒ 端口名含实例路径 ⇒ 端口签名恒不同 ⇒ 互斥）
- [ ] 5.7 `Granularity` 枚举（`RespectDeclarations` / `ForceInline` / `ForceLayer`）
- [ ] 5.8 **新增护栏** `scripts/verify_hierarchical_rtl.sh`（零未声明引用 · 实例端口名 ∈ 目标
      module 端口集 · 产物确定性（两次生成字节相同）· 跨模块 `ch_mem` 被拒）⇒ 挂 CI 两处
- [ ] 5.9 判据：结构同构（module 树 ≡ 成层组件子树）+ 边界矩阵 **28** 条逐条
- [ ] 5.9b **端口角色判定**（ADR-062 §2.1 / ADR-061 四角色）：① 自身端口 ② 隐式输出端口
      ③ **体内 wire**（inline 后代的 `ch_out` 在**非根** module 内）④ **子端口的连接网**
      （父 module 内 `logic` + 实例 `.port(net)`）
      ⇒ ⇒ `print_decl` / `print_logic` MUST **按角色**发射，MUST NOT 继续按 `driver()` 判
- [ ] 5.10 **顶层端口表 MUST 复刻扁平 `print_header` 的顺序**
      （clock → reset → 无 driver 的 input → output，`codegen_verilog.cpp:333-362`）
      ⇒ ⇒ `ModulePort` 必须带 `is_clock` / `is_reset` 维度
- [ ] 5.11 **顶层端口名 MUST 无框架前缀**（`__if_` / `__x_` 只允许出现在**非顶层**
      module 的端口上）⇒ ⇒ `__undriven_` 前缀**已作废**（冒泡端口 MUST 用 ctx 级全局名）

## C6 —— 等价性验证

- [ ] 6.1 Verilator backend 新增**只增不改**入口（既有 `initialize(ctx, …)` 逐字节不变）
      ⇒ ⇒ ⚠️ **不止加一个函数**（oracle R13）：MUST 接收**显式端口集合**
      （`node_id` + 顶层端口名 + 宽度 + 方向），并处理
      ① `build_port_access_table`（`:594-660`，按 node id 取 `data_map_` / 按 Vtop 字段取端口）·
      ② `emit_sim_main_postlude`（`:193-231`，从 ctx 全量生成）·
      ③ `invoke_verilator` 的**单文件**编译 + 缓存 key（`:490-497`）
      ⇒ ⇒ 工作量 **~7–9 天**（不是 ~4 天）
- [ ] 6.2 **映射表**产出（设计级信号名 ↔ 扁平端口名 ↔ 层次化 (module, port)）
- [ ] 6.3 ≥5 个 DUT 的逐拍等价（两层 / 三层+兄弟连线 / bundle 成员跨层 / 含 `ch_mem` /
      真实最深设计）⇒ 观测集合**非空**且**至少一个随激励变化**
- [ ] 6.4 **判别力自证**：故意破坏层次化（删连线 / 改隐式端口方向）⇒ 测试 MUST 转红 ⇒ 还原复绿
- [ ] 6.5 `verilator --lint-only -Wall` 零 error；同一设计两次生成**字节相同**
- [ ] 6.6 **默认（全 inline）产物与扁平产物逐字节一致** ⇒ **允许差异 = 空集**：
      `cmp <flat.v> <hier_forceinline.v>` 退出码 MUST 为 **0**
      （⇒ MUST NOT 用「这算注释差异」豁免；若实现必须产 banner ⇒ 先改 ADR-058 §5）
- [ ] 6.7 **结构同构的 RTL 树 MUST 独立提取**（`verilator --json-only` / `--dump-tree`；
      ⚠️ **`--xml-only` 在仓内 5.048 已删除**）⇒ MUST NOT 复用分析层的
      `ModuleDef` / `InstanceDef`（否则判据自证）

## C7 —— 文档同步

- [ ] 7.1 `docs/usage_guide/05-component-design.md` §5 改写（指向新的粒度模型）
- [ ] 7.2 `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` 补粒度章节
- [ ] 7.3 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 转正（Status Draft → Accepted）
- [ ] 7.4 根 `AGENTS.md` 的 WHERE TO LOOK / 已知限制同步

## C8 —— 归档

- [ ] 8.1 `openspec validate --strict` 通过（注意：仓库钉 CLI **1.4.1**，本机 **1.14.3**
      会对「Requirement > 500 字符」等报**与本改动无关**的 warning ⇒ 判据：与纯净 HEAD 的失败集合**同集合**）
- [ ] 8.2 归档**禁** `--skip-specs` ⇒ 2 个 capability 的 delta 合并进主 spec
- [ ] 8.3 归档后 MUST **补写** 2 个新 capability 的 `## Purpose`（archive **不合并** delta 的 Purpose）
- [ ] 8.4 计数锚点同步（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` /
      `EXAMPLES_TEST_SOURCES`）
- [ ] 8.5 全部护栏 + `--self-test` exit 0

---

## 依赖图

```
C0（spike：RTL 树提取 —— 先验证，否则判据重设计）
  └─▶ C1（归属+命名）
U-4（ODR，前置，来自 component-framework-hardening-2026-10）
  └─▶ C1
        ├─▶ C2（粒度声明）
        │     └─▶ C3（分析层 + golden）┐
        └─▶ C4（emitter 拆分）─────────┴─▶ C5（层次发射）
                                              └─▶ C6（等价性）─▶ C7（文档）─▶ C8（归档）
```
