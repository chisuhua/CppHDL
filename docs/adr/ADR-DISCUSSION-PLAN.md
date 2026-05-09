# ADR 讨论计划表

> **目的**: 按优先级排列所有待讨论的 ADR 议题，追踪讨论进度和决策状态
>
> **状态说明**: 已完成 / 进行中 / 待讨论 / 已推迟

---

## ✅ 已完成议题

| 编号 | 议题 | ADR 文档 | 讨论日期 | 关键决策 |
|------|------|---------|---------|---------|
| #1 | 非对称内存模型 | **ADR-007** §2-§5 | 2026-05-06 | D1: 移除 destruction_manager; D2: clear_sources 清理 users_; D3: Simulator 生命周期短于 context 为硬约束; D4: 异常安全不特殊处理 |
| #2 | `data_map_` 共享可变状态 | **ADR-007** §6 | 2026-05-06 | Q1: 改用 vector<sdata_type>; Q2: 接受 O(4N) + 轻量脏标志; Q3: 生命周期保持现状; Q4: DAG 依赖保持现状 |
| #3 | 隐式单时钟域约束 | **ADR-008** | 2026-05-06 | D1: 不支持多时钟域（CDC); D2: Async FIFO 不启用; D3: 代码标注 CDC 不支持 |
| #4 | 仿真评估顺序 | **ADR-009** | 2026-05-06 | D1: 采用边沿检测; D2: 双 eval → 边沿触发; D3: 输入分类保持现状 |
| #5 | `<<=` DAG 连接语义 | **ADR-010** | 2026-05-06 | Q1: 统一 set_src; Q2: static_cast 替代 dynamic_cast; Q3: CHREQUIRE 替代 TODO; Q4: 保留 && || 加 TODO |
| #6 | zero_cache/ones_cache 线程安全 | **ADR-011** | 2026-05-07 | D1: 混合策略，≤64 无缓存 + >64 thread_local |
| #9 | Bundle 反射系统 | **ADR-014** | 2026-05-07 | Q1: 10 字段限制立即修复（单独任务）; Q2: 文档化 LSB-first; Q3: 延后 add_user() |
| #10 | 拓扑排序作为单一 IR | **ADR-015** | 2026-05-07 | Q1: 保持单一 IR; Q2: 文档化确定性; Q3: 改进环路错误信息 |
| #11 | `ch_reg<T>` 继承设计 | **ADR-016** | 2026-05-07 | Q1: 保持继承; Q2: 延后优化; Q3: 不推荐组合 |
| #12 | 双 `in_static_destruction()` | **ADR-017** | 2026-05-08 | Q1: 保留原子标记; Q2: 移除简单 bool; Q3: 精简 destruction_manager（修正 ADR-007 D1） |
| #13 | Verilog 代码生成完整性 | **ADR-019** | 2026-05-08 | Q1: 批量+分阶段; Q2: mux→concat→bit_sel→sext/zext→shifts→neg; Q3: 移除空 catch; Q4: iverilog 编译检查+黄金文件 |
| #14 | DAG 代码生成器定位 | **ADR-020** | 2026-05-08 | Q1: 重新归类为可视化工具; Q2: 提取共享基类; Q3: 同步移除空 catch; Q4: 保留 |
| #15 | JIT 操作覆盖策略 | **ADR-021** | 2026-05-08 | Q1: FAIL FAST; Q2: 先移除 CONCAT 映射; Q3: 批量松散对齐; Q4: 补 AND/OR/XOR mask |
| #16 | SpinalHDL 流管道操作 | **ADR-022** | 2026-05-08 | G1: 不在聚合头; G2: 成员函数孤件; G3: stream_operators.h 孤立; G4: halfPipe 语义待确认; G5: 测试未验证编译 |
| #17 | 仿真原语评估 | **ADR-023** | 2026-05-08 | Oracle 调研: fork/join 推迟，仅加 simTimeout；其余跳过 |
| #18 | BlackBox/IP 集成评估 | **ADR-024** | 2026-05-08 | 零需求，低优先级搁置，等外部 IP 集成需求触发 |
| #19 | 形式验证接口评估 | **ADR-025** | 2026-05-08 | 零 formal 工具链集成，搁置；CH_ASSERT 宏空壳是独立问题 |
| #20 | 移除 CH_MODULE 宏 | **ADR-026** | 2026-05-08 | 已删除宏定义，替换 2 处使用为 ch::ch_module<T> |
| #21 | LLVM 版本硬编码修复 | **ADR-027** | 2026-05-08 | LLVM-18 → LLVM-${LLVM_VERSION_MAJOR}，QUIET → REQUIRED，WARNING → FATAL_ERROR |
| #22 | 已禁用测试处理 | **ADR-028** | 2026-05-08 | 保留 TODO 注释，记录禁用测试清单 |
| #23 | 命名约定一致化 | **ADR-029** | 2026-05-08 | 接受 3 种系统性质模式，公开 API 必须 CamelCase，AST 内部/工具类允许 snake_case |

