# 任务清单：`hierarchical-rtl-equivalence-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical **ADDED ×2** + **MODIFIED ×5**）。
> ⚠️ **前置**：**T3-④**（层次发射）。
> ⚠️ **工作量 ~7–9 天**（oracle R13 实测：backend 的「只增不改入口」远不止加一个函数）。

## T1 backend 只增不改入口

- [x] 1.1 入口接收**显式端口集合**（`node_id` + 顶层端口名 + 宽度 + 方向）⇒ ⇒
      集合 MUST **⊇ {clock, reset}**
      ⇒ ⇒ **B3 已落地**：实际签名 `try_initialize_with_verilog(verilog_path, ports, data_map)`
      （`include/core/verilator_backend.h`）——**无 `work_dir` 形参**：work dir 来自构造器
      （`VerilatorBackend(verilog_path)` 的实参即 work dir）⇒ 两条路径的物理隔离由调用方
      **各构造一个实例**承担（见 1.6）；端口值类型 = `ch::VerilatorPortSpec`
      （`include/core/verilator_port_set.h`，value-only 中立头）· `⊇ {clock, reset}`
      在**碰 I/O 之前**校验（`[hier][negative]` 用例）
- [x] 1.2 `build_port_access_table` 改造：从 ctx 全量推 ⇒ 改为**显式集合**
      ⇒ ⇒ **B3 已落地**：新增 `build_port_access_table_from_specs()`
      （`port_access_.size() == ports.size()` 在 `[hier]` 用例断言）；
      既有 `build_port_access_table()` 保留且**未被替换**
- [x] 1.3 `emit_sim_main_postlude` 同上
      ⇒ ⇒ **B3 已落地**：新增 `emit_sim_main_postlude_for_ports(path, ports)`（**不读 ctx**）；
      固定前导抽成 `kSimMainPrelude`，两条路径共用（文本与既有逐字节**相同** —— 缓存 key 的输入）
- [x] 1.4 **既有 `initialize(ctx, …)` 行为逐字节不变**
      ⇒ ⇒ **B3 已验收**：`sim_main.cpp` 生成文本与原实现 **IDENTICAL**
      ⇒ ⇒ ⚠️ **证据 MUST 可复算**（F7）：**不**引用 `/tmp` 下的一次性比对脚本作判据
      （临时脚本随会话消失 ⇒ 归档后无人能复算 ⇒ 「证据」退化成**断言**）⇒ ⇒
      可复算形态 = **单一来源的结构性判据**：
      ① `src/core/verilator_backend.cpp` 里**两条路径**（`try_initialize_with_verilog` 的
      `:659` 与既有 `initialize` 的 `:733`）**都** `out << kSimMainPrelude`（同一个常量
      ⇒ 结构上**不可能**各写一份 ⇒ 漂移无处发生）⇒ 复算命令：
      `grep -c 'out << kSimMainPrelude' src/core/verilator_backend.cpp` ⇒ **MUST = 2**；
      ② `kSimMainPrelude` **仅有一处定义** ⇒ `grep -c '^static const char \*kSimMainPrelude'`
      ⇒ **MUST = 1**；
      ③ 行为面：`ctest -LE perf` 238/238 · 总时长 363s（与 B2 基线一致 ⇒ cache key 未漂
      ⇒ 无冷编译）
      ⇒ ⇒ 原 `/tmp/opencode/cmp_prelude5.py` 的字面量级比对（B3 当时的**辅助**手段）**降级为
      历史记述**，不作判据
- [x] 1.5 **E1 来源约束**：上述集合 MUST 由 **emitter 产物**（映射表 ∪ `port_provenance`）派生
      ⇒ ⇒ **MUST NOT** 由 harness / backend 独立撰写或手工列举（ADR-058 v2.14 §2.3 E1）
      ⇒ ⇒ **B3 已落地**：`ch::backend_port_set(emitted, root_module)`（`src/hierarchy_verilog.cpp`）；
      负向对照 = 抹掉映射表里一个顶层端口名 ⇒ 派生集合**跟着少一个**
