# 任务清单：`hierarchical-rtl-emission-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical **ADDED ×11**）。
> ⚠️ **前置**：**T3-②**（分析层）· **T3-③**（emitter 拆分）。
> ⚠️ **本 change 的判据面最大** ⇒ ⇒ 建议**逐条**落地，每条都要有判别性对照。

## T0 批次 B1（策略枚举收敛 · 零行为变化 · ✅ 2026-11-05）

> 判据来源：ADR-060 §2.4（`PortPolicy` / `UndrivenPolicy` 的**唯一定义**在 `include/component.h`）
> + ADR-061 v1.6.3 §2.5.1（粒度的**唯一入口** = RAII `Component::GranularityScope`）。

- [x] 0.1 删 `HierarchyPortPolicy` / `HierarchyUndrivenPolicy`（`include/hierarchy_verilog.h`）
      ⇒ ⇒ `HierarchyEmitOptions` 改用 `ch::PortPolicy` / `ch::UndrivenPolicy`
      ⇒ ⇒ **唯一权威** = `include/component.h`（ADR-060 §2.4）
- [x] 0.2 删 `Component::set_granularity()`（声明 + 定义）
      ⇒ ⇒ 实测**零调用点** ⇒ ⇒ 绕过 RAII 的永久泄漏通道 ⇒ ⇒ 粒度只能经作用域设置
- [x] 0.3 `HierarchyEmitOptions`：`top_module_name` 已有（默认 `top`）·
      **无** `granularity` 字段（全局权威）⇒ ⇒ 与 ADR-061 v1.6.3 §2.5 一致
- [x] 0.4 ⚠️ **C1 的处置**：代码里**从未实现** `out_dir` / `one_file_per_module`
      ⇒ ⇒ **无需保留任何字段** ⇒ ⇒ ✅ **2026-11-05 更新：C1 已答 = 接受单文件多 module**
      （ADR-058 **v2.10**）⇒ ⇒ 与本任务结论**一致** ⇒ ⇒ **无追加工作**
      （⇒ ⇒ 「多文件模式」**不重开** ⇒ ⇒ backend **不需要**文件列表 + 缓存 key 改造）
- [x] 0.5 **判据**：编译期 `static_assert`（选项字段类型 == 权威枚举 + 默认值）+
      **粒度作用域的三格判据**（半生效 ⇒ `RespectDeclarations` 形态 · 正向 `ForceInline`
      ⇒ 单 module · 正向 `ForceLayer` ⇒ 3 module）+ 既有 4 用例绿 +
      扁平外部基线 3 设计 `cmp` 全 0 + `ctest -LE perf` **238/238**

## T0b 批次 B2a（发射循环 collect 化 · ✅ 2026-11-05）

> 判据来源：ADR-060 §2.4.1（`PortPolicy` 三取值的确切失败语义 + **执行时机承重**）
> + 台账 §11.5 **O3**（collect 化与 F2/F3 改**同一个发射循环** ⇒ ⇒ 必须先行）。

- [x] 0.6 分析层**降级为记录**：新增 `ImplicitPortHandling{Reject, Record}` +
      `HierarchyAnalysis::pending_implicit_edges`
      ⇒ ⇒ ⚠️ **默认仍是 `Reject`** ⇒ ⇒ **T3-② 的 fail-closed 断言一字不改仍成立**
      ⇒ ⇒ ⚠️ 新字段 **MUST NOT 进 `to_text()`** ⇒ ⇒ **T3-② 的 4 份 golden 逐字节不变**
- [x] 0.7 跨模块 `ch_mem` **仍然抛**（ADR-060 §2.1 的裁决就是「响亮拒绝」⇒ ⇒ 不参与 collect）
- [x] 0.8 发射层 **collect**：合成类问题（分析层记录的边 + 发射器自己的「父侧端点非端口」
      检查）**全部收集** ⇒ ⇒ 上移到发射前的**预检**（⇒ ⇒ 失败时**尚未**产生任何输出）
      ⇒ ⇒ 并删除发射循环内的旧检查（⇒ ⇒ 两份判定必然漂移）
- [x] 0.9 **策略驱动的诊断广度**：`Reject` 只报**第一条** · `SynthesizeWarn` /
      `SynthesizeFatal` 报**全部**（+ `CHWARN` 每一条一条）
      ⇒ ⇒ ⚠️ **中间态**：合成本身属 B2b ⇒ ⇒ 本批三档**一律 fail-closed**（差异只在诊断广度）
- [x] 0.10 **判据**：`SynthesizeFatal` / `SynthesizeWarn` ⇒ 诊断含**两条**边 ·
      `Reject` ⇒ 只含**第一条** · 失败**不留产物** · DUT 用例 MUST 有**两条**边
      （⇒ ⇒ 否则「报全部」与「报第一条」不可区分）
      ⇒ ⇒ **变异验证**：把 `all` 恒置 false（退回首错即返）⇒ **转红**（1 failed）· 还原复绿
      ⇒ ⇒ `ctest -LE perf` **238/238** · 扁平外部基线 3 设计 `cmp` 全 0 · 6 个护栏全 PASS

## T0c 批次 B2b-1（行 2「子 → 父」的隐式端口合成 · ✅ 2026-11-05）

> 判据来源：ADR-060 §2.1 **行 2**（子侧端点**不是**子端口 ⇒ ⇒ 合成**子侧 output** 端口）
> + ADR-061 决策② ②（成层组件的 `ch_out` **即使无人消费**也是端口 ⇒ ⇒ 父层**必须接**它）。
> ⚠️ **批次拆分**（据实施期发现）：行 1（父→子）要动 `ModuleEmitter` 的**驱动重定向**
> ⇒ ⇒ 风险最高 ⇒ ⇒ **独立成 B2b-2**；F3（inline 后代 `ch_out` 的作用域落点）**依赖**行 2
> ⇒ ⇒ ⇒ 独立成 B2b-3。

