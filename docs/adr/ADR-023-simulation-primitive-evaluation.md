# ADR-023: 仿真原语评估（simTimeout 最小化方案）

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户（Oracle 调研参考）

---

## 1. 背景

议题 #17 评估是否在 Simulator 中实现 SpinalHDL 风格的仿真原语：fork/join、sleep(cycles)、simTimeout(N)、simSuccess/simFailure。

### 1.1 当前 Simulator API

```
tick() / tick(N)      ✅  时钟步进
eval()                ✅  组合求值
reset()               ✅  复位
get_value/set_value   ✅  端口访问
reinitialize()        ✅  重新初始化
```

### 1.2 缺失原语对照

| SpinalHDL | C++ 替代 | 是否足够 |
|-----------|---------|---------|
| fork/join | std::thread | ❌ Simulator data_map_ 无锁，OS 线程不安全 |
| sleep(cycles) | tick(N) | ✅ 语法糖 |
| simTimeout(N) | 手动 MAX_TICKS | ⚠️ 易遗漏，散落在各测试中 |
| simSuccess/Fail | Catch2 REQUIRE/FAIL | ✅ 已足够 |

---

## 2. 分析过程

### 2.1 Oracle 调研

| 调查项 | 结论 |
|--------|------|
| 17 个移植示例模式 | 全部顺序：set→tick→get，无一需并发 |
| Simulator 线程安全 | data_map_ 无锁，tick() 无同步，OS 线程直接竞争 |
| fork/join 实质 | SpinalHDL 的 fork 是协作式协程，不是 OS 线程 |
| simTimeout 需求 | 多个示例已手动实现 MAX_TICKS，说明是普遍需求 |
| simSuccess/Fail | Catch2 的 REQUIRE/FAIL 已覆盖 |

### 2.2 历史证据

```cpp
// spi_controller.cpp:254 — 手动 MAX_TICKS 模式
// counter.cpp — 类似的 cycle count 硬上限
// test_riscv_tests.cpp:42 — MAX_TICKS = 10000
```

这些手动实现散落在各测试文件中，缺少统一机制。

---

## 3. 决议

### 3.1 最终决定：仅实现 simTimeout

| 原语 | 决策 | 理由 |
|------|------|------|
| **simTimeout(N)** | ✅ **实现** | 防止测试无限循环，多文件已手动实现 |
| sleep(cycles) | ❌ 跳过 | tick(N) 已覆盖，语法糖收益不足 |
| simSuccess/Fail | ❌ 跳过 | Catch2 REQUIRE/FAIL 已足够 |
| fork/join | ❌ 推迟 | 当前无需求，Simulator 内部无锁，实现成本高 |

### 3.2 simTimeout 设计要点

**API 设计**（建议）：

```cpp
class Simulator {
public:
    void set_timeout(uint64_t max_cycles);  // 设置超时周期数
    bool has_timed_out() const;              // 查询是否超时
    // tick() 内部增加：if (max_cycles_ > 0 && ticks_ >= max_cycles_) timed_out_ = true;
    
private:
    uint64_t max_cycles_ = 0;   // 0 = 不超时
    bool timed_out_ = false;
};
```

**默认行为**：`max_cycles_ = 0` 表示不启用超时，不影响现有代码。

**在 tick() 中的集成点**：`simulator.cpp:804` tick() 主循环末尾或开头检查 `ticks_ >= max_cycles_`。

---

## 4. 执行计划

| 步骤 | 内容 | 文件 | 工作量 |
|------|------|------|--------|
| 1 | Simulator.h 添加 `set_timeout()` / `has_timed_out()` / `max_cycles_` / `timed_out_` | `include/simulator.h` | <15min |
| 2 | tick() 中添加超时检查 | `src/simulator.cpp` | <5min |
| 3 | 添加测试：test_sim_timeout.cpp | `tests/test_sim_timeout.cpp` | <30min |

---

## 5. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录仿真原语评估结果，仅实现 simTimeout | Sisyphus |

---

**相关链接**:
- `include/simulator.h` — Simulator 类定义
- `src/simulator.cpp` — Simulator 实现（tick() 入口）
- `examples/spinalhdl-ported/` — 17 个移植示例（全部顺序模式）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #17