- [x] 1.6 **E2 物理隔离**：与既有路径 **两个独立 work_dir**（共享 `verilator_work_dir_` ⇒ 互踩）
      ⇒ ⇒ **B3 已落地**：`[hier]` 用例用 `make_temp_dir("_hier_v")`（Verilog 产物）
      与 `make_temp_dir("_hier_b")`（backend work dir）两个目录，且与既有路径的实例**不同**
      ⇒ ⇒ **B4a 加固**：`[hier]`（E2 隔离）用例 = 两个**不同设计** × 两个 work_dir ⇒
      ① 每个 work_dir 的 `top.v` == **它自己**的产物（且两者内容**不同** ⇒ 共用 dir 时必然相同
      ⇒ 本判据对「实现忽略构造器实参」有判别力）；② 两个实例各自驱动自己的设计（0x2A→0x2A 寄存器通路 /
      0x2A→0x2B 组合通路）；③ 交叉再驱动一次（0x5C）确认无串味
- [x] 1.7 **E3**：失败 MUST **致命**（返回 `VerilatorError`，**MUST NOT** 沿用
      `CHWARN` best-effort）；harness MUST 断言 backend **真初始化成功**（非 SKIP 即过）
      ⇒ ⇒ **B3 已落地**：`compile_and_load(/*fatal=*/true)` 路径；`[hier]` 用例 `REQUIRE(init.has_value())`
      （**不 SKIP**）+ 值判据 0x2A / 0x5C（手推：若 clock/reset 未接 ⇒ 寄存器冻结 ⇒ 恒 0 ⇒ 转红）
      ⇒ ⇒ **B4a 加固**：`[hier][negative]`（编译失败投毒）= 端口集合取自**真实产物**（前置校验必过）
      + 产物换成**语法错误**的 Verilog ⇒ MUST 返回 `VerilatorError` 且 `code == VerilatorCompile`
      + 端口表**不得半初始化**（`port_access_snapshot()` 空 · `clock_node_id() == UINT32_MAX`）
      ✅ **顶层名条款已按 ADR-058 v2.16 订正**（用户 2026-11-06 拍板「订正为文件名」）：约束对象 =
      **编译输入文件名**（主干 MUST 是 `top.v`），**不是**顶层 module 名 ⇒ ⇒
      「顶层 module 名 MUST NOT 影响可消费性」（`top_module_name = "my_top"` 照常可仿真）⇒ ⇒
      **不再做**「按 module 名 fail-early」（该判据无失败模式支撑 · 会拒绝本可工作的产物）⇒ ⇒
      落地形态 = 入口**复制**外部产物到 `<work_dir>/top.v` 再编译（**已落地**）+ 编译命令
      MUST NOT 传 `--prefix` / `--top-module`（实测 `--top-module my_top` ⇒ `Vmy_top.h` ⇒
      `Vtop.h` 消失）+ 缓存 key 只哈希**内容** ⇒ 复制不改变 key ⇒ ⇒
      判据：`[hier][filename]`（调用方路径主干 ≠ `top` ⇒ 仍 MUST 能编译 + work_dir 里的编译输入名
      MUST 是 `top.v` ⇒ **变异**：去掉复制 ⇒ 转红）+ `[hier][topname]`（`my_top` 产物 MUST 可消费，
      值判据 0x2A / 0x5C ⇒ **反例**：按 module 名拒绝 ⇒ 转红）⇒ ⇒
      证据与裁决：`.rddf/reviews/hierarchical-rtl-equivalence-2026-11-discovery.md` ·
      `.rddf/reviews/ADR-058-v2.16.md`

## T2 映射表与观测集