- [x] 0.11 分析层：`PendingImplicitEdge` 增 **节点指针 + 方向 + 子实例名**
      ⇒ ⇒ ⚠️ 实例名**必须按方向**取（行 2 的子是**驱动端**所在 module ⇒ ⇒ 实施期踩过一次坑）
      ⇒ ⇒ ⚠️ 新字段 **MUST NOT** 进 `to_text()` ⇒ ⇒ **T3-② 的 4 份 golden 逐字节不变**
- [x] 0.12 发射器：`ModuleView::synthesized_ports`（**合成端口不是 IR 节点** ⇒ ⇒ 单列）
      + `print_header` 追加 ⇒ ⇒ ⚠️ **扁平路径恒空** ⇒ ⇒ 产物逐字节不变
- [x] 0.13 **行 2 的合成**：子侧合成 output 端口 `__x_<n>` + 子体内
      `assign __x_<n> = <子侧节点>;` + 父侧实例端口映射 `. __x_<n>(<父侧端点>)`
      ⇒ ⇒ 父侧那条跨 module `assign` 由既有 `suppress_driver_assign` 抑制
- [x] 0.14 **未被消费的子 output 端口 MUST 以空连接出现**（`.port()`）
      ⇒ ⇒ 否则 Verilator 报 `PINMISSING`（且 warning 即 error ⇒ ⇒ lint 失败）
      ⇒ ⇒ ⚠️ **子 input 不在此列**：未驱动的子 input MUST **冒泡**（B3）⇒ ⇒ 现在**故意不接**
      ⇒ ⇒ ⇒ 让 `PINMISSING` 把这类形态**响亮暴露**（而不是静默产生 X）
- [x] 0.15 **判据**：DUT = `OuterRow2` 读 `InnerRow2` 的**内部寄存器**
      （`io().dout <<= inner.instance().acc`）⇒ ⇒ ① 能产出（此前是 fail-closed）·
      ② 子 module 有 `output [7:0] __x_0` ③ 子体内 `assign __x_0 = …`
      ④ 父层实例接上 `.__x_0(` ⑤ 父层**零**跨 module assign ⑥ 合成名只在非顶层
      ⑦ 未消费的子 output 空连接
      ⇒ ⇒ **变异 M1**（关掉行 2 合成 ⇒ 转红）· **M2**（去掉空连接 ⇒ 转红）· 还原复绿
      ⇒ ⇒ `verilator --lint-only` 对三个产物 **exit 0** · `ctest -LE perf` **238/238**

## T0d 批次 B2b-2（行 1「父 → 子」的隐式端口合成 · ✅ 2026-11-05）

> 判据来源：ADR-060 §2.1 **行 1**（父 → 子，子侧端点**不是**子端口 ⇒ ⇒ 合成**子侧 input** 端口）
> +「**图重写** MUST NOT 真的改 IR」（v1.2 · oracle R12）⇒ ⇒ 只允许**发射期虚拟重定向**。
> ⚠️ **范围收窄**（实施期定）：本批**只**覆盖「子侧端点是**寄存器**」+「父侧驱动节点在父 module 内」
> ⇒ ⇒ 「proxy/op 源」子形态与**第三轴**（pass-through）⇒ ⇒ 归 **B2b-3**。

- [x] 0.16 `ModuleView::port_drives_node`（**发射期**的重定向表：节点 → 合成端口名）
      ⇒ ⇒ ⚠️ **MUST NOT** 写回 `lnodeimpl`（`set_src` 会污染 `users_` ⇒ ⇒ 既有
      `toVerilog()` 产物 / 解释器行为 / 两次生成的确定性**全部会变**）
      ⇒ ⇒ **扁平路径恒空** ⇒ ⇒ 产物逐字节不变（外部基线 3 设计 `cmp` 全 0）
- [x] 0.17 `ModuleEmitter::print_reg`：命中重定向表 ⇒ ⇒ 寄存器的 next **改由合成端口驱动**
      ⇒ ⇒ ⚠️ 不重定向 ⇒ ⇒ 子 module 引用不到父层节点 ⇒ ⇒ 该寄存器退化成「保持」
      ⇒ ⇒ ⇒ **静默丢驱动**（⇒ ⇒ 这正是行 1 必须重定向的原因）
- [x] 0.18 发射层：**行 1** 合成 = 子侧 **input** 端口 `__if_<n>` + 驱动重定向 +
      父侧实例端口映射 `. __if_<n>(<父层驱动节点>)`
      ⇒ ⇒ ⚠️ 「父侧驱动节点 MUST 在父 module 内」否则是**第三轴**⇒ ⇒ 仍 fail-closed
- [x] 0.19 **判据**：DUT = `OuterRow1` 直接改子层寄存器 `inner.instance().acc->next`
      ⇒ ⇒ ① 能产出（此前 fail-closed）② 子 module 有 `input [7:0] __if_0`
      ③ ⭐ **驱动重定向**（`reg <= __if_0;` 且**不得**出现「Register update (no next)」）
      ④ 父侧实例接上 `.__if_0(` ⑤ 子 module 内**零**父层信号引用
      ⑥ 合成名只在非顶层 module 的**端口声明**里
      ⇒ ⇒ **变异 M1**（关掉行 1 合成 ⇒ 转红）· **M2**（不做驱动重定向 ⇒ 转红）· 还原复绿
      ⇒ ⇒ `verilator --lint-only` 对四个产物 **exit 0** · `ctest -LE perf` **238/238**

## T0e 批次 B2b-3（proxy 子形态 + 合成名去重 + 探针矩阵骨架 · 待实施）

> 判据来源：ADR-060 **v1.9** §2.1 行 1（proxy ⇒ 合成 · op ⇒ 第七格拒绝 ·
> 父驱动子 `ch_out` ⇒ 第八格拒绝）+ §2.2 去重接缝。

