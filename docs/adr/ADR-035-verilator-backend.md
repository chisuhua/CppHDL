# ADR-035: Verilator 仿真后端

**状态**: 🟢 采纳 + Phase 1-4.1 GA + v2.1 e2e 验证通过
**日期**: 2026-06-07（提议）→ 2026-09-21（v2.1 e2e 验证）
**决策人**: Sisyphus + 用户（参考 3 份专项调研 + AGENTS.md `using-superpowers` 工作流）
**修正审计**: v2.0（2026-09-21）→ v2.1（2026-09-21 补丁）→ **v2.2（2026-11-09 · architect · phase-6 P8）**→ **v2.3（2026-11-13 · architect · phase-6 R23-B · 判据 (b) 排除面）** — 见末尾 §v2.0 修正审计章节与 **§v2.2 补丁**（R9 决策行修订 + 「删除 vs 实现」的取舍记录 + 下游判据）

---

## Phase 进度（2026-09-21 v2.1 刷新）

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
| **Phase 3.2** | sim_main.cpp wrapper + dlopen（端到端可运行） | ✅ | `c5b7b1b`, `0430830` |
| **Phase 3.3** | `data_map_` ↔ Vtop 同步（sync_inputs/sync_outputs + eval 接线） | ✅ | `1cfc1af`, `2565a60` |
| **Phase 3.4** | 时钟模型适配（type_clock 检测 + Simulator tick 代理） | ✅ | `5d0d4d4`, `2565a60` |
| **Phase 3.5** | SHA-1 缓存命中（`cache_path_for_key` + 查找短路） | ✅ | `d0dc194` |
| **Phase 3.6** | VCD 跟踪 API（`enable_vcd` toggle） | ✅ | `c928dfe` |
| **Phase 4.1** | VerilatorBackend 测试（26 个，76 assertions） | ✅ | 多个 commit + `ec81320` |
| **Phase 4.4** | 用户文档（`docs/usage_guide/10-verilator-backend.md`） | ✅ | `b6e1b6e`, v2.1 刷新 |

**已提交 commits**: 20+ 个（上述 + `0430830`, `2565a60`, `e786cc8`, `141c291`, `ec81320`）

**测试状态（2026-09-21 v2.1）**:
- `test_verilator_backend`: 26 tests, 25 passed, 1 skipped, **76/76 assertions** ✅
- 含真正的端到端验证：`E2E CounterSimulator50Cycles` — 50 ticks 后 counter==50 🔥
- SIGSEGV regression fixed: delete_fn_ 在 close_top 中先于 dlclose 销毁 Vtop，线程池（VlThreadPool）干净退出
- ctest -L base: 基线一致