- [x] 2.1 emitter 产出**四元组**映射表（`node_id` → 名 → 扁平位置 → 层次位置集合），
      载体 = `HierarchyEmitResult` 的**新增公开字段**（value-only · `hier_top_port_name` 允许为空）
      ⇒ ⇒ **B1 已落地**（`SignalMapEntry` / `SignalLocation` · `include/hierarchy_verilog.h`）：
      发射器**在同一次调用内**产出（不另开 manifest 入口）· 顺序 = `node_id` 升序（确定性）·
      发射前过 `check_signal_map`（`src/hierarchy_signal_map_check.h`）⇒ 不满足 ⇒ `ok=false`
      且**不产出文件**；判据 = 顶层端口全覆盖 · `flat.is_port` ⇔ 顶层端口名非空 ·
      「声称在顶层 ⇒ MUST 真的有那个位置」· 「顶层端口名为空 ⇒ MUST 真的不在顶层」
      （变异 M-B1：漏记一个顶层端口的位置 ⇒ 转红 ✓）
- [x] 2.2 `O` 由测试**手写声明**；MUST ⊇ DUT 全部 `ch_out`；MUST 非空
      ⇒ ⇒ **B5 已落地**（`tests/hier_equivalence.h` 的 `Observation` 列表 · 调用方手写）：
      **全 inline** DUT 的 `O` = {`top.dout`, `leaf.dout`}（**全部** `ch_out` ⇒ 叶子的也在顶层 ⇒ 可观测）；
      **成层** DUT 的 `O` = {`top.dout`} —— ⚠️ 成层子树的 `ch_out` 在层次化顶层**不可观测**
      ⇒ ⇒ 本条的下限 MUST 读作「**两侧顶层都可见**的 `ch_out`」+ 盲区**显式登记**
      （ADR-058 §4 风险 6 ⇒ MUST NOT 声称「全部输出已比对」）—— **口径订正已登记**
- [x] 2.3 **「随激励变化」的机械判据**（两组激励 ⇒ 取值集合不同）
      ⇒ ⇒ **B5 已落地**：同一 DUT 跑两组激励（A = 0x11/0x22/0x33 · B = 0x44/0x55/0x66）⇒
      逐观测信号比较**取值集合** ⇒ 至少一个不同 ⇒ 否则 `O` 全恒 0 ⇒ 「逐拍全等」恒绿

## T2b 顶层端口集合判据（结构面 · 零仿真）

- [x] 2b.1 输入端口集合 MUST **相等**（名字 + 宽度逐项配对 · **双向**）+ 输出端口集合 MUST
      **等于**「扁平输出集 − 被移出顶层的那批」（**双向**）⇒ ⇒ 判据函数落在
      `src/hierarchy_port_set_check.h`（**发射器侧与单测共用同一份** ⇒ 否则单测只能证明
      「自己和自己一致」）⇒ ⇒ ⚠️ 期望集**手推**（MUST NOT 从层次化产物反推 ⇒ 那是
      「实际集 − 实际集」的**循环定义** ⇒ 恒绿 ⇒ 「不减法」这类缺陷抓不住）
- [x] 2b.2 产物级载体：护栏 **R8**（`scripts/verify_hierarchical_rtl.sh`）—— 从
      `verilator --json-only` **结构化提取**顶层端口表
      （`scripts/lib/verilog_tree_extract.py --ports` ⇒ `isPrimaryIO` + dtype 的 `range`
      ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器）⇒ ⇒ 与**手推期望集**逐项比对
      （名字 + 方向 + 宽度）⇒ ⇒ 同时判掉「成层子树的 `ch_out` 跑到顶层」
      ⇒ ⇒ 投毒对照：改顶层端口宽度 ⇒ R8 红（而 R1 仍绿 ⇒ 证明不是 R1 的重复）

## T3 等价性用例