- [x] 0.20 `print_proxy` 增 `port_drives_node` 分支（在 `suppress_driver_assign` **之前**拦截）
      ⇒ ⇒ ⚠️ **漏做的失败模式是静默读 X**：proxy 会被声明成无驱动 `logic`，
      而**默认 warning 集不查 `UNDRIVEN`** ⇒ lint **零告警**通过
- [x] 0.21 行 1 白名单加 `type_proxy`（⚠️ **MUST NOT** 加 `type_output`）
- [x] 0.22 合成名去重：发射前断言 `__if_<n>` / `__x_<n>` 不在 `names` 表内 ⇒ 冲突顺延 `_N`
- [x] 0.23 **探针矩阵骨架**：`成员类型 × 操作 × 方向` 机械组合，每格断言三态之一
      ⇒ ⇒ 本批覆盖 **5 格**：reg next ✅（B2b-2）· proxy ✅（本批）· op 操作数 ❌ 拒绝 ·
      父驱动子 `ch_out` ❌ 拒绝 · `operator=` 无处理（不产生边）
      ⇒ ⇒ **B4 补全**为完整矩阵（每格都要有断言且三态明确）
- [x] 0.20b ⚠️ **实施期补项**：**代理链穿透 MUST 在「被重定向的 proxy」处停下**
      （`print_output` 的 proxy-chain walk）⇒ ⇒ 否则它会取到 proxy 的 `src(0)` ——
      而那已被**父层**驱动覆盖 ⇒ ⇒ 子 module 内引用**父层信号** ⇒ ⇒ 跨 module 引用
      （且该信号在子 module 内**未声明** ⇒ 非法 Verilog）
      ⇒ ⇒ 被重定向的 proxy 是**子 module 内的本地 wire**（`print_proxy` 发的
      `assign <proxy> = __if_<n>;`）⇒ ⇒ 穿透停在它自己 = 正确
- [x] 0.24b **R2 的四条路径逐条变异**（critic 建议 · ADR-060 v1.10 R2）⇒ **✅ B2b-6 收口**：
      `print_input` driver walk 的变异原先**恒绿**（无判据）⇒ 新增 `[walk]` 用例
      （被重定向的命名 proxy 驱动 inline 子组件 `ch_in`）⇒ 去掉「只穿透未命名 proxy」守卫 ⇒ 转红；
      `print_proxy` 的 `src(0)`（early-return）变异 ⇒ 既有 row-1 proxy 用例转红。
      把 B2b-3 的 M3 变异**套用到另两条路径** —— `print_input` 的 driver walk
      （当前守卫 = 只穿透未命名 proxy）· `print_proxy` 的 `src(0)`（当前守卫 = early-return）
      ⇒ ⇒ 确认**各有专属判据** ⇒ ⇒ ⚠️ 目的 = 证明「每条 `src(0)` 解析路径都独立受约束」
      （ADR-060 v1.10 R2 的契约字面覆盖**所有**这类路径 ⇒ ⇒ 未来新增第五条时不被漏掉）
- [x] 0.24c **W1 的负向用例**（ADR-060 v1.10 §5 的宽度判据）：父侧 **9-bit** 节点驱动
      子侧 **8-bit** 端点（`ch_reg` next 与 proxy 各一）⇒ ⇒ MUST 在**合成之前**拒绝
      ⇒ ⇒ 诊断 MUST 点名两端 + 两个宽度值 ⇒ ⇒ MUST NOT 产出文件
      ⇒ ⇒ **✅ B2b-6 收口**：**四条**（双向 9→8 / 8→9 × {reg next, proxy}）· 单一 `width_check`
      helper 覆盖**全部**合成 site（含直接行 1 / 行 2）· 诊断含 `Remedy:`（可行动修法）
      ⇒ ⇒ 守卫变异（恒返回 true）⇒ 该用例转红
- [x] 0.24 **判据**：① proxy 跨层 ⇒ 子侧 `input __if_<n>` + 子内 `assign <proxy> = __if_<n>;`
      ② ⭐ **值判据**（逐拍值，不靠 lint/结构断言）③ op ⇒ 响亮拒绝 ④ 父驱动子 `ch_out`
      ⇒ 响亮拒绝 ⑤ 撞名 ⇒ 顺延 ⑥ `verilator --lint-only`（**ADR-060 §2.5 的门**）零告警

## T0f 批次 B2b-4（F3：inline 后代 `ch_out` 作用域 + `type_output`-as-wire · 待实施）

- [x] 0.25 分析层按作用域区分：顶层 ⇒ output 端口；非顶层成层 ⇒ **体内普通信号**
      ⇒ ⇒ ⚠️ **前置核实**：4 份 golden 的两处 `inline_folded` **都在 root module**
      ⇒ ⇒ 本改动**不动 golden** ⇒ ⇒ T3-② 的零差异判据保持（实测 87 断言全绿）
- [x] 0.26 `print_decl` 增 `type_output`-as-wire 分支（**不在** `view_.ports` 里 ⇒ ⇒ 进
      `wires` ⇒ ⇒ `emit_signal_decl` 发 `logic [W] name;`）；`print_header` 不把它当端口
- [x] 0.27 **判据**：非顶层 ⇒ `logic` 声明 + **具体的 `assign` 行** + **不**进端口表；
      顶层 ⇒ 仍是 output（负向对照）· ⭐ **产物确定性**（两次生成逐字节相同）
      ⇒ ⇒ **变异 M1**（关掉 as-wire 分支 ⇒ 转红 · 且产物缺 `logic` 声明 ⇒ lint 会抓）
      ⇒ ⚠️ **若不修**：体内输出是 `assign` 的目标却**无声明** ⇒ Verilator 报 undeclared

## T0g 批次 B2b-5（第三轴 pass-through + 兄弟边 · ✅ 已落地）