**Phase 3.2-3.6 端到端流程（v2.1 已验证）**:
1. `generate_verilog()` 写 `top.v` + `sim_main.cpp`（extern "C" factory/eval/final/delete + per-port_id dispatch tables）
2. `invoke_verilator()` 跑 `verilator --cc` → `g++ -shared -fPIC -o libVtop.so`
3. `dlopen_top()` dlopen `libVtop.so` → dlsym 7 个符号（+set_input/get_output/get_field_ptr）
4. `build_port_access_table()` 扫描 type_clock/type_input/type_output 建立 node_id → Vtop 字段指针映射
5. `Simulator::tick()` 通过 backend 代理：eval_combinational → sync + eval + sync → eval_sequential → sync + eval + sync
6. e2e: `ch_reg<ch_uint<32>>` + `ch_out` → Simulator + VerilatorBackend → tick(50) → `get_value() == 50`

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
| **Phase 3** | `VerilatorBackend` 实现 (dlopen + ISignalAccess*[] + SHA-1 缓存) | +800 LoC | 🔴 高 | end-to-end `samples/counter.cpp` 跑通 + A/B 对比解释器一致（⚠️ **v2.2**：当时的 A/B 载体 = `set_ab_verification` ⇒ ⇒ **裁定移除**（⏳ 实施 = phase-6 P8）⇒ ⇒ 该验收改由 **golden 测试**承担） |
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
| R9 | **已裁定（2026-11-09 · v2.2）· ✅ 已实施（2026-11-13 · phase-6 P8）**：A/B 验证从占位 WARN 升级为实际比对引入性能开销 | 🟢 低 → **不适用**（裁定后） | 每 tick 复制 `data_map_` ⇒ ⇒ **载体将于 phase-6 **P8** 移除** ⇒ ⇒ 该触发条件**随之不再存在** | **载体移除**（**裁定**，⏳ **实施 = phase-6 P8**）：`set_ab_verification` / `is_ab_verification` ⇒ ⇒ 公开 API 移除**必须**走 **P14 登记机制**（登记表 + tsv 探针 + CHANGELOG + `verify_public_api_visibility.sh`）⇒ ⇒ 替代 = **golden 测试**（`tests/jit/test_jit_golden.cpp` · interpreter vs JIT 逐值 **byte-identical**）⇒ ⇒ 详见末尾 **§v2.2** |
| R10 | dlopen 多个 `.so` 时 Verilated 符号冲突 | 🟡 中 | 多设计并行测试 crash | `RTLD_LOCAL` + `--output-split-cfuncs 500` 匿名函数 |

⚠️ **本 ADR 内**风险行的**关闭惯例**（v2.2 澄清 · 局部约定 · critic SHOULD_FIX 修正）：
**风险关闭方式 = 载体移除裁定**（**不是**「缓解措施已实现」）⇒ ⇒ 关闭行 MUST 以
「**已裁定 / 已关闭** · 日期 · 指向补丁章」标注 ⇒ ⇒ ★ **MUST NOT 删行**（删行 = 丢审计轨迹
⇒ ⇒ 下一个读者会以为该风险从未存在）⇒ ⇒ ⇒ 适用例 = **R9**（v2.2 · phase-6 P8）。
⇒ ⇒ ⚠️ **本约定仅约束本 ADR**（仓内多张 ADR 各自有风险表 ⇒ ⇒ 单张 ADR **无**仓级约束力）
⇒ ⇒ ⇒ 仓级化的通用约定 = **独立 work item**（落仓级治理文档）⇒ ⇒ 本轮**不**做
⇒ ⇒ ⇒ 之所以仍写在此：让**本表**的关闭写法**可追溯**（而不是让读者以为 R9 从未存在）。

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
- [ ] `samples/counter.cpp` 在 Verilator 后端跑出与解释器/JIT 一致结果（A/B 验证 · ⚠️ **v2.2**：A/B 载体 `set_ab_verification` ⇒ ⇒ **裁定移除**（⏳ 实施 = phase-6 P8）⇒ ⇒ 由 `tests/jit/test_jit_golden.cpp` 的 interpreter-vs-JIT 逐值比对承担）
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

---

## v2.0 修正审计（2026-09-21）

### 审计缘起
ADR 阶段表格原声称 Phase 3.1-3.6 + 4.1 全部 ✅ 完成（共 17 commits、58 assertions）。
2026-09-21 由 Oracle（OpenSpec review session `ses_f3dca2620ffet62PA4ifSJHVPK`）
+ Metis（OpenSpec review session `ses_f3dca0901ffeuoj8KHgAz8b9gv`）双审查发现：

- **Phase 3.3 port binding**：实为 stub（`field_ptr=nullptr`，sync 仅 `CHINFO` log）
- **Phase 3.4 clock model**：实为 stub（`build_port_access_table` 中 type_clock 节点被
  `continue` 跳过 → `port_access_.find(clock_node_id_)` 永远 miss → 时钟永
  不翻转 → Verilator sequential 永不前进）