---

## 🟡 当前进行中

当前无进行中的议题。

---

## 📋 待讨论议题（按优先级排序）

### P0 级 — 核心正确性

| **3** | **隐式单时钟域** — 无 ClockDomain 对象，无 CDC 原语。`default_clock_`/`default_reset_` 是仅有的时钟/复位源。async FIFO 因 CDC 缺失被注释掉 | `context.h:148-157`, `clockimpl.h`, `fifo.h:183-298` | PRD §3.1, ADR-004, **ADR-008** | 1 session ✅ |
| **4** | **仿真评估顺序** — `tick()` 内部双 `eval()`、边沿触发假定、`node->get_parent()` 作为输入分类启发式 | `simulator.cpp:736-850` | PRD T1, ADR-005, **ADR-009** | 1 session ✅ |
| **5** | **`<<=` DAG 连接语义** — 8 个重载、`dynamic_cast` 分发、`set_driver()` vs `set_src()` 方向依赖、4 处 TODO 端口验证缺失 | `io.h:680-853` | ADR-002, **ADR-010** | 1 session ✅ |
| **6** | **zero_cache/ones_cache 线程安全** — `thread_local` 被注释掉，无互斥量，存在活跃的数据竞争 | `types.h:176`, `types.cpp:446` | **ADR-011** | 0.5 session ✅ |

### P1 级 — 架构完整性

| # | 议题 | 关键发现 | 关联 PRD/ADR | 预计时间 |
|---|------|---------|-------------|---------|
| **7** | **IO 端口双 API** — `__io` 宏（`reinterpret_cast`）vs `ch_in<T>`/`ch_out<T>` 类型别名 | `io.h:319-326` vs `io.h:301-302` | **ADR-012** | 1 session 🔍 待审查 |
| **8** | **类型层次设计** — `ch_bool` vs `ch_uint<1>` 独立类型、缺少 `ch_sint`/`ch_bits` | `bool.h`, `uint.h`, `traits.h` | ADR-001, **ADR-013** | 1 session ✅ |
| **9** | **Bundle 反射系统** — `CH_BUNDLE_FIELDS` 10 字段硬限制、缺失 `add_user()`（T002）、比特序未指定 | `bundle_meta.h:64-72`, `bundle_base.h:268-312` | ADR-002, **ADR-014** | 1 session ✅ |
| **10** | **拓扑排序作为单一 IR** — 仿真和代码生成共享 `eval_list`，排序稳定性、循环检测无恢复 | `context.cpp:170-399`, `simulator.cpp:538` | ADR-003, ADR-005, **ADR-015** | 1 session ✅ |
| **11** | **`ch_reg<T>` 继承设计** — 继承 vs 组合，`unique_ptr<next_type>` 动态分配 | `reg.h:33-62` | — | 0.5 session ✅ |
| **12** | **双 `in_static_destruction()`** — 两个无关的静态析构检测函数，destruction_manager 注册被注释掉 | `logger.h:83`, `destruction_manager.h:154` | **ADR-017** | 0.5 session ✅ |

