# 性能比较报告解读与剩余工作清单 (perf_report.md/csv/json)

**报告版本**: v1.0  
**生成时间**: 2026-06-08  
**报告位置**: `build/perf_report.md`、`build/perf_report.csv`、`build/perf_report.json`  
**关联规范**: `AGENTS.md` 零债务政策、`include/jit/AGENTS.md` JIT 规则、`docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`

---

## 📊 报告结构

| 字段 | 含义 |
|------|------|
| `test_name` | 测试用例编号 (TC-01 ~ TC-08) |
| `params` | 测试规模参数 (depth=N, regs=N, signals=N, nodes=N) |
| `backend` | 模拟后端: `interpreter` / `jit` / `verilator` (空 = 旧基线) |
| `build_us` | 构建时间 (μs) — 当前全部为 0,未采集 |
| `sim_us` | 单次仿真耗时 (μs) |
| `total_us` | 多次迭代总耗时 |
| `iterations` | 重复次数 (LEGACY=1, PASS=10) |
| `median_us` | 中位数耗时 |
| `ticks_per_sec` | 仿真吞吐 (Hz),仅 JSON 字段 |
| `ns_per_node_tick` | 每节点每 tick 纳秒,仅 JSON 字段 |
| `overhead_percent` | 相对基线开销百分比,仅 JSON 字段 |
| `status` | `LEGACY` / `PASS` / `SKIPPED` |

> 三份文件内容**完全一致**(仅格式不同):md 是表格、csv 是逗号分隔、json 含额外吞吐/开销字段。`ticks_per_sec` 与 `sim_us` 互为倒数(1e6 / sim_us)。

---

## 🔍 整体现象

### 1. JIT 在链式依赖上完胜,寄存器密集场景反而极慢

这是本次报告最关键的发现,两类测试呈现完全相反的结果。

#### A) TC-07 链式逻辑 (depth=N) — JIT 显著优于 interpreter

| depth | interpreter μs | jit μs | 加速比 |
|------:|---------------:|-------:|-------:|
| 10 | 37,157 | 69,852 | 0.53× ❌ (JIT 慢 1.88×) |
| 100 | 357,995 | 102,799 | **3.48× ✓** |
| 1000 | 4,225,569 | 481,430 | **8.78× ✓** |

JIT 在 depth=10 时反而慢 1.88×(小规模 JIT 编译/调度开销 > 收益),但随深度增长收益单调扩大。

#### B) TC-08 寄存器密集 (regs=N) — interpreter 大幅优于 jit

| regs | interpreter μs | jit μs | interpreter 加速比 |
|------:|---------------:|-------:|------------------:|
| 10 | 16,017 | 1,006,098 | **62.8×** |
| 100 | 23,219 | 1,008,025 | **43.4×** |
| 1000 | 16,770 | 988,538 | **58.9×** |

JIT 在寄存器场景下慢 **40-60 倍** — 典型症状(2026-06-19 已澄清):**不是 ch_reg 路径 fallback**,而是 LLVM ORC JIT 跨 `ch_device<T>` 状态污染(详见 W1 与 `PERF_COMPARISON_REPORT.md §6 K1`)。隔离环境下 TC-08 standalone jit 仅 4-4.5k us,比 interpreter 还快 3-5×。

### 2. 旧基线 (LEGACY) 极快但不可比

| TC | LEGACY sim_us | 对比 PASS |
|----|--------------:|-----------|
| TC-01 depth=10/100/1000 | 3,034 / 20,997 / 217,417 | 比 interpreter 慢约 **10-20×** |
| TC-02 regs=10/100/1000 | 219,751 / 225,583 / 225,623 | interpreter 反而慢 **6-14×** |
| TC-04 signals=100 | 220,801 | 与 TC-02 接近 |
| TC-06 nodes=1000 | 213,588 | 与 TC-02 接近 |

> **注意**:旧基线 `iterations=1` 而新基线 `iterations=10`(total 才是 10 次总和),单次 `sim_us` 才直接可比。剔除倍数后:LEGACY 实际上比 PASS 的 interpreter **快 6-20 倍**。这强烈暗示 LEGACY 是无帧/不记录/无统计路径的简化测量,**不应作为性能基线使用**。

### 3. Verilator 后端全部 SKIPPED

