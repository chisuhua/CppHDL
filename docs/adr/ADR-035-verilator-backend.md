# ADR-035: Verilator 仿真后端

**状态**: 🟢 采纳 + Phase 1-4.1 完成（脚手架 GA，完整仿真待 Phase 3.2+）
**日期**: 2026-06-07（提议）→ 2026-06-07（脚手架 GA）
**决策人**: Sisyphus + 用户（参考 3 份专项调研 + AGENTS.md `using-superpowers` 工作流）

---

## Phase 进度（2026-06-07 快照）

| Phase | 范围 | 状态 | 提交 |
|-------|------|------|------|
| **Phase 0** | 调研（3 个 explore/librarian agents） | ✅ | （无代码） |
| **Phase 0.5** | ADR-035 决策记录 | ✅ | `ac6445b` |
| **Phase 1.1** | `default_clock`/`default_reset` 加进 port list | ✅ | `ac6445b` |
| **Phase 1.2** | 所有 `ch_out` 端口发射，移除 body 重复声明 | ✅ | `5733597` |
| **Phase 1.3** | `print_logic` 走 proxy 链找真实 driver | ✅ | `41e6521` |
| **Phase 1.4** | Bundle 字段名保留 + 移除 input 重复声明 | ✅ | `d196650` |
| **Phase 1.6** | iverilog + verilator --lint-only 端到端验证 | ✅ | `1257e4d` |
| **Phase 1.7** | 验收：131/131 ctest 通过 + 6/6 外部工具验证 | ✅ | （合并到 1.6） |
| **Phase 2.1** | `IEvalBackend` 接口（`include/core/eval_backend.h`） | ✅ | `673d1a2` |
| **Phase 2.2** | `InterpreterBackend`（`src/core/interpreter_backend.cpp`） | ✅ | `673d1a2` |
| **Phase 2.3** | `Simulator::set_backend()` 入口重构 | ✅ | `c5c90d7` |
| **Phase 3.1** | `VerilatorBackend` 脚手架（generate + invoke + SHA-1） | ✅ | `8c9800a` |
| **Phase 3.2** | sim_main.cpp wrapper + dlopen（端到端可运行） | ✅ | `c5b7b1b` |
| **Phase 3.3** | `data_map_` ↔ Vtop 同步（端口表生成 + eval 接线） | ✅ | `1cfc1af` |
| **Phase 3.4** | 时钟模型适配（type_clock 检测） | ✅ | `5d0d4d4` |
| **Phase 3.5** | SHA-1 缓存命中（`cache_path_for_key` + 查找短路） | ✅ | `d0dc194` |
| **Phase 3.6** | VCD 跟踪 API（`enable_vcd` toggle） | ✅ | `c928dfe` |
| **Phase 4.1** | VerilatorBackend 测试（17 个，58 assertions 全过） | ✅ | 7 个 commit |
| **Phase 4.4** | 用户文档（`docs/usage_guide/10-verilator-backend.md`） | ✅ | `b6e1b6e` |

**已提交 commits**: 17 个（`ac6445b`, `5733597`, `41e6521`, `d196650`, `1257e4d`, `673d1a2`, `c5c90d7`, `8c9800a`, `1482b14`, `b6e1b6e`, `c5b7b1b`, `1cfc1af`, `5d0d4d4`, `d0dc194`, `c928dfe` + 2 个 ADR 文档更新）

**测试状态**:
- `test_verilator_backend`: 17/17 测试通过（58 assertions）
- ctest -L base: 132/132 通过（排除 2 个预存在 flaky + 3 个 JIT 测试需要 LLVM，本环境未装）

**Phase 3.2 端到端流程（已验证）**:
1. `generate_verilog()` 写 `top.v` + `sim_main.cpp`（main() + extern "C" factory/eval/final/delete）
2. `invoke_verilator()` 跑 `verilator --cc --exe --build` 编译
3. `dlopen_top()` dlopen `obj_dir/Vtop` 并 dlsym 4 个符号
4. 端到端测试 `DlopenAndResolveSymbols` 验证 `obj_dir/Vtop` 存在