- [x] 3.1 5 个 DUT（**清单锁定**：两层 / 三层+兄弟连线 / bundle 成员跨层 / 含 `ch_mem` /
      真实最深设计 ⇒ ⇒ 后两者的粒度矩阵见 design §6.2：**只跑 `RespectDeclarations`**）
      ⇒ ⇒ **B5 已落 2 个**（全 inline 两层 / 成层两层 ⇒ 成层那层用 ADR-060 §2.1 **行 2**
      「父层读子内部寄存器」⇒ ⇒ 才有**隐式端口**供 3.5 用）⇒ ⇒ **B6 已落其余 3 个**：
      **三层 + 兄弟连线**（`a` 的输出驱动兄弟 `b` ⇒ 兄弟边）/ **bundle 成员跨层**
      （父层驱动子层 bundle 字段 ⇒ proxy 子形态 ⇒ 隐式端口）/ **含 `ch_mem`**（mem 在**成层子
      module 内**自用 ⇒ 父层只读子**端口** ⇒ **不是**跨模块 mem 读 ⇒ 合法）/ **真实最深设计**
      （裁剪版流水线：`ch_reg` + **dmem** + **2 个声明成层的流水级**；顶层端口宽度 8 bit ≤ 64
      ⇒ ⇒ ⚠️ **不含 imem**（无取指级）⇒ 与裁决文字「imem/dmem」的差距**如实登记**）
      ⇒ ⇒ 5 个 DUT 全覆盖（2 + 3）
- [x] 3.2 逐拍全等；**多拍激励** + **至少一个寄存器**
      ⇒ ⇒ **B5 已落地**：`tests/test_hierarchy_equivalence.cpp` 的 `[equivalence][layered]`
      —— 激励 = **3 拍**（0x11/0x22/0x33）· DUT 含 `ch_reg` ⇒ 寄存器通路被真正走通
      ⇒ 逐拍比对 `O`（`flat[O] ≡ hier[O]`）⇒ 全等
- [x] 3.3 `ForceInline` vs `ForceLayer` 逐拍全等 ⇒ ⚠️ **仅用于不含跨模块 `mem` 读的小 DUT**
      （含 `ch_mem` / 深设计在 ForceLayer 下会被**合法拒绝** ⇒ 无可执行对象，见 design §6.2）
      ⇒ ⇒ **B6 已落地**（`[equivalence][granularity]`）：3 个 DUT（两层 / 三层+兄弟 / bundle 跨层）
      × {`ForceInline`, `ForceLayer`} ⇒ 逐拍全等 ⇒ ⇒ 观测面 MUST 取**两种粒度下都可见**的信号
      （否则读不到的一侧会被误判成分歧）⇒ ⇒ harness 的 `emit` 新增 `Granularity` 形参
      （`GranularityScope` 是**静态**开关 ⇒ 只包住发射 ⇒ 不泄漏到别的阶段）
      ⇒ ⇒ ⚠️ **实测修正**：深设计的「alu → memstage」是**兄弟边** ⇒ 若根同时读 `alu.y`
      ⇒ 「同一子端口两个驱动」⇒ 发射器**合法拒绝** ⇒ ⇒ 改成根回读**另一个**端口（`alu.state`）
      ⇒ ⇒ 该拒绝形态**已被如实登记**（不是绕过）
- [x] 3.4 同一 `node_id` 名字逐字相同（按映射表索引比对）
      ⇒ ⇒ **B5 已落地**：按映射表的 `node_id` **逐项**比对 ctx 级全局名表（MUST NOT 只比
      「名字集合交集」）⇒ 逐字相同