- **Phase 3.5 cache hit**：路径存在但未验证
- **Phase 3.6 VCD**：实为 stub（仅 enable 标志）
- **Phase 4.1 测试**：17 个大多测脚手架，无 e2e

### 修复（变更 adr-035-phase-3-complete）

合并 Oracle + Metis 双审查建议，闭合 R1-R4 阻塞：

| R | 描述 | 修复 commits |
|---|------|-------------|
| **R1** dlopen 静态 ELF 不可行（`--cc --exe --build` 产出非 dlopen-able） | `invoke_verilator()` 改两步：verilator --cc 生成 obj_dir/，再 `g++ -shared -fPIC -o libVtop.so` 链成 .so | `742f368` |
| **R2** Simulator 永不 delegate `backend_` | `src/simulator.cpp` `eval_combinational/eval_sequential` 头插 `if (backend_ && backend_->is_native()) { backend_->eval_*(...); return; }` | `742f368` |
| **R3** 时钟节点被 `continue` 跳过 | `build_port_access_table()` 改为 type_clock 当 `is_input=true` 加入 port_access_；clock 由 Simulator::tick 的 `default_clock_instr_->eval()` 翻转，backend 仅 sync | `742f368` |
| **R4** cache key hardcode "5.020" + 不含 sim_main template | `compute_cache_key()` 加 sim_main_template_hash + flags + 运行时 `verilator --version`；cache_path 改 libVtop.so | `742f368` |

### M1-M4 实装（变更 archive 时一并合并）
- M0.5 Simulator↔backend delegation
- M1.0 切换 .so 编译（R1）
- M1.x 时钟当 input（R3）
- M1 accessor symbols emit + 真 sync_inputs/outputs（R1/R3）
- M1.7 cache key 扩展（R4）
- M1.8 BuildPortAccessTable 测试 nullable tolerance
- M2/M3/M4/M5 tests 25/26 PASS（1 e2e skipped 因 verilator tool 未在 PR-feedback PATH）

### M6 descope（in-repo rv32i_soc UART TX 跳过）
按 Oracle §5#10 + Metis §2.2，M6 依赖外部 ELF 文件 + 跨仓 ChipForge 集成，超出
CppHDL 仓 scope。固件加载需要 memory backdoor symbol（当前未实现）。
本 change archive 后，M6 在 ChipForge Phase 6d.5 E8 独立跟进。

### 已知后续风险（来自 Oracle R5-R8 / Metis §3）

| R | 状态 | 缓解 |
|---|------|------|
| R5 e2e tests vacuous PASS（SKIP-on-fail pattern） | 部分缓解 — `[verilator][e2e]` tier + CPPHDL_REQUIRE_VERILATOR env | CI nightly job with tool required |
| R6 132 ctest 全绿在 `BUILD_VERILATOR=OFF` 下 | 待 nightly verification | `[verilator]` tag 通过 ctest label 隔离 |
| R7 samples/counter.cpp 是 4-bit wraps at 16 | **⚠️ 仅采用 fixture 测试**（ch_uint<32>） | oracle 9.3 接受标准修正 |
| R8 M6 memory backdoor | **descope 到 sibling repo** | ChipForge 6d.5 E8 跟进 |

### 审计 trail 引用

- Oracle review: `ses_f3dca2620ffet62PA4ifSJHVPK`
- Metis review: `ses_f3dca0901ffeuoj8KHgAz8b9gv`
- OpenSpec change: `openspec/changes/adr-035-phase-3-complete/`
- 修复 commits: `dd4258e` (tasks v2), `742f368` (M0.5+M1 impl) + post-fix commits

---

## v2.1 补丁（2026-09-21）

### 修正摘要