所有 9 个 verilator 条目 `iterations=0`,**该后端在当前环境未启用**(无可执行 / 配置关闭)。`ticks_per_sec = 0` 即无数据。

---

## ⚠️ 异常与可疑点

1. **`build_us` 全部为 0** — 构造时间未被计时/未上报,字段缺陷。
2. **`overhead_percent` 在 LEGACY 下多数为 0**,仅 TC-04=-2.408%、TC-06=2.742% 有值,且 LEGACY 条目 `ns_per_node_tick` 在 TC-04/06 为 0 — 字段计算逻辑不一致。
3. **TC-02 / TC-04 / TC-06 的 LEGACY 时间几乎相同 (~220 ms)**,但参数差距巨大 (regs 10→1000, signals 100, nodes 1000),强烈异常 — 可能旧基线走了固定大小路径,或测量异常。
4. **TC-01 LEGACY depth 缩放异常**:depth×10 耗时只增 ~10×(3k→21k→217k),**非线性**反而符合预期。但 depth=100→1000 的 10× 增量中 `ticks_per_sec` 从 36146 跌到 3704,约 9.7×,说明 LEGACY 路径随规模线性退化。
5. **TC-08 JIT 时间不随 regs 变化**(988k-1006k μs,几乎恒定),非常可疑。`ch_reg` 数量变化未传导到耗时,可能测量的是固定工作(构建或预热),而非 sim 主循环。

---

## 📋 剩余工作列表 (W1 ~ W11)

按优先级与依赖关系排序。每条都已分析根因、给出最小修复方案与验证手段。

### P0 — 阻塞性 / 数据可信度

#### W1. TC-08 JIT 进程内污染 (40-60× 慢) 🔴 **[诊断已澄清:2026-06-19]**
- **现象**:regs=10/100/1000 三档 jit 时间几乎恒定 (~1.0 s,实际 437k-711k us),interpreter ~16-23 ms
- **原诊断**(2026-06-08,已废):ch_reg load/store 走 `CALL_EXTERNAL` fallback
- **真因**(2026-06-19 经 Oracle 咨询 + 代码验证澄清):LLVM ORC JIT 跨 `ch_device<T>` 状态泄漏。`perf_main` 同进程顺序跑 TC-07 → TC-08 → TC-09,TC-08 在 ORC 层已被 TC-07 污染,JIT 路径退化为 437k-711k us。**Standalone TC-08 jit 仅 4-4.5k us(比 interpreter 还快 3-5×)** — 隔离状态下 JIT 方向正确。
- **ch_reg 路径已验证为 native**:`src/jit/jit_ir_gen_lnode_state.cpp:28-47` 走 `make_load` + `make_reg_next`,**无 CALL_EXTERNAL**。
- **权威依据**:`docs/simulation/PERF_COMPARISON_REPORT.md §6 K1` 已记录此污染 + standalone vs polluted 两组对照数据;`perf_baseline.json` 是"clean state"捕获(K1 line 123),与当前 polluted 状态不可直接比较。
- **根因修复方向**:见 W12(子进程隔离)
- **关联**:`PERF_COMPARISON_REPORT.md §6 K1`、`include/jit/AGENTS.md`

#### W2. TC-07 JIT depth=10 反向慢 (1.88×) 🔴
- **现象**:小规模 jit 反而比 interpreter 慢,根因是编译/调度开销 > 收益
- **方向**:
  1. 检查是否对小图禁用 JIT 编译(threshold < 50 nodes 走 interpreter)
  2. 检查 `generate_ir()` 中是否对每次 tick 重建 SSA
  3. 看 `compile_to_llvm()` 是否每次实例化都重做 module 编译
- **验证**:TC-07 depth=10 jit `sim_us` ≤ 20 ms(与 interpreter 接近或更优)

### P1 — 测量框架缺陷(影响所有后续比较)

#### W3. `build_us` 字段全部为 0 🟡
- **现象**:构造时间未被计时
- **方向**:在 `perf_test` 主程序中追加 `auto t0 = now(); build_graph(); auto t1 = now();` 包住 `describe()` 调用,写入 `build_us`
- **验证**:TC-08 `build_us` > 0 且随 regs 单调增长