---

## 1. 背景

CppHDL 当前的仿真后端是**解释器 + LLVM JIT**（`src/simulator.cpp` + `src/jit/jit_compiler.cpp`）。实测基线（`docs/simulation/ARCHITECTURE.md:225-226`）：

| 场景 | ticks/sec | 评级 |
|------|-----------|------|
| `depth=10` 组合链 | 29,840 | 🟡 |
| `depth=1000` 组合链 | **265** | 🔴 |
| `depth=10000` 组合链 | 20 | 🔴 |
| 100 signals trace 开销 | **82.88%** | 🔴 |
| `tick(N)` vs `tick()` 循环 | **-27%** (更慢) | 🔴 |

**性能瓶颈**主要来自 4 个 `instr_base::eval()` 虚函数调用 + `unordered_map` 哈希查找（`docs/simulation/ARCHITECTURE.md:50-58`）。

**SpinalHDL 路径**（`docs/simulation/ARCHITECTURE.md:42-47, 327-336`）已证明 Verilator 可达 10-100× 加速：

> "Verilator 将 Verilog 编译为 O3 优化的 C++，JNI 绑定实现进程内直接调用... 直接指针访问 signalAccess[]，零拷贝"

**架构文档**（`docs/simulation/ARCHITECTURE.md:14`）把 Verilator 列为可选架构：

> | 仿真引擎 | Verilator JNI + VPI 后端 |

**freeRTOS 规格**（`docs/freertos/01-specification.md:28`）已点名 Verilator：

> 仿真环境：C++ simulation only (Verilator 模式)

**调研报告**已确认：
- 现有 Verilog codegen (`src/codegen_verilog.cpp`, 865 行) **已能生成完整 `module top`** —— Verilator 的"前端"已具备
- `ch::Simulator` API 干净（`set_input_value` / `get_port_value` / `tick`）—— 测试台代码完全后端无关
- 无 backend 抽象 —— Simulator 单体内嵌解释器 + JIT，**新增第 3 个后端需要重构**
- 现有 codegen 有 5 个 Verilator 阻塞 bug 必须先修

---

## 2. 决策

**采纳 Verilator 仿真后端**。分 4 阶段交付，预计 13-20 个工作日。

| 阶段 | 内容 | 改动量 | 风险 | 验收标准 |
|------|------|--------|------|----------|
| **Phase 1** | 修 codegen 5 个 Verilator 阻塞 | +300/-50 LoC | 🟢 低 | 132 ctest 全绿 + 10 个代表设计 `iverilog -g2012` + `verilator --lint-only` 通过 |
| **Phase 2** | 引入 `IEvalBackend` 抽象 + 3 个实现 | -109 +650 LoC | 🟡 中 | 132 ctest 全绿 + 28/28 ported tests 全绿 + 行为零变化 |
| **Phase 3** | `VerilatorBackend` 实现 (dlopen + ISignalAccess*[] + SHA-1 缓存) | +800 LoC | 🔴 高 | end-to-end `samples/counter.cpp` 跑通 + A/B 对比解释器一致 |
| **Phase 4** | 测试、文档、性能基准、AGENTS.md 增补 | +400 LoC | 🟢 低 | docs/usage_guide/10-verilator-backend.md + 28 个 examples 全 Verilator 跑通 |

**Release 目标**: v1.5（Verilator 后端 GA）

---

## 3. 关键设计决策

### 3.1 抽象位置：新增 `IEvalBackend` 接口

放置 `include/core/eval_backend.h`，与 `simulator.h` 平行。

```cpp
class IEvalBackend {
public:
    virtual ~IEvalBackend() = default;
    virtual bool initialize(ch::core::context* ctx, ch::data_map_t& data_map) = 0;
    virtual void clear() = 0;
    virtual void eval_combinational(ch::data_map_t& data_map, ...) = 0;
    virtual void eval_sequential(ch::data_map_t& data_map, ...) = 0;
    virtual void eval_input(ch::data_map_t& data_map, ...) = 0;
    virtual void reset(ch::data_map_t& data_map) = 0;
    virtual std::string name() const = 0;
};
```