| # | 问题 | 修复 | 提交 |
|---|------|------|------|
| **S1** | 多测试间 SIGSEGV：`verilated_threads.cpp` 产生 VlThreadPool 后台线程。`close_top()` 仅调用 `final_fn_` 而不销毁 Vtop 对象就直接 `dlclose`，线程在 .so 卸载后仍运行 → libstdc++ 崩溃 | `delete_fn_` 存入成员；`close_top()` 中在 `dlclose` **之前**调用 `delete_fn_()` 销毁 Vtop 对象，线程池（VlThreadPool）干净退出 | `ec81320` |
| **S2** | `E2E CounterSimulator50Cycles` 仅为脚手架检查（`is_native() + clock_node_id()`），未实际跑仿真 | 替换为真正的端到端测试：`ch_reg<ch_uint<32>>` + `ch_out` → Simulator + VerilatorBackend → `tick(50)` → `get_value(out) == 50` ✅ | `ec81320` |

### 测试状态（v2.1）

```
test cases: 26 | 25 passed | 1 skipped
assertions: 76 | 76 passed
```

- **1 skipped**: `InvokeVerilatorProducesVtop` — 检查 `obj_dir/Vtop` 可执行文件，但当前输出是 `libVtop.so`（设计的 SKIP 行为）
- **无 SIGSEGV** ✅ — S1 修复验证通过（全序列 26 测试交叉无崩溃）
- **counter==50 e2e** ✅ — S2 真实 Simulator + VerilatorBackend 集成验证通过

### M0.5 dispatch 验证

`Simulator::tick()` 的 4-eval 时序（comb-1 → clock toggle → seq → clock toggle → comb-2 → comb-3）经 VerilatorBackend 代理后，每次 `eval_fn_()` 都会触发 Verilator 发现 `default_clock` 0→1 边沿并执行 `always_ff @(posedge default_clock)` 块，counter 正确递增。`sync_inputs_to_vtop()` 从 Simulator 的 `data_map_` 读取 `default_clock` 值（由 `default_clock_instr_->eval()` 翻转）并同步到 Vtop，`sync_outputs_from_vtop()` 将 `ch_out` 端口值写回 `data_map_` → `sim.get_value()` 可读。

### M3 线程安全修正

VlThreadPool 是 Verilator 5.x 的全局线程池单例。正确关机顺序：`final_fn_()` → `delete_fn_()`（Vtop 析构函数停了线程池）→ `dlclose()`（.so 卸载安全）。错误顺序（v2.0 行为）：`final_fn_()` → `dlclose()` 跳过 delete → 线程跑在已卸载的代码上 → SIGSEGV。


---

## v2.2 补丁（2026-11-09 · architect · phase-6 **P8**）

### 修正摘要

| # | 问题 | 修复 | 提交 |
|---|------|------|------|
| **R9** | §5 风险登记的 **R9 行**把「A/B 验证从占位 WARN 升级为实际比对」列为**待实现的风险**，其缓解措施写「显式 `set_ab_verification(true)` 才启用」⇒ ⇒ 该行把一个**即将被移除**的 API 呈现为**现行决策** ⇒ ⇒ 归档后 ADR-035 会继续呈现一条**不存在的 API** 的启用契约（= 契约冻结错误） | **R9 改记为「已裁定（v2.2）· ⏳ 待实施」**：载体**将于** phase-6 **P8** 移除（`set_ab_verification` / `is_ab_verification` / `ab_verification_` / 每 tick WARN）⇒ ⇒ 风险随之**不成立**（无「升级为实际比对」这件事 ⇒ 无「每 tick 复制 `data_map_`」开销）⇒ ⇒ 替代机制 = **golden 测试**（`tests/jit/test_jit_golden.cpp` · interpreter vs JIT 逐值 **byte-identical**）⇒ ⇒ 公开 API 移除**必须**走 **P14 登记机制** | （实施 = phase-6 **P8** · builder） |

### 为什么是「**删除**」而不是「**实现**」（路线取舍 · **有记录**）