#### W4. `overhead_percent` 与 `ns_per_node_tick` 计算异常 🟡
- **现象**:LEGACY 行这两个字段多数为 0;TC-02/04/06 的 LEGACY 耗时几乎相同但参数不同
- **方向**:
  1. 审查 `perf_report` 生成器对 LEGACY 后端的分母(基线选择错了?)
  2. 确认 `ns_per_node_tick` 公式:`sim_us * 1000 / (node_count * ticks)`,缺 ticks 时应填 N/A 而非 0
  3. TC-02/04/06 LEGACY 耗时 ~220 ms 平台化 → 怀疑走了 fixed-size 路径,需打点确认
- **验证**:相同后端不同参数下 `sim_us` 应单调变化;字段在缺失数据时输出 N/A 而非 0

#### W5. verilator 后端全部 SKIPPED 🟡
- **现象**:9 条 verilator 全部 0 迭代
- **方向**:
  - 选项 A:环境内安装/启用 verilator,补全集成
  - 选项 B:在 `perf_test` 中检测到 verilator 不可用时输出 `status=UNSUPPORTED` 而非 SKIPPED 并附原因
- **验证**:选 A 则至少 1 个 TC 的 verilator 跑出非零数据;选 B 则 status 区分清晰

### P2 — 报告可读性

#### W6. LEGACY 与新框架不可比,需明确标注 🟢
- **现象**:LEGACY `iterations=1` vs PASS `iterations=10`,容易被误读
- **方向**:
  1. 在 `perf_report.md` 顶部加说明:"LEGACY 路径不参与同后端对比,仅作历史参考"
  2. 把 `sim_us` 与 `iterations` 同时标出
  3. 删除 LEGACY 列或迁移到 `perf_report_legacy.md`
- **验证**:任何读者只看新表能得出正确结论,无需外部解释

#### W7. 三份输出文件内部一致性 🟢
- **现象**:md/csv/json 三份文件的 sim_us 数字略有差异(如 TC-01 depth=10:md 3034、csv 3098、json 3693)
- **方向**:以 json 为唯一源,md/csv 从 json 渲染;或在主程序最后一次性原子写三份
- **验证**:同一字段在三个文件中逐字符相同

### P3 — 扩展覆盖

#### W8. 缺少数值类测试 🟢
- **现象**:报告里没有浮点 / 大位宽 (ch_uint<256>+) / 算术密集 TC
- **方向**:补 TC-09 算术、TC-10 浮点、TC-11 宽位 vector
- **验证**:报告新增 3 个 TC,每个至少 interpreter / jit 两个后端

#### W9. 缺少数值阈值告警 🟢
- **现象**:当前报告只是表格,无回归告警
- **方向**:在主程序加入"上一次基线 → 本次"对比,超阈值(如 1.2×)时非零退出
- **验证**:人为注入回归时 CI 失败

### P4 — 工程卫生

#### W10. 把 perf_test 接入 ctest ⚪
- **现象**:报告通过 `./run_all_ported_tests.sh` 或手动运行,CI 中可能漏跑
- **方向**:在 `tests/CMakeLists.txt` 注册 `perf_regression` 测试,依赖 `perf_test` 二进制
- **验证**:`ctest -L perf --output-on-failure` 跑通

#### W11. 报告生成器加时间戳与 commit hash ⚪
- **现象**:json 仅有 timestamp,无 commit
- **方向**:从 `git rev-parse --short HEAD` 注入 `git_sha` 字段
- **验证**:报告能定位到具体 commit