- [x] 0.28 **第三轴**：driver 的 `owner` 在更上层（含 `nullptr`）⇒ 沿祖先链逐级**
      同名** `input` 端口（每级一个）
      ⇒ ⇒ **落地形态 = G0 的 chain builder**（`src/hierarchy_verilog.cpp` 的 `build_chain`：
      逐级同名端口 + 逐跳实例映射 · **幂等** · 撞名/宽度/双驱动 **fail-closed**）
      + **两种 leaf adapter**：
      · **port adapter** —— 叶端点本身是叶 module 的 `ch_in` ⇒ 叶上**不**合成端口，
        最后一跳直接接它；
      · **node adapter** —— 叶端点是 reg/proxy ⇒ 叶上合成端口 + **驱动重定向**
        （`port_drives_node`）。
      ⇒ ⇒ **W1 宽度一致性**在合成之前判（不一致 ⇒ 拒绝，点名两个宽度）；
      叶端点是**输出**端口 ⇒ 拒绝（第八格）；非 reg/proxy 的非端口节点 ⇒ 拒绝（第七格）。
      ⚠️ **向上 pass-through（子 → 父但非直接父子）仍 fail-closed** ⇒ 属 **B3**（冒泡），
      但 MUST 复用同一个 `build_chain`（`ports_are_input=false`）。
- [x] 0.29 **兄弟边**：子 A 输出 → 子 B 输入 ⇒ 共同祖先内**一根** wire + 两处实例映射
      （**MUST NOT** 兄弟直连）
      ⇒ ⇒ 落点：`ModuleView::synthesized_wires`（**新增** · 与 `synthesized_ports` 同族 ——
      不是 IR 节点 ⇒ `print_decl` 发 `logic [W] name;`）+ 两端实例端口映射
      （`.out(wire)` / `.in(wire)`）
      ⇒ ⇒ **本批支持的形态**：两端是**同一 module 的直接子** + 驱动端是**子 output 端口** +
      被驱动端是**子 input 端口** + 宽度一致；其余（表亲边 / 非端口端点）⇒ **fail-closed**
- [x] 0.30 **判据**：第三轴每级一个同名端口 + 顶层可驱动（值判据）；兄弟边恰好一根连接网 +
      **零**隐式端口；两者均过 §2.5 的 lint 门
      ⇒ ⇒ 三个用例（`[third_axis]` ×2 + `[sibling]` ×1）· 全部产物
      `verilator --lint-only` **exit 0**
      ⇒ ⇒ **变异 3/4 抓 + 1 条等价变异**：M1 关掉第三轴 ⇒ 2 个用例红 · M2 关掉兄弟边 ⇒ 该用例红 ·
      M3 端口名不用 ctx 级全局名（改 `__pass_<n>`）⇒ 同名端口断言红 ·
      ⚠️ **M4（去掉 leaf 的驱动重定向）恒绿 = 等价变异**：第三轴的合成端口名**恒等于**驱动
      节点的全局名 ⇒ 寄存器 `next` 走既有路径时解析出的名字**逐字相同**
      ⇒ ⇒ 不是判别力缺口；⚠️ 但 `port_drives_node` **仍然保留**（`__if_<n>` 那条路径依赖它，
      且第三轴一旦改名它就承重）
      ⇒ ⇒ ⚠️ **实施期修掉的两个真缺陷（都属本批）**：
      ① `has_nodes` 曾要求 `child_instance_name` 非空 ⇒ ⇒ 非相邻边恒空 ⇒ **第三轴/兄弟边
      永远进不到合成分支**（一律 fail-closed）；
      ② 同一子端口既有分析层连接、又有合成映射 ⇒ ⇒ `Duplicate pin connection`
      ⇒ ⇒ 新增 `add_conn`：**幂等** + 同端口不同驱动 ⇒ **fail-closed**（不静默择一）
      ⇒ ⇒ 并把手里的合成连接在「未消费 output ⇒ 空连接」**之前**并入（否则同端口两个映射）

## T0h 批次 B2b-4b（G1 · 同类型多实例的序号按**实例路径字典序** · ✅ 已落地）

> 判据来源：**ADR-060 v1.12 §2.3** 的 **G1**（architect · 2026-11-05；
> 推导细节见 design.md §7.1.4）。
> ⚠️ **MUST 在 B3 加「同类型多实例」判据之前落地** ⇒ ⇒ 否则要动既有判据。

- [x] 0.31 序号分配改为**按 `HierarchyModule.key` 的字节字典序**（**组内**；先按
      sanitized `module_type_name` 分组）（现状 = `analysis.modules` 的遍历序 ⇒ ⇒
      依赖**组件创建顺序** ⇒ ⇒ 与 ADR-060 §2.3「按实例路径排序」不符）
      ⇒ ⇒ 理由：确定性 + **与 ctx 节点 ID 分配顺序无关**
      ⇒ ⇒ ⚠️ **MUST NOT 重排 `analysis.modules` 的 post-order 发射序**（只改**名字**）
      ⇒ ⇒ ⚠️ **同 base 不同类型 MUST 在编号前判身份**（现状只查**生成后**重名 ⇒ ⇒ 会把
      两个不同类型**静默**编成 `_0`/`_1` ⇒ ⇒ 违反 §2.3「同名不同类型 ⇒ 响亮拒绝」）
      ⇒ ⇒ 撞 ⇒ 响亮拒绝
      ⇒ ⇒ ⚠️ **实施记录（错误载体）**：落地形态是 `HierarchyEmitResult.error` + `ok == false`
      （发射器**统一**的校验失败模型 —— 一并覆盖既有的「生成名重复」检查），**不是**
      §2.3 字面的 `CHFATAL_EXCEPTION` 抛出 ⇒ ⇒ 面向上层仍是「响亮拒绝 + 点名两端」，
      且与「校验先于 I/O、失败可被调用方观测」不冲突；**若 architect 要求改抛异常 ⇒ 单独小批**
      ⇒ ⇒ ⚠️ **已知边界（如实登记）**：判据里的「类型身份」= **`module_type_name` 原文**
      （分析层 `HierarchyModule` 里**唯一**的类型维度）⇒ ⇒ 两个**真不同类型**若声明了
      **完全相同**的层名 ⇒ ⇒ 本层**无法区分**（会被当作同类型多实例编号）；档 ④
      （`ForceLayer` 回退 `typeid`）下不同类型的名字必不同 ⇒ ⇒ 该形态不可达