- [x] 3.5 判别力自证 **≥2 形态**（删连线 · 改隐式端口方向）⇒ 两红 ⇒ 还原复绿
      ⇒ ⇒ **B5 已落地**（`[equivalence][mutation]`）：**形态 1** 删一条连线（实例化里的
      `.top_leaf_unnamed_input(...)`）⇒ 红（**编译期**：Verilator `%Warning-PINMISSING` ⇒ fatal）；
      **形态 2** 改隐式端口方向（子侧 `output [7:0] __x_0` ⇒ `input`）⇒ 红（**编译期**：
      `%Error-ASSIGNIN`）；**形态 3（补）** 改连线表达式（连 `8'h00`）⇒ 产物**仍能编译** ⇒
      红来自**值分歧** ⇒ ⇒ 证明「逐拍全等」这条判据**本身**有判别力（形态 1/2 只能证明
      「harness 会发现异常」）⇒ ⇒ 还原 ⇒ **复绿**

## T4 确定性与零回归

- [x] 4.1 同一设计两次生成**字节相同**（同 ABI 内）
      ⇒ ⇒ **B7 已落地**：`[equivalence][determinism]`（同一 ctx 两次发射）⇒ ① 产物**逐字节**相同
      + ② 映射表**逐项**相同（**顺序**也相同 ⇒ 不是「集合相等」的弱判据）+ ③ 顺序 MUST 是
      `node_id` **升序**（确定性的一**可机械判定**的形态）
      ⇒ ⇒ **跨进程**那一侧由护栏 **R4** 覆盖（两次生成 `diff -r`）
      ⇒ ⇒ ⚠️ **变异 M-B7-1（实测 3 次，如实登记）**：① **按地址排序**（`key.c_str()` 比较）
      ⇒ **恒绿**（分配器的单调性 ⇒ 相对序不变）⇒ **无效变异**（不是判别力缺口）；
      ② **进程级随机种子**打乱 module 顺序 ⇒ 被 **R1 lint 门**抓住（产物非法 ⇒ 证明 module 顺序
      **受依赖顺序约束**：子先于父 ⇒ ⇒「顺序依赖」的可注入空间很小）；③ **进程内静态计数器**
      反转**映射表顺序** ⇒ ⇒ **本判据转红** ✓（⇒ ⇒ 「顺序」确实是契约的一部分）⇒ 还原复绿
- [x] 4.2 产物通过 **ADR-060 §2.5 的 lint 门**（退出码 + 无 `%Error` + `lint_off` 白名单；
      ⚠️ **不加** `-Wall`**、**禁** `-Wno-fatal`）
      ⇒ ⇒ **已由护栏 R1 覆盖**（`scripts/verify_hierarchical_rtl.sh` · 3 个产物 · exit 0）
- [x] 4.3 `ctest -LE perf` / samples / examples 全绿 · 计数锚点**按新增用例同步更新**
      （⚠️ 等价性用例带 `verilator_e2e` 标签 ⇒ `VERILATOR_E2E_TOTAL` 必变；
      parity 两锚点（66 / 289）**不变** —— 等价性用例不是 `DUAL_BACKEND_TEST_PAIR`）
      ⇒ ⇒ **B5 已同步**：`CTEST_TOTAL` 249 → **250**（新 ctest 条目）·
      `VERILATOR_E2E_TOTAL` 75 → **76**（新条目带 `verilator_e2e` 标签 ⇒ 且 MUST 登记进
      capability 的**契约锁定清单** ⇒ 已由独立小 change `dual-backend-lock-register-hier-equivalence-2026-11`
      落地并归档）· parity 两锚点（66 / 289）**不变** ✓
      ⇒ ⇒ **B7 已加 P8 守卫**：`scripts/verify_hierarchical_p8.sh`（R1 `include/` 无 harness 头 ·
      R2 `include/` 无 harness 符号 · R3 harness 在 `tests/` · R4 公开 API 面 ⊆ **已裁决的 4 个公共头**
      （`codegen_verilog.h` · `hierarchy_verilog.h` · `core/verilator_backend.h` ·
      `core/verilator_port_set.h`）⇒ ⚠️ R4 需 git 历史 ⇒ 浅克隆环境**明确 NOTE 跳过**（不静默假装绿））
      + `--self-test`（合法清单 ⇒ 绿 · 越界清单 ⇒ 红）