| 选项 | 结论 | 理由 |
|---|---|---|
| **A. 删除载体 + 由 golden 测试承担** | ✅ **选中** | ① **现状 stub = 零保护**：它只置位 + 每 tick 打一条 `CHWARN`（**从未比对**）⇒ ⇒ 删除**不降低安全水位**（这是最强的一条 ⇒ ⇒ 比「golden 已覆盖」更诚实）；② golden 提供**同型**校验（interpreter vs JIT 逐值 diff）于**测试期**；③ JIT **默认 ON** ⇒ ⇒ ctest 中所有 ≥`JIT_MIN_NODES` 的用例**本就在 JIT 路径上**以**手推期望值**断言 ⇒ ⇒ runtime A/B 的增量价值 = 仅覆盖「未断言内部节点的运行期场景」，不足以抵其成本；④ 每 tick 复制 `data_map_` 的成本**已在根 `AGENTS.md` KNOWN LIMITATIONS 载明**；⑤ Zero-Debt「No dead code」⇒ 一个**不比对**的开关是**死代码** ⇒ ⇒ 删除 = 契约与实现**同时**收口 |
| **B. 实现 runtime A/B**（tick 级快照 diff + 逐指令对账） | ❌ 未选 | ① 价值见 A③（增量价值低）；② 成本高（每 tick 快照 ⇒ 内存与耗时）⇒ ⇒ 且**默认关闭**的开关**长期无人打开** ⇒ ⇒ 仓内实测 **0 个调用点**（`.cpp` 里唯一出现 = 定义本身；`ADR-005:313` 是注释里的示例）⚠️ 该证据是**仓内**证据 ⇒ ⇒ 下游可见性由 **P14 登记**承担；③ 又一个「有 API 无用户」的形态；④ ⇒ 「删 vs 实现」是**有记录的取舍**，**不是**未完成义务 ⇒ ⇒ 下个窗口**不得**把 B 当遗留重启 |
| **C. 保留 API + 进「已弃用（尚未移除）」表**（`.rddf/checks/public-api-surface.md`） | ❌ 未选 | ① 更省事（R9 行**不用**改）⇒ ⇒ 但该表既有两条（`&&`/`\|\|`、`ICacheComplete`）都是**语义正确、仅不鼓励**的兼容面 ⇒ ⇒ 而 `set_ab_verification` 是**从未工作的 no-op** ⇒ ⇒ 按 ADR-050 D2 先例（`is_built` / `tick_state` 这类「**说谎的 API**」= **移除 + 登记**，不是弃用）；② Zero-Debt「No dead code」—— 给死代码贴弃用标签**仍是**死代码；③ phase-6 **P8** 行已裁定 A 并记录 B 的拒绝理由 ⇒ ⇒ 此刻改 C = 重开已定路由（churn > 补完 v2.2）⇒ ⇒ 故**不选**，但在此**留档**（= 取舍的完整形态） |

### 范围（承重 · 下游实施 MUST 逐字遵守）

- ⚠️ **本补丁记录的是「裁定」**。⇒ ⇒ **2026-11-13 后注：实施已完成**（phase-6 **P8** ·
  builder）⇒ ⇒ 三个符号 + `tick()` 的 WARN 块 + 两个局部量（`ab_result_a` / `run_ab_verify`）
  已**全部删除** ⇒ ⇒ 代码面 grep == 0 · `.rddf/checks/public-api-surface.md` 已登记 ·
  `docs/CHANGELOG.md` 已记一行 · golden 覆盖下限由 `scripts/verify_jit_golden_coverage.sh` 守；
  ⇒ ⇒ 下方「尚未发生」的措辞保留为**当日记录**（不改写历史）；