- [x] 0.32 **判据**（`tests/test_hierarchy_verilog.cpp` 的 `[g1]` 两个用例）：
      同一设计里两个**同类型**成层组件 ⇒ ⇒ module 名 = `<module_type_name>` /
      `<module_type_name>_1`（⚠️ **首份不加后缀** —— `numbered_name` 的既有约定，与 G1 正交）
      ⇒ ⇒ 且**交换组件的创建顺序 MUST NOT** 改变这两个名字（⇒ ⇒ 即「序号按路径字典序」的
      可观测形态）⇒ ⇒ 第二用例覆盖「同 base **不同类型** ⇒ 拒绝」+ 点名两个 module key
      + 不留文件
      ⇒ ⇒ **变异 2/2 全抓**：M1 退回「按遍历序」⇒ `top_ba.find("seq_leaf a (")` 转红 ·
      M2 去掉编号前身份判定 ⇒ `REQUIRE_FALSE(r.ok)` 转红；还原后 130 断言全绿
- [x] 0.33 ⚠️ **G0 的实现约束**（B2b-5 遵守；**以 design.md §7.1.4 v2 与
      ADR-060 v1.12 为准**）：**一个** chain builder（沿祖先链收集 → **返回六元组**
      · 幂等 · fail-closed）+ **两种 leaf adapter** —— 冒泡叶 = 无 driver 的
      `type_input`；第三轴叶 = reg/proxy（**需驱动重定向**）
      ⇒ ⇒ 冒泡与第三轴**共用**该 chain builder ⇒ ⇒ 两套实现 ⇒ ⇒ MUST 被
      「同一条机制」的判据抓住
      ⇒ ⇒ ⚠️ MUST NOT 实现成「一个函数 · 参数 = 起点节点 + 方向」的单函数形态
      （该形态是 2026-11-05 oracle 修正前的表述，已被 §7.1.4 v2 取代）
      ⇒ ⇒ **落地核对**：`build_chain(path, port_name, width, ports_are_input, leaf_is_port,
      leaf_port_name, leaf_redirect, driver_arg)` + `add_pass_port` / `add_conn` ——
      **链**与**叶策略**分离，`ports_are_input` 只决定端口方向（**不**是「起点节点 + 方向」
      那种把叶策略也压扁的形态）⇒ ⇒ B3 的冒泡直接调用同一条链

## T0i 批次 B2b-6（宽度守卫 + 两条变异欠账 · ✅ 2026-11-05）

> 判据来源：**ADR-060 v1.10 W1**（跨层边两端宽度 MUST 一致、合成前拒绝）—— 通用措辞
> **早已覆盖全部合成 site**，只是实现只在第三轴 / 兄弟边判了宽度 ⇒ 直接行 1 / 行 2 **静默扩展/截断**
> ⇒ ⇒ 本批是**实现追赶**，不是新裁决。

- [x] 0.34 **单一 `width_check` helper**（`src/hierarchy_verilog.cpp`）⇒ **全部**合成 site 同一调用点：
      直接行 1 / 行 2 / 第三轴 / 兄弟边 ⇒ ⇒ ⚠️ MUST NOT 每个分支各写一份（漏一处 = 静默截断且 lint 看不见）
      ⇒ ⇒ 诊断 = 点名两端方向与两个宽度值 + `Remedy:`（显式切片/扩展，或改驱动子模块显式端口）
- [x] 0.35 直接行 1 / 行 2 的合成连接改走 `add_conn`（与 B2b-5 同一幂等 + 双驱动 fail-closed 通道）
- [x] 0.36 **判据**（`[width]` 一个用例 / 4 组断言链）：双向（9→8 截断 / 8→9 扩展）×
      {`ch_reg` next, proxy} ⇒ ⇒ `ok=false` + **不留文件** + 诊断含 `width mismatch` /
      `9-bit driver -> 8-bit target`（或 8→9）/ `Remedy:`
      ⇒ ⇒ **守卫自身变异**（恒返回 true）⇒ 用例转红 · 0.24b 两条路径变异见上

## T0j 批次 B3a（冒泡 G3 + `UndrivenPolicy` + 向上 pass-through · ✅ 2026-11-05）

> 判据来源：**ADR-060 v1.13** §2.2 的 G3-1..G3-6 + §2.4.2；**ADR-058 v2.12** §2.3 ⑤。

- [x] 0.37 **冒泡端口在分析层登记**（G3-1）—— 成层组件的**悬空** `ch_in`（F2 缺口）与
      inline 折叠的悬空 `ch_in` 走**同一条**路径；顶层端口顺序由节点 id 决定（不因插入时机改变）
- [x] 0.38 **逐跳 pass-through 连接由分析层生成**（G3-2）—— ⚠️ 修掉**存量缺口**：
      原 inline 冒泡只登记端口、不生成连接 ⇒ 折进非根成层 module 时中间层空连接
      ⇒ 值传到中途断线且零报错；幂等 = 同名 + 同向 + 同宽（G3-3），否则响亮拒绝
- [x] 0.39 **链条机制抽成 `src/hierarchy_chain.h`**（G3-6）：分析层冒泡与发射层第三轴/
      向上 pass-through **共用** `plan_pass_chain`（端口名 + 逐跳实参规则单实现）
- [x] 0.40 **`UndrivenPolicy` 两格语义**（§2.4.2）：`Reject` ⇒ 不产出 + 列出**全部** +
      适用面收窄（只对 inline 折叠后 module 归属为**非根成层 module** 的悬空 `ch_in`）；
      `Bubble` ⇒ 逐条 `CHWARN` + 端口旁注释 + 产出；顶层 `synthesized_ports` 恒空（G3-5）