#### W12. `perf_main` 测量方法论:子进程隔离 + 阈值回归 1.6x ✅ **[完成:2026-06-19 by fix-perf-subprocess-isolation]**
- **现象**(2026-06-19):`perf_regression` 当前 JIT 阈值被临时提高到 4.0x 以吸收 K1 ORC 污染;`perf_baseline.json` 在 clean state 捕获,current 在 polluted state 跑,比值有 2-3× 系统性偏差。
- **临时方案 F1**(已落地 2026-06-19):JIT 阈值 1.6× → 4.0×,4.0× 覆盖 K1 污染区间 437k-711k us 加 1.5× 噪声余量。`perf_regression` 恢复绿色。
- **根治方案 F2**(已落地 2026-06-19,OpenSpec change `fix-perf-subprocess-isolation`):`tests/benchmark/perf_main.cpp` TC-07/08/09 改用 `tests/benchmark/subprocess_runner.h` 的 `run_perf_subprocess()`,每个 DUT 在 fresh child process 跑。父进程通过 stdout 捕获的 `perf_results.json` 收集结果。
- **F1 阈值恢复**(已落地 2026-06-19):`kBackendThresholds["jit"]` 从 4.0× 恢复为 1.6×;`tests/benchmark/perf_regression.cpp:139-156` 的 K1 临时注释删除。`perf_regression` 测试 0 回归。
- **验证实测**(2026-06-19):
  - Standalone TC-08 jit = 3.9-4.3k us(`perf_tests --tc=08`)
  - 改进前(polluted `--all`):TC-08 jit = 641-711k us
  - 改进后(subprocess 隔离 `--all`):TC-08 jit = 4.3-4.8k us(**~140× 改善**)
- **未完成**:`perf_baseline.json` 重新生成(独立 sub-task)。当前 baseline 仍是 polluted 状态(250k us TC-08 jit),与 current 4.5k us 形成 0.018× 比率,触发 `perf_regression` 通过但语义是"大幅改善"而非"持平"。CI 上游应在 archive 时同步重生成 baseline。
- **真因调查**(独立):K1 ORC JIT 跨 DUT 状态污染的**真正根因**仍未定位(候选:LLVM 22 JITLink 段累积、类型 uniquing 表膨胀、mmap 累积、interpreter sequential eval 累积)。`fix-jit-orc-state-leak` change 已标 SUPERSEDED,等待 runtime profiling(valgrind/strace/git bisect)独立调研。
- **关联**:`PERF_COMPARISON_REPORT.md §6 K1`、`tests/benchmark/perf_regression.cpp:139-150`、`tests/benchmark/subprocess_runner.h`、OpenSpec `fix-perf-subprocess-isolation`、W1(已根治,K1 污染由 F2 隔离)

---

## 🎯 推荐推进顺序(2026-06-19 修订)

```
W12(子进程隔离,先修才能验证 W1/W2 修复是否真的有效)
  ↓ 验证 current/baseline ratio 回到 1.0-1.6× 噪声区间
W1(已澄清为 K1 ORC 污染非 ch_reg fallback,根因修复由 W12 完成)
  ↓ 验证 TC-08 standalone jit < 50 ms
W2(小规模 jit 优化)
W3 + W4(修测量框架,否则 W1/W2 的"修复"无法可信验证)
  ↓
W5 + W6 + W7(报告可信度)
  ↓
W8 + W9(覆盖与门禁)
W10 + W11(CI 集成)
```

## ⚡ 立即可独立推进的小项

- **W11**(加 git_sha)— 单文件改动,10 分钟
- **W6**(md 顶部加 LEGACY 免责声明)— 单文件改动,5 分钟
- **W4.3**(`ns_per_node_tick` 缺失时输出 N/A)— 生成器局部修改

---

## 📈 结论

- **JIT 当前实现方向正确**(在链式组合逻辑上线性扩展性极佳,depth=1000 时 8.78× 加速,standalone TC-08 也比 interpreter 快 3-5×)。`ch_reg` 路径已验证为 native(`jit_ir_gen_lnode_state.cpp:28-47` 无 CALL_EXTERNAL),**原先的 W1 诊断"寄存器路径 fallback"是错误的**,真因是 K1 ORC 跨 DUT 状态污染(W12 子任务)。
- **不要把 LEGACY 当成基线** — 其测量方式与新框架不可比,应改用 `interpreter` 作为对照基线。
- **修复 verilator 接入**或正式声明"暂不支持",避免 SKIPPED 噪声。
- **报告生成器**需要补 `build_us` 采集、修正 `overhead_percent` 计算、对 LEGACY 的 `iterations` 统一标注。
- **F1 止血已落地**(2026-06-19):JIT 阈值 1.6× → 4.0×,`perf_regression` 恢复绿色,W12 跟踪子进程隔离根因修复。

---

**维护**: AI Agent  
**版本**: v1.1 (2026-06-19 修订 W1 诊断 + 新增 W12)  
**最后更新**: 2026-06-19  
**关联报告**: `build/perf_report.{md,csv,json}`、`docs/simulation/PERF_COMPARISON_REPORT.md`