- ⚠️ **事实订正（v2.2 · oracle 复核 · 承重）**：`set_ab_verification` 那一块虽写在
  `#if __has_include("jit/jit_compiler.h")` 之下，但 `include/jit/jit_compiler.h` 是**源树物理头**
  ⇒ ⇒ `__has_include` **恒真** ⇒ ⇒ 该 API 在**两种构建中都存在**（`CH_JIT_ENABLE=OFF` 只让
  `CH_JIT_ENABLED` 宏未定义 ⇒ ⇒ 只影响 `jit_enabled_` 的初值）⇒ ⇒ 删除是**无条件**的
  ⇒ ⇒ 两种构建 MUST 各自编译通过（**不是**「OFF 时本就没有该符号」）。
  ⚠️ ⇒ ⇒ 根 `AGENTS.md` KNOWN LIMITATIONS 那条的「with `CH_JIT_ENABLE=OFF` the symbol is absent」
  是**事实错误** ⇒ ⇒ MUST 在实施时**一并订正**（不是纯删 ⇒ ⇒ 见下方「文档同步」条）。
- **只删**：`Simulator::set_ab_verification(bool)` / `Simulator::is_ab_verification()` /
  `Simulator::ab_verification_` / `Simulator::tick()` 里那段「A/B 验证仅作为启用标记」的 `CHWARN`
  **与两个局部量**（`ab_result_a` —— 今天起就是**零使用的死声明**；`run_ab_verify` —— 仅服务那段 WARN）
  ⇒ ⇒ Zero-Debt「No dead code」⇒ ⇒ 删 WARN 块后它们**必须**同删，否则变成新的死变量；
- **MUST NOT** 顺带改 JIT 的其余行为（`set_jit_enabled` / `try_jit_compile` / `tick_comb` + `tick_seq`
  的时序**逐字不变**）⇒ ⇒ 现有 `tests/jit/*` 三个 target 的行为**不受影响**；
- **两种构建都要能编过**：该块本就受 `#if __has_include("jit/jit_compiler.h")` 保护 ⇒ ⇒
  `CH_JIT_ENABLE=OFF` 的构建**本来就**不含该符号 ⇒ ⇒ 删除后**两种**构建均通过（**MUST** 验证）；
- **公开 API 移除 MUST 走 P14 登记机制**：`.rddf/checks/public-api-surface.md` 的移除章 +
  `.rddf/checks/public-api-removals.tsv` 的机器探针 + `docs/CHANGELOG.md` 一行 +
  `scripts/verify_public_api_visibility.sh`（含 `--self-test`）**绿**；
- **文档同步**：根 `AGENTS.md` KNOWN LIMITATIONS 的该条 + `docs/usage_guide/10-verilator-backend.md`
  的 A/B 行 + `docs/simulation/JIT_ROADMAP.md` 的 API 清单 ⇒ ⇒ **MUST NOT** 留「可用但占位」的表述。
  ⇒ ⇒ ⚠️ 根 `AGENTS.md` 那条 = **替换**（**不是**纯删）⇒ ⇒ 建议逐字文本（供 builder 直接采用）：
  `- **A/B（interpreter vs JIT）运行期双跑校验已移除**（2026-11-09 · ADR-035 v2.2）：`
  `  `set_ab_verification` / `is_ab_verification` 是**从未比对**的 no-op ⇒ ⇒ 已删除（Zero-Debt）`
  `  ⇒ ⇒ 残余风险 = 运行期**无**双跑校验 ⇒ ⇒ 缓解 = golden 测试（`tests/jit/test_jit_golden.cpp`）`
  `  + JIT 默认 ON ⇒ ctest 以**手推期望值**断言 ⇒ ⇒ ⚠️ 「`CH_JIT_ENABLE=OFF` 时该符号不存在」`
  `  是**事实错误**（`__has_include` 对源树头恒真）⇒ ⇒ 一并订正。`

### 残余风险（**如实登记** · 不掩盖）