- [x] 0.41 **有 driver 的向上 pass-through**：每级合成 **output** 端口；底端是 node ⇒
      体内 `assign <port> = <node>;`；顶端是 reg/proxy ⇒ 合成**体内 wire** + **驱动重定向**
      （顶端是 input 端口 / op 等 ⇒ 拒绝）；⚠️ 端口名 MUST 用**合成名** `__up_<n>`
      （驱动节点名会在它自己的 module 内**重复声明**）；⚠️ 中间层 output 端口 MUST NOT 发
      `assign`（原先会发出 `assign X = ;`）
- [x] 0.42 **判据**：`[bubble]` ×3（三层冒泡 / inline 折叠 / `Reject` 双实例 + 防误拒）+
      `[upward]` ×1 ⇒ ⇒ 顶层端口顺序（bubbled input 在 `ch_out` **之前**）· **逐跳实连**
      （`.X(X)`，且全产物无 `.X()`）· 零父层引用 · 端口旁注释 · 每条悬空输入**恰好一条**告警 ·
      `Reject` 列出两个实例与各自层级链 · `Reject` 下根/inline 折叠的悬空 `ch_in` **仍产出**
      ⇒ ⇒ **变异 5/5 抓**：不生成冒泡连接 / 退回 F2 缺口 / 不加端口注释 / 只报首条 /
      关掉向上 pass-through
      ⇒ ⇒ ⚠️ **诚实边界**：本批判据是**连通性 + 零父层引用**的结构形态；**逐拍值判据**归
      **T3-⑤**（design §7.1.3 已登记的缺口）—— 不以结构断言冒充值正确性

## T0k 批次 B3b（端口完整性 G2 + 拒绝类点名断言 · ✅ 2026-11-05）

> 判据来源：**ADR-058 v1.12 §2.3** 的 **G2** + **ADR-062 §5** 端口完整性。

- [x] 0.43 **`HierarchyEmitResult.port_provenance`**（**value-only**）：`module` / `port` /
      方向 / 宽度 / `origin`（五类：Self · ClockReset · InlineFolded · Bubbled · Synthesized）
      ⇒ ⇒ **含注入的 clock/reset**（非根 module 的它们不在分析层端口表里）·
      **MUST NOT** 带 `lnodeimpl*` / 连接关系
- [x] 0.44 **完整性校验在发射期、第一次 I/O 之前**：`expected :=` 分析层端口 ∪ 合成端口 ∪
      注入 clock/reset · `actual :=` `ModuleView.ports` ∪ `synthesized_ports`
      ⇒ ⇒ 判据 = **`expected ⊆ actual`**（子集）+ 同 module **名字唯一** + 方向/宽度一致
      ⇒ ⇒ 机械形态抽成 `src/hierarchy_port_check.h`（纯函数 ⇒ 可单测）；失败 ⇒ `ok=false` + 点名
- [x] 0.45 **拒绝类点名断言**（实现已存在，**不重写**）：跨模块 `ch_mem`（两端 module key +
      两条替代路径）· 自驱动 `ch_in`（端口名）· 同 base 不同类型（两个 module key）·
      同类型多实例各发一份 · 父驱动子 `ch_out`（子侧端口名 + module key）·
      第七格（节点名 + module key）
- [x] 0.46 **判据**：`[provenance]`（五类来源 + 每个条目在产物里可找到 + 两次生成逐项相同）·
      `[provenance][guard]`（缺端口 / 方向或宽度不一致 / 重名 —— 直接喂端口表，正确实现下
      这些路径从公开 API 不可达）· `[reject][naming]`（mem + 自驱动）
      ⇒ ⇒ **变异 4/4 抓**：守卫恒通过 / 不填端口清单 / 清单漏合成端口 / 不查重名
      ⇒ ⇒ ⚠️ 分析层 `to_text()` 与既有字段顺序/命名**零改动**（4 份 golden 逐字节不变）

## T0l 批次 B3c（接口白名单 · 先证可达性 · ✅ 2026-11-05）

> 判据来源：**ADR-062 §2.6**（接口白名单）+ proposal Acceptance 的「违反 ⇒ 点名端口」。

### 可达性 spike（先做，结论决定本批内容）

分析层的端口循环**只从 4 类节点**构造端口表（`type_input` / `type_output` /
`type_clock` / `type_reset`）⇒ ⇒ 逐类核对：

| 违规类型 | 节点形态 | 能否进端口表 | 证据 |
|---|---|---|---|
| `ch_mem` | `type_mem` / `type_mem_read_port` / `type_mem_write_port` | ❌ **不可达** | 该循环把它们放进 `body`；跨模块 mem 在更早的分支**直接拒绝** |
| `ch_reg` | `type_reg` | ❌ **不可达** | 同上（`body`） |
| bundle / `ch_stream` 切片 | `type_proxy` | ❌ **不可达** | 成员不是端口 ⇒ `body`；跨层时被合成为**数据 wire 端口**（只有宽度、无接口类型） |
| `inout` | —— | ❌ **不可表达** | 框架单方向端口模型（`include/core/io.h`） |
| 用户显式声明 clock/reset | —— | ❌ **不可表达** | 框架按 `has_seq` 注入（`push_clock_reset`）；用户写 `ch_in<ch_bool>` 只是普通数据输入 |

⇒ ⇒ **结论**：四类违规**当前没有一条公开 API 路径**能进端口表。

### 本批落地

- [x] 0.47 **守卫**：`src/hierarchy_port_check.h` 的 `is_whitelisted_port_node`（决策表）+
      `check_interface_whitelist`（点名 `module` + `port` + 引 ADR-062）
      ⇒ ⇒ 允许 = `input` / `output` / `clock` / `reset`；其余（`mem*` / `reg` / `proxy` /
      `op` / `mux` / `bitsupdate` / `lit` / `none`）一律拒绝