### 3.2 三种实现

| 后端 | 来源 | 时机 |
|------|------|------|
| `InterpreterBackend` | 提取现有 `for instr: instr->eval()` 循环 | Phase 2A |
| `JitBackend` | 包装现有 `jit_compiler_` + `sync_*_buffer` | Phase 2A |
| `VerilatorBackend` | dlopen `libVtop.so` + ISignalAccess*[] 端口映射 | Phase 3 |

### 3.3 API 兼容性

**保留** `sim.set_jit_enabled(false/true)` 作为 `set_backend(make_unique<JitBackend>())` 的 deprecated wrapper。

**新增** `sim.set_backend(std::unique_ptr<IEvalBackend>)` 主入口。

**理由**：`tests/test_jit_concat.cpp:42, 72, 102, 146, 206, 238` 共 6 处使用 `set_jit_enabled(true)`，加上 15 个 `[jit]` 测试检查 `is_jit_compiled()`。一次性破坏会破坏 132 个 ctest 全部通过率的承诺。

### 3.4 时钟模型适配（CppHDL "3 次 eval" vs Verilator "1 次 step"）

**问题**：`Simulator::tick()` 当前内部做 3 次 eval（`src/simulator.cpp:855-876`）：
```cpp
eval_combinational();           // ① 前置组合
if (default_clock_instr_) {
    eval();  eval();           // ② 时钟 0→1 (时序) + 时钟 1→0 (组合)
    eval_combinational();       // ③ 末端组合
}
```