## T3-⑤ 收口（14 项 · 2026-11-06 实施后架构审查的待办）

> 权威来源：`.rddf/reviews/T3-005-equivalence-debt-review.md`（9 项 F1–F9）+
> planner 落档的 F10–F14（`.rddf/roadmap/features/feat-hierarchical-rtl-output.md` §「T3-⑤ 收口与后续规划」②）。
> ⚠️ **口径类裁决（F1/F5/F9）归 delta / proposal / roadmap**（归档后主 spec 是唯一权威）⇒ 本节只记**动作与证据**。

- [x] F1 `proposal.md` 的 `O` 下限措辞 ⇒ 引用 delta 的两阶段口径 —— **planner 已改**（本 change 不动）
- [x] F2 盲区断言做成**对偶**（被观测侧 + 盲区侧，均绑到映射表）
      ⇒ ⇒ `check_observation_surface` 的断言 **①**（被观测侧）与 **④**（盲区侧）成对
- [x] F3 「每个被观测信号 ∈ 映射表 ∧ `hier_top_port_name` 非空」⇒ ⇒ 断言 **①**
- [x] F4 「两次 emit 共享同一 ctx」改成**可失败**形态
      ⇒ ⇒ `HierarchyEmitResult::source_ctx`（`include/hierarchy_verilog.h:121` ·
      赋值 `src/hierarchy_verilog.cpp:1400`）+ 四条断言（`tests/test_hierarchy_equivalence.cpp:1235-1238`）
      ⇒ ⇒ ⚠️ **不违反 value-only 纪律**（那条只禁 `lnodeimpl*` / 连接关系；ctx 指针是**身份锚**）
      ⇒ ⇒ **理由登记**：只比 `node_id` **不能**证明同 ctx（两个 ctx 的编号空间可能碰巧重合）
- [x] F5 roadmap **P0-4** 的「三处齐」口径改读 —— **planner 已改**（本 change 不动）
- [x] F6 修 `deep_obs` 的**死重观测** ⇒ 兄弟叶真实输出 `a.dout` / `b.dout`
      + `REQUIRE(inline_obs.size() == 4)` 钉条数（防再 slip）
- [x] F7 `tasks.md` 的 prelude 证据引用改为**可复算**形态（见 §T1 1.4）
- [x] F8 用法指南 **P7 痛点行**同步 ⇒ **交 T3-⑥ D1**（本 change **不改**该文档）—— 已登记
- [x] F9 feature 文件的 `O` 措辞 3 处订正 + 残留 1b 指针 + P8 归位条件 —— **planner 已改**（本 change 不动）
- [x] F10 **修 2 个红灯** + **反向对照**：
      ① 口径落地 = `ObsMode`（`FullCoverage` / `PublicSubset`）+ `ObsPlan.exempt` 显式豁免清单
      （机械载体 · MUST NOT 仅注释）+ 六段断言 `check_observation_surface`
      ⇒ ⇒ **红灯消解**：9 用例 / **414 断言**全绿（原 2 红 = `deep` 的 `inline_obs.size()` 1≠4 ·
      `mid` 未声明成层导致 `RespectDeclarations` 零增量）
      ② **反向变异对照**（防「删掉盲区断言」式为绿而绿 · 每条均**先确认能编译** · 还原后重建复绿）：
      | 变异 | 形态 | 结果 |
      |---|---|---|
      | **M-F10-2a** | `plan_for(EqDeepTop)` 的 `b` 拨回固定 `ForceLayer`（= 单粒度被当成跨粒度） | ✅ **转红**（`inline_obs.size()` 1≠4） |
      | **M-F10-2b** | 删掉 `exempt` 登记（可见但未观测者不再进豁免清单） | ✅ **转红**（断言 ④ 盲区侧：`mid.dout` 可见却既未观测也未豁免） |
      | **M-F10-2c** | 跨粒度下 `mode` 谎报 `FullCoverage` | ✅ **转红**（断言 ⑥：`exempt` 非空） |
      | **M-F10-2d** | 手写 `all_ch_outs` 漏掉 `leaf.dout`（同时不进 `O`、不进 `exempt`） | ✅ **转红**（断言 ⑤ **独立锚**：`top_leaf_unnamed_output` 未被 `O ∪ exempt` 覆盖） |
      | **M-F10-2e** | **叠加**：删掉 `REQUIRE(inline_obs.size() == 4)` + 2a | ⚠️ **恒绿** ⇒ **如实登记为无效变异**：它证明的是「钉条数的断言本身承重」（删了它 2a 就逃逸）⇒ 不是判别力缺口 |
      ⇒ ⇒ 还原后 `All tests passed (414 assertions in 9 test cases)`