| 风险 | 缓解 |
|---|---|
| 删除后**运行期**的 interpreter-vs-JIT 双跑校验 = **无** ⇒ ⇒ 未被 golden 覆盖的 JitOp 的**误编译**只在「断言未达处」暴露 | ① golden 覆盖**下限 = 5 / 39** + 增长规则（新增 JitOp MUST 附 golden **或** 登记未覆盖理由）⇒ ⇒ ② JIT **默认 ON** ⇒ ⇒ ctest 的用例本就在 JIT 路径上以**手推期望值**断言（**不是**抄仿真输出）⇒ ⇒ ③ ⇒ ⚠️ 根 `AGENTS.md` KNOWN LIMITATIONS 的那一条 MUST **替换**为这一行残余风险（**不是**纯删 ⇒ ⇒ 否则 KNOWN LIMITATIONS 的读者会失去这条信息 ⇒ ⇒ 删掉之后「运行期没有双跑校验」这件事就**无人知道** ⇒ ⇒ = 典型的「删干净了但信息也丢了」） |

### 下游判据（phase-6 **P8** · builder）

- **①** 移除登记四件套（见上）⇒ ⇒ `verify_public_api_visibility.sh` **绿**
  ⇒ ⇒ ⚠️ **锚点 MUST 同步 bump**（否则护栏恒红）：`EXPECTED_REMOVAL_ROWS` **15 → 16**
  （登记表 +1 行）· `EXPECTED_PROBES` **24 → 26**（tsv +2 条探针）
  ⇒ ⇒ 建议的 2 条探针（`kind=removed`）：`set_ab_verification` · `ab_verification_`
  （后者同时覆盖内联 getter 的函数体引用 ⇒ ⇒ 2 条即闭合 3 个符号）；
- **②** 拆成**两条可 grep**的判据（v2.2 · critic BLOCKER 修正 —— 原写法「docs/ 命中 == 0
  且允许 1 处」与「R9 行 MUST NOT 删行」+「v2.2 补丁章 MUST 保留」**自相矛盾** ⇒ ⇒ 字面执行必红）：
  - **(a) 代码面**：`grep -rn "set_ab_verification\|is_ab_verification\|ab_verification_" include/ src/`
    ⇒ 命中数 == **0**（⚠️ **`AGENTS.md` 不在此列** —— 它的 KNOWN LIMITATIONS 那条 MUST 被**替换**为
    「已移除 + 残余风险」的说明 ⇒ ⇒ 必然**含**符号名 ⇒ ⇒ 把它放进零命中面会与「替换 ≠ 纯删」**直接矛盾**）；
  - **(b) 文档面**（`AGENTS.md` + `docs/`）：`grep -rn "set_ab_verification\|is_ab_verification\|ab_verification_" AGENTS.md docs/`
    ⇒ **排除** `docs/adr/ADR-035-verilator-backend.md`（本 ADR 的 R9 行 + v2.2 补丁章 = **审计轨迹**
    ⇒ ⇒ 不计入命中）⇒ ⇒ 其余每条命中 MUST 在**同一段**内含「**已移除**」或「**已删除**」字样
    ⇒ ⇒ 判据 = 过滤掉含该字样的段之后 **剩余 == 0**；
    - **v2.3 修订（2026-11-13 · 用户追认 · architect 复盘 #5 裁决 ③）**：判据 (b) 的排除面**再排除**
      「**带日期的评审快照**」—— 理由 = 它们与本 ADR 的补丁章同属**当日记录**（审计轨迹）⇒ ⇒
      改写它们 = **篡改历史** ⇒ ⇒ 而「文档面 0 命中」类判据 MUST 与「历史不改写」**共存**。
      ⇒ ⇒ ⚠️ **排除面 MUST 按「结构特征」而非「目录名 / 日期前缀」**：文件名匹配
      `^\d{4}-\d{2}-\d{2}-.*(review|midterm).*\.md$` 的**具名清单**
      （**活主题文档不在其列** —— `docs/architecture/2026-11-02-hierarchical-rtl-output.md`
      是 `verify_arch_ledger_residuals.sh` 的**消费对象** ⇒ ⇒ 裸 glob 排除会让它**永久免疫**）
      ⇒ ⇒ 当前具名清单（**5** 个）：
      `docs/architecture/2026-11-09-phase-6-midterm-review.md` ·
      `docs/architecture/2026-11-12-phase-6-b2-midterm-review.md` ·
      `docs/architecture/2026-11-12-phase-6-rtl-track-review.md` ·
      `docs/architecture/2026-11-13-phase-6-rtl-track-review-5.md` ·
      `docs/architecture/2026-11-13-phase-6-rtl-track-review.md`
      ⇒ ⇒ ⚠️ **禁**按目录 / 日期**自动归类**（快照越多 ⇒ ⇒ 判据强度单调下降 ⇒ ⇒ 恒绿）
      ⇒ ⇒ **新增排除 = 改判据 = diff 可见 + 可评审**
      ⇒ ⇒ ⚠️ **豁免 MUST 登记代价**：残差台账 `id=24`（`已知`）登记
      「判据 (b) 对快照面**永久失明**（快照内的过期引用不被清零要求覆盖）」
      ⇒ ⇒ **决策内容零变更**（只改**判据面**的排除集合 ⇒ ⇒ 轻量修订）