### P2 级 — 功能完整性

| # | 议题 | 关键发现 | 关联 PRD/ADR | 预计时间 |
|---|------|---------|-------------|---------|
| **13** | **Verilog 代码生成完整性** — 33 个 ch_op 中仅 15 个实现，其余输出 `"<OP_NOT_IMPLEMENTED>"` | `codegen_verilog.cpp:146-187` | **ADR-019** | 1 session ✅ |
| **14** | **DAG 代码生成器定位** — 与 verilogwriter 几乎相同结构，用途不明确 | `codegen_dag.h`, `codegen_dag.cpp` | **ADR-020** | 0.5 session ✅ |
| **15** | **JIT 操作覆盖策略** — CALL_EXTERNAL 回退 + UNSUPPORTED_OP 完全编译失败 | `jit_compiler.cpp:291-415` | **ADR-021** | 1 session ✅ |
| **16** | **SpinalHDL 流缺失操作** — 实际已实现（见 ADR-022），存在聚合头集成缺口 G1-G3 | `stream_pipeline.h`, ADR-022 | **ADR-022** | 1 session ✅ |
| **17** | **仿真原语缺失** — fork/join、sleep、simTimeout、simSuccess/simFailure | `simulator.h`, `simulator.cpp` | Oracle 调研→仅 simTimeout | 0.5 session ✅ |
| **18** | **BlackBox/IP 集成** — 无法包装外部 Verilog/VHDL IP | 全代码库无匹配 | **ADR-024** → 搁置 | 1 session ✅ |
| **19** | **形式验证接口缺失** — 无 assume/cover，无 SVA 生成 | `assert.h`, `codegen_verilog.cpp` | **ADR-025** → 搁置 | 0.5 session ✅ |

### P3 级 — 工程纪律和技术债务

| # | 议题 | 关键发现 | 关联 PRD/ADR | 预计时间 |
|---|------|---------|-------------|---------|
| **20** | **`CH_MODULE` 宏去留** — AGENTS.md 标注为反模式，仍在 2 个文件中使用 | `module.h:99`, counter_simple.cpp, uart_tx.cpp | **ADR-026** → 已删除宏 + 替换 2 处 | 0.5 session ✅ |
| **21** | **LLVM-18 硬编码** — CMake 中版本固定，`find_package(LLVM QUIET)` 静默降级 | `CMakeLists.txt:58-74` | **ADR-027** → 已修复 | 0.5 session ✅ |
| **22** | **已禁用测试处理** — 5+ 测试被注释掉/排除，无修复时间线 | `tests/CMakeLists.txt` | **ADR-028** → 保留 TODO | 0.5 session ✅ |
| **23** | **命名约定不一致** — ~20 个类以小写开头，与 AGENTS.md 的 CamelCase 冲突 | 多个文件 | **ADR-029** → 接受现状并文档化例外 | 0.5 session ✅ |
| **24** | **Include 防护风格不统一** — `#pragma once` vs `#ifndef` 混用 | 遍及 `include/` | — | 0.5 session |
| **25** | **JIT ch_op ↔ JitOp 三文件同步** — 需编译时验证防止静默数据损坏 | `lnodeimpl.h`, `jit_ir.h`, `jit_compiler.cpp` | ADR-003 | 0.5 session |
| **26** | **`context_interface` 抽象类** — 4 方法纯虚接口无其他实现，属死代码或未来抽象 | `abstract/context_interface.h` | — | 0.5 session |
| **27** | **`using namespace ch::core` 污染** — 在 `ch` 命名空间内部使用，破坏隔离 | `if_stmt.h:13` 等 | — | 0.5 session |
| **28** | **组件构建非幂等性** — `built_` 标志存在但 rebuild 语义未指定 | `component.h:94`, `component.cpp:135-138` | ADR-004 | 0.5 session |

---

## 📊 统计