- [x] F11 **`[deep]` 诚实性缺口 ⇒ 用 DUT 改造解决**（不只订正注释）
      ⇒ ⇒ `EqDeepMid` 加 `CH_HIERARCHY_LAYER("eq_deep_mid")` ⇒ `RespectDeclarations` 成为
      **唯一未覆盖的「中间拓扑」**（原三者均无声明 ⇒ `RespectDeclarations` ≡ `ForceInline` ⇒ 零增量）
- [x] F12 **断言 ⑤ 的独立锚**（打破「`O` 清单与 DUT 清单同时漏项 ⇒ 恒绿」）
      ⇒ ⇒ 从**映射表**（emitter 产物）导出「双顶层可见」的 output 候选
      （排除 `default_clock` / `default_reset` 与顶层 `ch_in`）⇒ MUST 被 `O ∪ exempt` 覆盖
      ⇒ ⇒ ⚠️ **不做顶层端口计数校验**（planner 裁决）：扁平侧顶层端口 = **所有** `ch_in` +
      **所有** `ch_out` + clk/rst ⇒ 只列顶层 `in` 的清单**算不出** ⇒ 会**假红**
      ⇒ ⇒ 独立锚由**逐项点名**承担（不依赖计数）
- [x] F13 **`[granularity]` 的第三粒度** ⇒ 三粒度矩阵
      ⇒ ⇒ deep DUT 补 `RespectDeclarations` 格（**F11 先行**才有意义）· 两层/bundle 的
      `RespectDeclarations` ≡ `ForceLayer` ⇒ **零增量** ⇒ 刻意不加（不是遗漏）
- [ ] F14 **台账**（architect 域）：残留 **13**（backend 改造工作量重估 ~7–9 天）已随 T3-005 实施完成
      而**失效** ⇒ MUST 标 ✅ 关闭；残留 **11** 与 feature §334 的 P6/P8 表**重复** ⇒ 去重（权威归台账）
      ⇒ ⚠️ **本 change 不动**（architect 域 · 须切 architect 或代记账 · planner 已在 roadmap 登记）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
ctest -L verilator_e2e --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-equivalence-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```

## 归档前判据（V1 · D4 的闭环 · 正反两面）

> 台账 §11.11.2 的 V1：主 spec 的 5 条 Requirement 已由本 change 的 delta 以 **MODIFIED** 修订
> ⇒ ⇒ 这两条 grep 是**归档门**，任一条不满足 ⇒ **不得归档**。

```bash
S=openspec/specs/rtl-hierarchical-output/spec.md
# 正面：断言面的 7 行必须清零（本 change 开工前 = 7）
grep -cE -- '-Wall|`strict`|MUST \*\*(抛异常|拒绝)|MUST 改为抛异常' "$S"
# 反面：合法的「MUST NOT 依赖抛异常」必须仍在（防「为清零而误删」）
grep -c '抛异常' "$S"
```

⇒ ⇒ 归档时 MUST 分别为 **0** 与 **1**（1 = 仅 `:398` 的静态析构期禁令）。