- [x] 0.48 **检查点 = 端口表定稿后**（与 G2 同一段、I/O 之前）：只查 `ModuleView.ports`
      （**声明式端口**）⇒ ⇒ 合成端口是数据 wire（只有宽度）⇒ **不在其列**
      （否则与 B2b-3 的 proxy 形态冲突）
- [x] 0.49 **判据**（`[whitelist]`）：决策表**逐枚举值**断言（4 允许 + 10 拒绝）+
      拒绝时点名 `'bad'` / `'m'` / `ADR-062`
      ⇒ ⇒ **变异 2/2 抓**：白名单恒通过（允许侧）· 白名单过严（拒绝合法端口 ⇒ 既有用例成片转红）
- [x] 0.50 ⚠️ **如实登记的判别力边界**：守卫的**接入点**（`toVerilogHierarchical` 里的调用）
      **无法用公开 API 触发**（不可达）⇒ ⇒ 该行的「删掉调用」变异**无判据**
      ⇒ ⇒ 覆盖 = 决策表单测 + 与 G2 同段的代码位置审查；**MUST NOT** 为了「让判据红」
      而伪造一个走私路径（那会把非法形态合法化）

## T0m 批次 B4（护栏 R1–R7 + 自检投毒 + 探针矩阵 + CI · ✅ 2026-11-05）

> 判据来源：**ADR-060 §2.5**（lint 门的可执行定义）· ADR-058 §5（结构判据）·
> proposal Acceptance 的「护栏 R7」与「零回归」。

- [x] 0.51 **产物生成器** `tests/hier_rtl_products.cpp`：3 个设计（三层 / 行 1 / 冒泡）+
      `--check-reject mem` 模式 ⇒ ⇒ 在**真实进程**里产出，产物交给 verilator 判
      ⇒ ⇒ ⚠️ **MUST NOT** 注册成 ctest 用例（否则 `CTEST_TOTAL` 锚点会因一个非判据程序漂移）
- [x] 0.52 **R1 lint 门** = ADR-060 §2.5 的**唯一**可执行定义：`--lint-only --top-module <top>`
      ⇒ 不许 `-Wall`（合法产物会触发 `DECLFILENAME` / `PINCONNECTEMPTY` / `UNUSEDSIGNAL`）·
      不许 `-Wno-fatal`（会让 warning 不影响退出码 ⇒ 门失效）· 捕获退出码 · 额外交付
      「stderr 不含 `%Error`」· `lint_off` 白名单（仅 `WIDTHEXPAND` / `WIDTHTRUNC`）
      ⇒ ⇒ 开关纪律由 `check_lint_flags` 扫**唯一**的 `lint_file` 函数体（注释里提到它们不算）
- [x] 0.53 **R2** pin 名 ∈ 目标端口集 · **R3** 宽度一致（**剥掉 `lint_off` 后重新 lint**）·
      **R4** 两次生成 `diff -r` · **R5** 拒绝 + 消息子串 · **R6** 树同构 · **R7** 路径分离
- [x] 0.54 **`--self-test` ≥7 投毒** ⇒ 实落 **8 个 fixture / 9 条对照**：
      R1 未声明标识符 · R2 改 pin 名 · R3 改端口宽度（**配对断言：投毒后 R1 仍绿**）·
      R4 篡改产物 · R5 假工具输出 · R6 删实例化语句（**R1 仍绿**）· R7 同路径调用点 ·
      R1a 给 lint 调用塞 `-Wno-fatal` ⇒ ⇒ 每条「必红 ⇒ 还原复绿」，映射表见脚本头注释
- [x] 0.55 **探针矩阵补全**：轴 A（6 个合成 site）× 轴 B（6 类端点）= **36 格**，
      **16 Ok / 15 Reject / 5 Na**；`[matrix]` 用例逐格枚举 + 三态计数断言 +
      **证据 tag 真实存在**（删测试 ⇒ 转红）+ `Na` 必须有理由 + 11 格另有行为断言
- [x] 0.56 **如实登记的判别力边界**：① R7 只解析**字面量实参**（`path` 变量实参静态不可判
      ⇒ 由「层次化产物走独立输出目录」的约定承担）；② R2/R3 的判别力**依赖 verilator**
      （不是自研分析器 ⇒ 这是**有意**选择：MUST NOT 自研 Verilog 解析器）；
      ③ 本 change **不做值等价性** ⇒ 逐拍等价归 **T3-⑤**（R1–R7 全绿 ≠ 行为正确）

## T1 module 主体与实例化

> ⚠️ **B3 的三批映射（2026-11-05 第二次重排）**：`[B3a]` 冒泡（G3）+ `UndrivenPolicy`
> （含**存量 inline 冒泡缺连接**与**有 driver 的向上 pass-through**）；`[B3b]` 端口完整性（G2）
> + 拒绝类**点名断言**（实现已在 0.7 / 前置校验③ / T0h 落地，**不重写**）；
> `[B3c]` 白名单（**先证可达性**；只查 `Self` origin 声明式端口，合成端口不在其列）。
> ⚠️ **错误载体措辞待同步（critic SF-2）**：本文件与 proposal 里的「抛」「`CHFATAL_EXCEPTION`」
> 是 ADR-058 v2.12 ⑤ 之前的旧载体措辞 ⇒ 对外可观测形态 = `ok=false` + `error` 点名
> ⇒ ⇒ 在 B3b/B3c 落地时**一并订正**（MUST NOT 写 `REQUIRE_THROWS`）。

- [x] 1.1 每个成层组件产出一个 `module`（`<module_type_name>_<序号>`）
- [x] 1.2 父 module 内例化子 module + **实例端口映射** ⇒ ⇒ **零跨 module `assign`**
      （跨 module 的驱动边 ⇒ `suppress_driver_assign` ⇒ 值由实例端口映射提供）
