# 任务清单：`hierarchical-rtl-emission-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical ADDED ×7）。
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
- [ ] 0.24b **R2 的四条路径逐条变异**（critic 建议 · ADR-060 v1.10 R2）：
      把 B2b-3 的 M3 变异**套用到另两条路径** —— `print_input` 的 driver walk
      （当前守卫 = 只穿透未命名 proxy）· `print_proxy` 的 `src(0)`（当前守卫 = early-return）
      ⇒ ⇒ 确认**各有专属判据** ⇒ ⇒ ⚠️ 目的 = 证明「每条 `src(0)` 解析路径都独立受约束」
      （ADR-060 v1.10 R2 的契约字面覆盖**所有**这类路径 ⇒ ⇒ 未来新增第五条时不被漏掉）
- [ ] 0.24c **W1 的负向用例**（ADR-060 v1.10 §5 的宽度判据）：父侧 **9-bit** 节点驱动
      子侧 **8-bit** 端点（`ch_reg` next 与 proxy 各一）⇒ ⇒ MUST 在**合成之前**拒绝
      ⇒ ⇒ 诊断 MUST 点名两端 + 两个宽度值 ⇒ ⇒ MUST NOT 产出文件
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

## T0g 批次 B2b-5（第三轴 pass-through + 兄弟边 · 待实施）

- [ ] 0.28 **第三轴**：driver 的 `owner` 在更上层（含 `nullptr`）⇒ 沿祖先链逐级**
      同名** `input` 端口（每级一个）
- [ ] 0.29 **兄弟边**：子 A 输出 → 子 B 输入 ⇒ 共同祖先内**一根** wire + 两处实例映射
      （**MUST NOT** 兄弟直连）
- [ ] 0.30 **判据**：第三轴每级一个同名端口 + 顶层可驱动（值判据）；兄弟边恰好一根连接网 +
      **零**隐式端口；两者均过 §2.5 的 lint 门

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
- [ ] 0.33 ⚠️ **G0 的实现约束**（B2b-5 遵守；**以 design.md §7.1.4 v2 与
      ADR-060 v1.12 为准**）：**一个** chain builder（沿祖先链收集 → **返回六元组**
      · 幂等 · fail-closed）+ **两种 leaf adapter** —— 冒泡叶 = 无 driver 的
      `type_input`；第三轴叶 = reg/proxy（**需驱动重定向**）
      ⇒ ⇒ 冒泡与第三轴**共用**该 chain builder ⇒ ⇒ 两套实现 ⇒ ⇒ MUST 被
      「同一条机制」的判据抓住
      ⇒ ⇒ ⚠️ MUST NOT 实现成「一个函数 · 参数 = 起点节点 + 方向」的单函数形态
      （该形态是 2026-11-05 oracle 修正前的表述，已被 §7.1.4 v2 取代）

## T1 module 主体与实例化

- [x] 1.1 每个成层组件产出一个 `module`（`<module_type_name>_<序号>`）
- [x] 1.2 父 module 内例化子 module + **实例端口映射** ⇒ ⇒ **零跨 module `assign`**
      （跨 module 的驱动边 ⇒ `suppress_driver_assign` ⇒ 值由实例端口映射提供）
- [ ] 1.3 **[B3]** **MUST NOT 复用** module 定义；同名不同类型 ⇒ `CHFATAL_EXCEPTION`
- [x] 1.4 顶层 module 端口表 **逐字逐序**等于扁平端口表（clock → reset → input → output；
      **全 inline 时整份产物与扁平逐字节相同** —— 实测）

## T2 端口

- [ ] 2.1 **[B3]** 自身 `ch_in` / `ch_out` ⇒ 成层且非根 module 的端口集合 **⊇**
- [ ] 2.2 隐式端口 ⇒ **[B2b-3 行 1 proxy / B2b-1 行 2 / B2b-5 第三轴 + 兄弟边]**
      （ADR-060 §2.1 矩阵 + **v1.9 第七/八格拒绝**）
- [ ] 2.3 **[B3]** 悬空冒泡（逐字同名 + 注释 + `CHWARN` + `strict` 抛）
- [x] 2.4 clock / reset：根**无条件**发；非根**分开判**（clock ⇒ reg ∨ 写端口 ∨ sread；
      reset ⇒ reg ∨ sread）+ **需求向上传导**（子要 clock ⇒ 父也要 ⇒ 实例化不缺 pin）
- [ ] 2.5 **[B2b-4 体内 wire / B3 三角色总和]** `type_output` 三角色（自身端口 / 隐式输出 / **体内 wire**）

## T3 拒绝与错误模型

- [ ] 3.1 **[B3]** 跨模块 `ch_mem` ⇒ 响亮拒绝（判定 = **inline 折叠后**归属）
- [ ] 3.2 **[B3]** 自驱动 `ch_in` ⇒ 抛；子 module 内 MUST NOT `assign <ch_in> = ...`
      ⇒ ⚠️ **v1.9 第八格**：父层驱动子 `ch_out` 同型拒绝（**MUST NOT** 依赖白名单偶然形状）
- [x] 3.3 已 build 未 `add_child` ⇒ 抛 + `add_child` 修法（由分析层前置校验给出）+ 负向对照
      （`[hierarchy][emit][error]`：失败 ⇒ `ok=false` + **不留任何文件**）
- [ ] 3.4 **[B3]** 接口白名单违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口
- [x] 3.5 错误模型：校验先于 I/O · 临时文件→rename · 失败**可被调用方观测**（返回结构体）·
      静态析构期用**返回码**兜底（不抛不写盘）

## T4 护栏

- [ ] 4.1 **[B4]** `scripts/verify_hierarchical_rtl.sh`（lint 门 = **ADR-060 §2.5**：
      不加 `-Wall` · 捕获退出码 · stderr 不含 `%Error` · `lint_off` 白名单 · 禁用 `-Wno-fatal`）（零未声明引用 · 实例端口名 ∈ 端口集 ·
      **端口宽度一致** · 产物确定性 · 跨模块 `ch_mem` 被拒）
- [ ] 4.2 **[B4]** 判据 MUST 基于 `verilator --json-only` 的结构化提取（复用
      `scripts/lib/verilog_tree_extract.py`）⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器
- [ ] 4.3 **[B4]** CI 挂 **两处**（主路径 + `--self-test`）

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