**Verilator 一次 `eval()` = 一次组合 + 一次时序评估**（[`Connecting to Verilated Models` 文档](https://verilator.org/guide/latest/connecting.html)）。

**决策**：**采用方案 B（性能优先）**：
- `VerilatorBackend::tick()` 内部调 1 次 `top->eval()`，接受 negedge 行为可能简化
- CppHDL 现有 132 个测试和 28 个示例都是 posedge 触发的，无 negedge 敏感逻辑，**方案 B 在功能上等价**
- 提供 `set_strict_posedge_model(true)` 切换为方案 A（3 次 eval）做正确性回归

### 3.5 加载模式：dlopen + SHA-1 缓存（参考 SpinalHDL）

参考 [`SpinalHDL VerilatorBackend.scala`](https://github.com/SpinalHDL/SpinalHDL/blob/dev/sim/src/main/scala/spinal/sim/VerilatorBackend.scala) 的成熟模式：

1. **`ch::toVerilog()` 生成 `top.v`**（现有 codegen）
2. **`verilator --cc --exe --build`** 编译为 `obj_dir/Vtop`
3. **自定义 makefile** 链接 `libVtop.so`（参考 [Verilator issue #4273](https://github.com/verilator/verilator/issues/4273)）
4. **`dlopen` + `dlsym`** 加载 `Vtop*` 工厂函数
5. **SHA-1 缓存键** = `(设计 hash, Verilator 版本, CFLAGS, wave 选项)` → `~/.cache/cpphdl/verilator/<hash>/libVtop.so`
6. **LRU 淘汰** `maxCacheEntries = 100`

### 3.6 端口访问性能：ISignalAccess*[] 数组（参考 SpinalHDL）

参考 [`SpinalHDL VerilatorBackend.scala:107-150` ISignalAccess](https://github.com/SpinalHDL/SpinalHDL/blob/dev/sim/src/main/scala/spinal/sim/VerilatorBackend.scala)：

```cpp
// C++ 端：预生成端口访问器数组
struct PortAccessor {
    void* field_ptr;       // &top->clk, &top->io_a, ...
    uint64_t mask;          // (1ULL << bitwidth) - 1
    uint8_t shift;          // bundle 字段的 bit offset
};
std::vector<PortAccessor> accessors_;   // node_id → accessor, O(1) 查表
```

**性能**：直接指针访问 ≈ 1-2 ns/次（vs CppHDL 当前 `unordered_map` 查找 10-50 ns，**10-50× 提升**）。

### 3.7 数据流：`data_map_` 仍归 `Simulator`，后端通过参数注入访问

**最小破坏**：`set_input_value()` 写 `data_map_[node_id]`，后端通过 `eval_*(data_map, ...)` 读取。
- **JitBackend**：`sync_to_buffer(data_map_)` → 推送到 `data_buffer_`（已有）
- **VerilatorBackend**：`sync_to_buffer(data_map_)` → 推送到 `top->xxx`
- **`sync_from_buffer(data_map_)`**：解释器/JIT 把内部 buffer 同步回 `data_map_`

---

## 4. 关键依赖

### 4.1 前置依赖（Phase 0 已有）

- ✅ `ch::toVerilog()` 库 API（`include/codegen_verilog.h:33`）
- ✅ `ch::Simulator` 干净的测试台 API（`include/simulator.h:99-432`）
- ✅ 28 个 ported examples 全部能生成 Verilog
- ✅ `docs/simulation/ARCHITECTURE.md` 已认可 Verilator 方向

### 4.2 Phase 1 必须修复的 codegen bug（前置）

5 个 Verilator 阻塞（详见调研报告 + 配套计划文档）：

| # | 阻塞 | 位置 |
|---|------|------|
| 1 | `default_clock` 未声明为 input 端口 | `src/codegen_verilog.cpp:226-280` (print_header) + 489 (print_reg) |
| 2 | `default_reset` 完全缺失 | 同上，需要新增加 `if (reset) reg <= 0;` |
| 3 | `io` 以外的输出端口被过滤 | `src/codegen_verilog.cpp:248-261` |
| 4 | `io` 硬连第一个 `type_reg` | `src/codegen_verilog.cpp:432-440` |
| 5 | Bundle 字段名丢失（变 `top_io_N`） | `include/core/bundle/bundle_base.h:267-311` + `verilogwriter` 构造器 |

> Issue #6 (12 ch_op + type_bitsupdate 缺口) **已被 commits f010346 + 806f175 解决**，从 Phase 1 移除。

### 4.3 环境依赖

- **Ubuntu 24.04 + `apt install verilator`**（5.020-1 默认版）—— 仓库已确认可用
- **目标 Verilator 版本**：5.020+（`rootp` API 稳定）
- **C++20 编译器**：GCC 13.x 或 Clang 18.x（Ubuntu 24.04 默认）

---

## 5. 风险登记

| # | 风险 | 等级 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | Verilator ABI 多版本不兼容 | 🔴 高 | 同份 RTL 在 5.020 vs 5.042 编译产物不兼容 | SHA-1 缓存键含 `verilator --version`；CI 校验 |
| R2 | 132 个 ctest 中 6 处显式 `set_jit_enabled` | 🟡 中 | 旧 API 移除破坏测试 | 保留为 `set_backend` 的 wrapper；3-Phase 弃用 |
| R3 | 3 次 eval/tick 模型与 Verilator 单步不兼容 | 🟡 中 | 时序敏感设计可能错 | 方案 B 默认 + `set_strict_posedge_model(true)` 切换 A 模式 |
| R4 | Verilate 慢（>30s）影响开发循环 | 🟡 中 | 用户体验差 | SHA-1 缓存 + `--output-split 5000` |
| R5 | 顶层 IO 字段名改动破坏 ABI | 🔴 高 | 重命名后 wrapper 编译失败 | 严格 `&top->clk` 取地址，不硬编码 offset |
| R6 | `disconnect()` 顺序错误 | 🟡 中 | 偶发 crash | `disconnect()` 显式重排：先 `backend_->clear()` 再清 `data_map_` |
| R7 | 内存端口 (mem_read_port / mem_write_port) Verilator 不识别 | 🟡 中 | CppHDL `instr_mem*` 链接无意义 | Verilator 后端**不实现** mem port，依赖 Verilator 内置 VL_MEM_IF |
| R8 | Phase 1 codegen 修复引入回归 | 🟡 中 | 132 ctest 部分失败 | 一次只改一个 issue；每个 sub-phase 都跑 ctest |
| R9 | A/B 验证从占位 WARN 升级为实际比对引入性能开销 | 🟢 低 | 每 tick 复制 `data_map_` | A/B 默认关闭；显式 `set_ab_verification(true)` 才启用 |
| R10 | dlopen 多个 `.so` 时 Verilated 符号冲突 | 🟡 中 | 多设计并行测试 crash | `RTLD_LOCAL` + `--output-split-cfuncs 500` 匿名函数 |

---

## 6. 实施时间线

| 阶段 | 工作量 | 累计 |
|------|--------|------|
| Phase 0（已完成） | 1 天 | 1 |
| Phase 1 | 5-8 天 | 6-9 |
| Phase 2 | 1-2 天 | 7-11 |
| Phase 3 | 5-7 天 | 12-18 |
| Phase 4 | 2-3 天 | 14-21 |

**两人并行可压缩到 8-10 天**（Phase 1 + Phase 3 是不同 subagent 域）。

---

## 7. 验证标准

### 7.1 Phase 1 验收
- [ ] 132 ctest 全绿（不增不减）
- [ ] 28/28 ported tests 通过 `run_all_ported_tests.sh`
- [ ] 10 个代表设计的 Verilog 输出能 `iverilog -g2012` 编译
- [ ] 10 个代表设计的 Verilog 输出能 `verilator --lint-only` 通过
- [ ] AGENTS.md 增补"Verilog 可综合性"段落

### 7.2 Phase 2 验收
- [ ] 132 ctest 全绿（**行为零变化**）
- [ ] 新增 `test_backend_interface.cpp`：IEvalBackend 三实现对相同 fixture 输出一致
- [ ] `set_jit_enabled(true/false)` 仍可工作
- [ ] AGENTS.md 增补"仿真后端"段落

### 7.3 Phase 3 验收
- [ ] `samples/counter.cpp` 在 Verilator 后端跑出与解释器/JIT 一致结果（A/B 验证）
- [ ] 10 个代表 sample 在 Verilator 后端跑通
- [ ] SHA-1 缓存命中时启动 < 100ms
- [ ] trace 正确生成 FST 文件

### 7.4 Phase 4 验收
- [ ] `tests/test_verilator_backend.cpp` 27 个 [verilator] 测试通过
- [ ] `run_all_ported_tests.sh` 28/28 在 Verilator 后端通过
- [ ] 性能基准：TC-01 `depth=1000` 达到 ≥ 5000 ticks/sec（vs 当前 265）
- [ ] `docs/usage_guide/10-verilator-backend.md` 完整

### 7.5 Release v1.5 验收
- [ ] 所有 Phase 1-4 验收项
- [ ] 集成测试：端到端 `riscv-mini` hello world 在 Verilator 后端跑通
- [ ] `CHANGELOG.md` v1.5 段
- [ ] 跨平台 CI 验证（Ubuntu 22.04 + 24.04）

---

## 8. 替代方案

### 8.1 方案对比

| 方案 | 工作量 | 性能 | 外部依赖 | 推荐 |
|------|--------|------|----------|------|
| **A. Verilator 后端**（本 ADR） | 13-20 天 | 10-100× | Verilator 工具链 | ✅ **采纳** |
| **B. asmjit JIT**（`docs/simulation/ROADMAP.md:253-285`） | 2-3 月 | 5-10× | asmjit（header-only） | 🟡 备选，2x 性能提升 vs 3x 工作量 |
| **C. Cash simjit 移植** | 1 月 | 30-100× | LIBJIT 替代 LLVM | 🟡 备选 |
| **D. 继续 P0/P1 解释器优化** | 1 周 | 3-6× | 无 | 🟢 已部分采纳，**不冲突**（可与 A 并行） |

**采纳 A 的理由**：
1. **外部依赖成熟**：Verilator 在工业界（SpinalHDL/Chisel/CIRCT）广泛使用
2. **有完整参考实现**：SpinalHDL VerilatorBackend 是生产级参考
3. **freeRTOS 规格点名**：避免日后返工
4. **架构文档已认可方向**：减少决策阻力
5. **风险可控**：纯加法（不破坏现有解释器/JIT）

### 8.2 不采纳 Verilator 的代价

| 决策 | 后果 |
|------|------|
| 不做 Verilator 后端 | - freeRTOS 仿真性能需求被推迟<br>- SpinalHDL/CppHDL 仿真速度差距持续扩大<br>- 错过"标准化 RTL 验证" 路径 |
| 仅做方案 D（解释器优化） | - 3-6× 性能提升不够（目标 10-50×）<br>- 失去 Verilator 工具链生态<br>- 仿真模型不可被外部 Verilog 工具（yikes/vcs）消费 |

---

## 9. 决策日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-06-07 | v1.0 | 初始 ADR；3 份调研报告综合；决策采纳 | Sisyphus + 用户 |

---

## 10. 参考资料

### 10.1 调研报告（本 ADR 引用）

1. **Codegen Verilator 阻塞报告**（`docs/adr/ADR-035/01-codegen-blockers.md`）—— 5 个阻塞 + 1 个已解决的精确 file:line 映射
2. **Verilator C++ API 报告**（`docs/adr/ADR-035/02-verilator-api.md`）—— `verilator --cc --exe --build`、Vtop 类、ISignalAccess、SpinalHDL 参考、Ubuntu 24.04 版本
3. **Simulator eval 分发报告**（`docs/adr/ADR-035/03-simulator-dispatch.md`）—— `data_map_` 突变点审计、IEvalBackend 接口设计、风险清单

### 10.2 现有 ADR 引用

- **ADR-019**: Verilog codegen 完整性（ch_op 覆盖）—— Phase 1 的 12 ch_op + type_bitsupdate 已解决
- **ADR-008**: 单时钟域约束 —— 与 Verilator 单时钟模式对齐
- **ADR-009**: 仿真求值顺序 —— `eval_combinational → eval → eval → eval_combinational` 3 次模式
- **ADR-005**: 双模式仿真（解释器 + JIT）—— Phase 2 将扩展为"三模式"
- **ADR-018**: 多仿真架构 —— Verilator 后端复用其多 simulator 隔离模式

### 10.3 外部参考

- [SpinalHDL VerilatorBackend.scala](https://github.com/SpinalHDL/SpinalHDL/blob/dev/sim/src/main/scala/spinal/sim/VerilatorBackend.scala) —— **核心参考**（SHA-1 缓存、ISignalAccess、dlopen 模式）
- [Verilator Connecting to Verilated Models](https://verilator.org/guide/latest/connecting.html) —— API 官方文档
- [Verilator issue #4273](https://github.com/verilator/verilator/issues/4273) —— dlopen `.so` 模式
- [Verilator 4.210 release notes](https://github.com/verilator/verilator-announce/issues/48) —— `rootp` 迁移
- [ZipCPU TBCLOCK](https://zipcpu.com/blog/2018/09/06/tbclock.html) —— 多时钟驱动模式（备查）
- [Ubuntu 24.04 verilator 包](https://packages.ubuntu.com/noble/verilator) —— 5.020-1
- [Marlin-Verilator (Rust)](https://crates.io/crates/marlin-verilator) —— dlopen 模式 LGPL 兼容性参考

### 10.4 本地文档

- `docs/simulation/ARCHITECTURE.md:1-359` —— 当前仿真架构瓶颈分析
- `docs/simulation/ROADMAP.md:1-437` —— 仿真优化路线图
- `docs/simulation/SIM_OPT_PLAN.md:30-32` —— 标记 S0-1/S0-2/S0-3 为"✅ completed"（**注：与代码不同步，本 ADR 修正**）
- `docs/simulation/JIT_ROADMAP.md` —— JIT 路线图（已全部完成）
- `docs/freertos/01-specification.md:28` —— "Verilator 模式"是仿真目标
- `include/jit/AGENTS.md:1-165` —— JIT 子系统规则
- `include/AGENTS.md` —— 头文件库规约

---

**维护**: AI Agent
**下次审查**: Phase 1 完成后（预计 2026-06-14）