- [x] 1.3 **[B3b]** **MUST NOT 复用** module 定义；同名不同类型 ⇒ 响亮拒绝（**点名断言测试** —— 实现已在 T0h / B2b-1 落地，不重写）
      ⇒ ⇒ **✅ B3b**：`[g1]` 断言「每个实例各发一份」（`module ` 计数 = 3）· 同 base 不同类型
      ⇒ 错误结果点名**两个 module key**（`top.a` / `top.b`）
- [x] 1.4 顶层 module 端口表 **逐字逐序**等于扁平端口表（clock → reset → input → output；
      **全 inline 时整份产物与扁平逐字节相同** —— 实测）

## T2 端口

- [x] 2.1 **[B3b]** 自身 `ch_in` / `ch_out` ⇒ 成层且非根 module 的端口集合 **⊇**
      ⇒ ⇒ **✅ B3b**：发射期 `expected ⊆ actual` + 名字唯一性（`src/hierarchy_port_check.h`）·
      `port_provenance` 五类来源可审计
- [ ] 2.2 隐式端口 ⇒ **[B2b-3 行 1 proxy / B2b-1 行 2 / B2b-5 第三轴 + 兄弟边]**
      （ADR-060 §2.1 矩阵 + **v1.9 第七/八格拒绝**）
- [x] 2.3 **[B3a]** 悬空冒泡（逐字同名 + 注释 + `CHWARN` + `UndrivenPolicy::Reject` ⇒ 不产出）
      ⇒ ⇒ **✅ 2026-11-05**：端口与**逐跳连接**都在分析层生成（G3-2）· 成层组件（F2）与
      inline 折叠两条路径**同一套** · 端口旁注释 = `// implicit: undriven input of <组件路径>`
      · `Reject` 列出**全部**悬空输入 + 冒泡层级链（双实例用例）
- [x] 2.4 clock / reset：根**无条件**发；非根**分开判**（clock ⇒ reg ∨ 写端口 ∨ sread；
      reset ⇒ reg ∨ sread）+ **需求向上传导**（子要 clock ⇒ 父也要 ⇒ 实例化不缺 pin）
- [x] 2.5 **[B2b-4 体内 wire ✅ / B3b 三角色总和]** `type_output` 三角色（自身端口 / 隐式输出 / **体内 wire**）
      ⇒ ⇒ **✅ B3b**：三角色各自的端口/体内落点由 `port_provenance`（Self / Synthesized）+
      「非根 module 端口表不含 inline 后代 ch_out」判据覆盖

## T3 拒绝与错误模型

- [x] 3.1 **[B3b]** 跨模块 `ch_mem` ⇒ 响亮拒绝（实现已在 B2a · 本批补**点名断言**：两条替代路径 + 两端 module）
      ⇒ ⇒ **✅ B3b**：`[reject][naming]` 断言 `cross-module ch_mem` + `'top.child'` / `'top'` + `(1)` / `(2)`
- [x] 3.2 **[B3b]** 自驱动 `ch_in` ⇒ 响亮拒绝（实现已在前置校验③ · 本批补点名断言）；子 module 内 MUST NOT `assign <ch_in> = ...`
      ⇒ ⇒ **✅ B3b**：`[reject][naming]` 断言 `drives its own` + 端口名 `unnamed_input` + 不留文件
      ⇒ ⚠️ **v1.9 第八格**：父层驱动子 `ch_out` 同型拒绝（**MUST NOT** 依赖白名单偶然形状）
- [x] 3.3 已 build 未 `add_child` ⇒ 抛 + `add_child` 修法（由分析层前置校验给出）+ 负向对照
      （`[hierarchy][emit][error]`：失败 ⇒ `ok=false` + **不留任何文件**）
- [x] 3.4 **[B3c]** 接口白名单违反 ⇒ **响亮拒绝**（对外 = `ok=false` + 点名端口；ADR-058 v2.12 ⑤）⇒ ⚠️ **先证可达性**
      ⇒ ⇒ **✅ 2026-11-05**：可达性 spike 结论 = 四类违规**当前均不可达**（详见 T0l）；
      守卫按防御性实现 + 决策表单测，不硬造 DUT
- [x] 3.5 错误模型：校验先于 I/O · 临时文件→rename · 失败**可被调用方观测**（返回结构体）·
      静态析构期用**返回码**兜底（不抛不写盘）

## T4 护栏

- [x] 4.1 **[B4]** `scripts/verify_hierarchical_rtl.sh`（lint 门 = **ADR-060 §2.5**：
      不加 `-Wall` · 捕获退出码 · stderr 不含 `%Error` · `lint_off` 白名单 · 禁用 `-Wno-fatal`）（零未声明引用 · 实例端口名 ∈ 端口集 ·
      **端口宽度一致** · 产物确定性 · 跨模块 `ch_mem` 被拒）
      ⇒ ⇒ **✅** R1–R7 全部落地（**R7** = 层次化产物路径 ≠ 扁平产物路径 · **适用面 = 仓内调用点**）
      ⇒ ⇒ **R3 的机制 = 剥掉 `lint_off` 后重新 lint**（那两条正是宽度告警的全局消音器）
- [x] 4.2 **[B4]** 判据 MUST 基于 `verilator --json-only` 的结构化提取（复用
      `scripts/lib/verilog_tree_extract.py`）⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器
      ⇒ ⇒ **✅** R6 的三元组由提取器给出；结构不变量（唯一根 / 每 module 恰实例化一次 /
      全可达 / 无环）在守卫脚本内对**提取结果**求值（不解析 Verilog 文本）
- [x] 4.3 **[B4]** CI 挂 **两处**（主路径 + `--self-test`）
      ⇒ ⇒ **✅** 挂在 **`pr-verilator-fast`**（⚠️ **不是** `build-and-test` —— 后者
      `BUILD_VERILATOR=OFF`，没有真 verilator ⇒ R1/R2/R3/R6 会空转）；`if: matrix.shard == 1`

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_hierarchical_rtl.sh --self-test
openspec validate hierarchical-rtl-emission-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