| 优先级 | 已完成 | 进行中 | 待讨论 | 合计 |
|--------|--------|--------|--------|------|
| P0 | 6 | — | — | 6 |
| P1 | 6 | — | — | 6 |
| P2 | 7 | — | — | 7 |
| P3 | 4 | — | 5 | 9 |
| **合计** | **23** | **—** | **6** | **29** |

---

## 讨论流程

每次讨论一个议题，按以下格式进行：

1. **Sisyphus 呈现议题** — 现状、隐含决策、关键文件、需要决策的问题
2. **用户讨论** — 选择方案、提出约束
3. **Sisyphus 记录决议** — 更新 ADR 文档

---

## 已创建/更新的 ADR 文档

| ADR | 状态 | 覆盖议题 |
|-----|------|---------|
| ADR-001 | ✅ 已采纳 | SpinalHDL 移植策略 |
| ADR-002 | ✅ 已完成 | Bundle 连接修复 |
| ADR-003 | 📝 草稿 | JIT 编译器架构 |
| ADR-004 | 📝 草稿 | 数据所有权和生命周期 |
| ADR-005 | 📝 草稿 | 双模式仿真设计 |
| ADR-006 | 📝 草稿 | 错误处理策略 |
| **ADR-007** | **✅ 已采纳** | **#1 非对称内存模型, #2 data_map_ 设计** |
| **ADR-008** | **✅ 已采纳** | **#3 单时钟域约束（CDC 不支持）** |
| **ADR-009** | **✅ 已采纳** | **#4 仿真评估顺序（边沿检测）** |
| **ADR-010** | **✅ 已采纳** | **#5 `<<=` 连接语义** |
| **ADR-011** | **✅ 已采纳** | **#6 zero()/ones() 线程安全** |
| **ADR-012** | **🔍 待审查** | **#7 IO 端口双 API（Oracle 分析已记录）** |
| **ADR-013** | **✅ 已采纳** | **#8 类型层次设计** |
| **ADR-014** | **✅ 已采纳** | **#9 Bundle 反射系统（Q1 修复任务待启动）** |
| **ADR-015** | **✅ 已采纳** | **#10 拓扑排序作为单一 IR** |
| **ADR-016** | **✅ 已采纳** | **#11 ch_reg<T> 继承设计（Q2 优化延后）** |
| **ADR-017** | **✅ 已采纳** | **#12 双 `in_static_destruction()`（多仿真场景，修正 ADR-007 D1）** |
| **ADR-018** | **✅ 已采纳** | **多仿真并发架构（跨议题汇总 + 线程安全审计 + 编排层建议）** |
| **ADR-019** | **✅ 已采纳** | **#13 Verilog 代码生成完整性（18 个缺失操作批量 + 分阶段实施）** |
| **ADR-020** | **✅ 已采纳** | **#14 DAG 代码生成器定位（可视化工具 + 提取共享基类）** |
| **ADR-021** | **✅ 已采纳** | **#15 JIT 操作覆盖策略（FAIL FAST + 分阶段补缺失操作）** |
| **ADR-022** | **✅ 已采纳** | **#16 SpinalHDL 流管道操作分析（实际已实现，记录 G1-G5 残余缺口）** |
| **ADR-023** | **✅ 已采纳** | **#17 仿真原语评估（仅加 simTimeout，其余跳过/推迟）** |
| **ADR-024** | **✅ 已采纳（搁置）** | **#18 BlackBox/IP 集成评估（零需求，低优先级，等触发条件）** |
| **ADR-025** | **✅ 已采纳（搁置）** | **#19 形式验证接口评估（零 formal 工具链集成，搁置）** |
| **ADR-026** | **✅ 已采纳（已执行）** | **#20 移除 CH_MODULE 宏（删除定义 + 替换 2 处使用）** |
| **ADR-027** | **✅ 已采纳（已执行）** | **#21 LLVM 版本硬编码修复（LLVM-18 → 动态检测）** |
| **ADR-028** | **✅ 已采纳** | **#22 已禁用测试处理（保留 TODO 注释清单）** |
| **ADR-029** | **✅ 已采纳** | **#23 命名约定一致化（接受现状，AST 内部/工具类例外规则）** |