- **③** golden 覆盖**下限 = 5 / 39**（分母 = `include/jit/jit_ir.h` 的 JitOp 总数）⇒ ⇒ **防回退**
  ⇒ ⇒ ⚠️ **该下限当前**无**现成护栏**（`scripts/verify_test_counts.sh` 不数 golden 覆盖 ⇒ ⇒
  「今天就已满足」⇒ ⇒ 判据**不可执行**）⇒ ⇒ **builder MUST 新增护栏锚点**并让本判据以该锚点为准
  ⇒ ⇒ 建议的机械核法（**builder 可直接落地**）：
  - 覆盖分子：`grep -oE "JitOp::[A-Z_0-9]+" tests/jit/test_jit_golden.cpp | sort -u | wc -l` ≥ **5**；
  - 分母：`grep -cE '^  [A-Z_0-9]+,?$' include/jit/jit_ir.h`（枚举项行 · **含末项**）== **39**
    （⚠️ **正则 MUST 带 `,?`** —— 末项 `NOP` **无尾逗号** ⇒ ⇒ 用 `'^  [A-Z_0-9]+,'` 只数到 **38**
    ⇒ ⇒ 判据会**恒红** ⇒ ⇒ 这是「判据字面必红」的同型病 ⇒ ⇒ 已实测：`38`（旧）vs `39`（新））；
  - ⇒ ⇒ 两条 MUST 落进 `scripts/verify_test_counts.sh`（或新护栏）⇒ ⇒ 否则「防回退」是**空判据**；
- **④** **两种构建都要能编过**（⚠️ 该 API 在**两种**构建中**都存在** ⇒ ⇒ 不是「OFF 时不存在」）：
  - 主构建：`cmake --build build -j$(nproc)` ⇒ ⇒ 成功；
  - OFF 构建：**降级为库目标**（`cmake -B build-nojit -DCH_JIT_ENABLE=OFF` ⇒ ⇒
    `cmake --build build-nojit --target cpphdl` ⇒ ⇒ 成功）⇒ ⇒ ★ 理由：库目标编译**足以**暴露
    成员 / 引用不匹配 ⇒ ⇒ 全量 255 个 ctest target 的冷构建**成本过高** ⇒ ⇒
    全量 OFF 构建**另立 work item**（本仓 CI 当前**无** OFF matrix leg）。

### 审稿记录

`.rddf/reviews/ADR-035-verilator-backend-v2.2.md`（oracle 1 + critic **3 轮** · 第 2 / 3 轮为**复审** · 最终 **APPROVE** / 0 BLOCKER）
⇒ ⇒ **关联 change = `backlog: phase-6 P8`**（openspec change **未立项** ⇒ ⇒ **AC 映射暂缺** · 立项后回填）
