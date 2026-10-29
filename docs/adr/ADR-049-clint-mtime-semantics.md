# ADR-049: CLINT `mtime` 的语义模型 —— 裁决「自由运行计数器」与写优先 / 分频 / 归属 capability

- Status: Accepted（proposed 2026-10-30）
- Date: 2026-10-30
- Author: build-agent（architect role）
- Parent: ADR-046（RV32I 单源化 —— `include/cpu/` 是微架构唯一权威）· ADR-035（Verilator 后端 —— 64-bit 端口上限）· ADR-050（FSM 语义 —— 本 ADR 沿用其「契约 + 护栏 + 变异验证」三件套的落地形态）
- 关联 OpenSpec change：**待立项**（实施本 ADR）
- 关联 capability：**新建** `cpu-clint-timer`（ADDED ×N ⇒ 归档**禁** `--skip-specs`）
- 编号：⚠️ **049 由此前预留**（`.rddf/improvements/clint-mtime-autoincrement-2026-10.md:74,84`）⇒ ADR-050 明确让出该编号（见其 §头「编号」行）⇒ 本 ADR 取 **049**
- 前置审查：本 ADR 的两个候选实现形态经**独立探针实测**（§1.4）⇒ 两条**实现陷阱**是实测发现，不是推理

## 1. Context

### 1.1 触发

`include/cpu/riscv/clint.h` 的 `mtime` 在 RISC-V 规范里是**自由运行的机器定时器**（software 靠读它做时基），但当前实现里它**只能由软件经写端口显式写入** ⇒ 只构成「**静态比较器**」。

该缺口由 change `riscv-mini-test-triage-2026-10` 实测挖出（修 `test_interrupt_flow` 的断言时）⇒ 该 change 已把 `clint.h` 的文件头注释改成**如实描述**（不再声称「每个 `tick()` 递增」），并删掉了那个**空实现**的 `tick()` 与 2 个恒返 0 的占位 getter ⇒ **缺陷被登记但未修**。

### 1.2 现状：实现是「保持 + 可写」

```cpp
mtime_lo->next = select(write_mtime_lo, io().write_data, mtime_lo);   // 保持
mtime_hi->next = select(write_mtime_hi, io().write_data, mtime_hi);   // 保持
```

⇒ 无任何递增源 ⇒ `mtime` 在整个仿真期间**恒为软件写的那个值** ⇒ `mtip = (mtime >= mtimecmp)` 退化为「**软件写过的比较值**的比较」⇒ 软件不写就不产生定时器中断。

### 1.3 归属缺口：CLINT **不在任何 capability** 覆盖内

实测：`grep -rn "clint" openspec/specs/*/spec.md` 仅在 `verilator-rv32i-e2e` 里有一句「本路径不触发」⇒ ⇒ **没有任何 capability 规定 CLINT 的语义** ⇒ ⇒ 本次是**契约变更** ⇒ ⇒ **归属 capability 本身是待裁决项**（D7）⇒ ⇒ delta 在 ADR 落盘前写不出来 ⇒ ⇒ 本 ADR 是立项的**强前置**（禁先登记 change 再标 `skip_specs`）。

### 1.4 规模事实与**实测**结论（探针，`/tmp/opencode/probe_clint{,2}.cpp`）

- CLINT 的仓内实例化点：**仅** `examples/riscv-mini/tests/test_interrupt_flow.cpp`（2 处）⇒ **未**接入 SoC（`rv32i_pipeline_top.h` 里没有 CLINT 实例）⇒ ⇒ 本次改动爆炸半径**极小**；
- `mtime` 的消费者：仅该测试 ⇒ 期望值需重新推导的**只有 1 个文件**；
- **实测**：两个候选内部表示（见 D6）**语义等价**，逐拍序列一致（`hi/lo` = `1/0 → 1/1 → 1/50 → 1/51`）；
- ⚠️ **实测挖出两条实现陷阱**（**必须**写进本 ADR，否则实施期会重踩）：
  1. **进位 MUST 用硬件原语**：`carry ? 1_d : 0_d` 是 **C++ 三元** ⇒ 在 `ch_bool` 上是**建图期**求值（`ch_bool::operator uint64_t()` 只对 const 节点返真值）⇒ **恒取 0** ⇒ 计数器**永远跨不过 32 位边界**（探针第一版实测：`hi` 恒 0）；
  2. **64 位「保高半换低半」MUST 用 `bits_update<32>`**：`<<` 在 64 位上**编译不过**（框架 shift 实现用 C++ `1 << N` ⇒ `1 << 64` 溢出）；而手写 `hi | lo`（**漏了移位**）⇒ **静默清零高半**（探针第一版实测：`hi` 变 0）。

## 2. Decision

### D1. `mtime` 的语义：**64 位自由运行计数器**

`mtime` MUST 是 64 位**自由运行计数器**：每个递增拍自增 **1**（无符号回绕），不再是「软件写的静态值」。

### D2. 递增频率：**可配置分频**（编译期常量）

`Clint` MUST 提供编译期常量 `TIMER_DIVIDER`（默认 **1** = 每拍递增 1）⇒ `mtime` 每 `TIMER_DIVIDER` 个时钟沿递增 1。

- **分频相位（ MUST 可测）**：内部计数器 `d` 复位为 0；递增发生在 `d == TIMER_DIVIDER - 1` 的那一拍 ⇒ **复位后第 `TIMER_DIVIDER` 拍**发生第一次递增，随后 `d` 归 0；
- ⚠️ **写拍的分频计数器 MUST 照常推进**（相位 = **复位后拍数的纯函数** ⇒ 与写**无关**）⇒ 写拍若恰是递增拍，该次递增**被丢弃**（写优先，D3）⇒ ⇒ **不**存在「写复位分频器」与「写暂停分频器」两种实现都能满足本 ADR 的空间。

### D3. 软件写与递增的优先级：**写优先**（RISC-V 允许校时）

- 软件写 `mtime`（lo 或 hi）的**那一拍**：被写的 32 位取**写值**，另一 32 位**保持**，且**本拍不递增**；
- 未被写的拍：`mtime` 按 D2 递增；
- ⇒ 写后的值成为**下一次**递增的起点（= 「写后从写值继续递增」）。

### D4. `mtime` 复位值：**0**

与 `mtimecmp` 的复位值 `0xFFFFFFFF`（全 1）形成对比 ⇒ 复位后 `mtime < mtimecmp` ⇒ **复位后不产生定时器中断**（现状语义**不变**）。

### D5. 64 位原子读：**允许撕裂**（**不**新增端口）

软件经 32 位 `read_data` + 地址偏移分别读 `mtime` 的低 / 高半 ⇒ 两次读之间计数器可能已进位 ⇒ **读值可能撕裂**。RISC-V 规范允许（软件应读两次高半检测翻转）⇒ ⇒ **不**新增分离 / 原子读端口 ⇒ 记为**已知边界**（§4「不在本次范围」）。

### D6. 内部表示：**实施者自选**（两种均已实测可行）；推荐**单一 64 位寄存器**

- **形态 A**：两个 32 位 `ch_reg` + 显式进位（读写路径最简，代价是进位时序）；
- **形态 B**：一个 64 位 `ch_reg` + `bits<>` 切片（进位**自动正确**，代价是「保高半换低半」需 `bits_update<32>`）；
- ⇒ 两者**语义等价**（§1.4 实测）⇒ ⇒ 本 ADR **不强制**形态，只**强制可观测语义**（D1–D5）+ **强制规避 §1.4 的两条陷阱**；
- **推荐形态 B**：进位逻辑**不存在** ⇒ 少一类 bug；但形态 A 的读写路径更短 ⇒ 二者皆可，**由实施者按 §1.4 的陷阱清单自证**。

### D7. capability 归属：**新建** `cpu-clint-timer`

CLINT 是**外设**，其语义与 RV32I 流水线微架构**无关** ⇒ ⇒ **不**塞进 `rv32i-canonical-home`（那是「微架构单源化」的归属）⇒ ⇒ **不**塞进 `cpu-cache-subsystem`（cache 与 CLINT 无语义关系）⇒ ⇒ **新建** `cpu-clint-timer` capability。

⚠️ **命名**：前缀按**子系统**而非命名空间取（仓内既有：`rv32i-*` = 微架构 / `cpu-cache-subsystem` = `include/cpu/` 的 cache 子系统 / `chlib-*` = chlib）⇒ CLINT 属 `include/cpu/` 的**外设**子系统 ⇒ 取 `cpu-` 前缀 ⇒ **不**取 `rv32i-`（那会把「与微架构无关」写成自反）。⚠️ 注意 CLINT 的**命名空间**是 `ch::chlib`（`include/cpu/AGENTS.md` §ANTI-PATTERNS 已登记的命名空间不一致）⇒ **命名空间不是命名的依据**。

### D8. 契约同步：**MUST** 走 OpenSpec change（**禁** `--skip-specs`）

本 ADR **有**契约变更（新增 capability）⇒ ⇒ 实施 change MUST 带 `cpu-clint-timer` 的 **ADDED** delta ⇒ ⇒ 归档**禁** `--skip-specs`（否则契约变更被静默丢掉 —— 正是根 `AGENTS.md` 明令禁止的错误）。

## 3. 备选方案（否决）

| 选项 | 结论 | 理由 |
|---|---|---|
| **S1** 保持「静态比较器」（只改注释，不改行为） | **否决** | 缺口是**真实**的：RISC-V 软件靠 `mtime` 做时基；当前实现让「定时器中断」只能由软件自己写 `mtime` 触发 ⇒ 语义上与规范不符 ⇒ **不是**「够用」而是「名不副实」 |
| **S2** `mtime` 每拍递增，**固定**频率（不可配置） | **否决** | 定时器频率是**平台常量**（通常 ≠ CPU 时钟）⇒ 不可配置 ⇒ SoC 只能靠「跑快 N 倍」凑 ⇒ 表达力不足；且**可配置分频**的实现代价仅一个计数器（默认 1 ⇒ 零代价）⇒ 无理由拒绝 |
| **S3** **运行期**可配置频率（新增 `tick_en` 输入端口） | **否决** | 新增输入端口 ⇒ 全部实例化点 MUST 显式驱动（根 `AGENTS.md` §ANTI-PATTERNS 登记的「未驱动端口读到未定义值」类缺陷）⇒ 而频率在**编译期**即可确定 ⇒ 引入端口不带来任何表达力收益 |
| **S4** 递增**优先**于软件写（写被忽略） | **否决** | RISC-V 允许软件写 `mtime` **校时** ⇒ 写优先才能表达「校时」⇒ 递增优先 = 规范不允许且**无法校时** |
| **S5** 软件写 `mtime` 时**整块** 64 位被覆盖（lo 写 ⇒ hi 也归零） | **否决** | lo / hi 是**两个独立可写寄存器**（RISC-V 允许只写一半）⇒ 整块覆盖会丢失另一半 ⇒ 语义错误 |
| **S6** 写 `mtime` 的那一拍**仍递增**（写值 + 1） | **否决** | 「写值 = 当前时刻」的直觉被破坏（写 50 读回 51）⇒ 与「校时」语义不符 ⇒ 且**跨 32 位边界**时写 `lo=0xFFFFFFFF` 会让 hi 莫名 +1 ⇒ 违反 D3 |
| **S7** 新增分离的 `mtimeh` / `mtimel` 端口（原子读） | **否决** | 现有 `read_data` + 地址偏移**已**提供两个 32 位读；新增端口 ⇒ 实例化点 MUST 驱动 + 契约面扩大 ⇒ 而撕裂是 RISC-V **允许**的 ⇒ 无收益（见 D5） |
| **S8** 把 CLINT 语义塞进既有 capability（`rv32i-canonical-home`） | **否决** | CLINT 是**外设**，与流水线微架构无共享语义 ⇒ 塞进去会让该 capability 的边界失焦（ADR-046 的归属原则）⇒ 见 D7 |
| **S9** 递增放在 **C++ 侧**（恢复 `tick()` 方法） | **否决** | C++ 侧方法**不在**仿真时序里（`describe()` 是建图期）⇒ 上一版被删的 `tick()` 就是**空实现**且注释还声称「Simulator 每 tick 调用」—— 事实上**零调用方** ⇒ 递增 MUST 由 RTL（`ch_reg` 的 `next`）实现 |

## 4. Consequences

### 正面

- CLINT 的名字**兑现**它承诺的语义（`mtime` 真的是定时器）⇒ 软件时基 / 定时器中断**可表达**；
- 「契约变更」有了**归属**（D7）⇒ CLINT 从「不在任何 capability 覆盖内」变成**有契约的外设** ⇒ 后续改动有单一真相；
- 爆炸半径**极小**：实例化点 2 处、消费者 1 个文件 ⇒ 实施风险低；
- 两个实现陷阱（§1.4）被**实测**钉进 ADR ⇒ 实施期不必重新踩（`carry ? :` 的建图期求值、`<<` 在 64 位上编译不过）。

### 负面 / 代价

- **`mtime` 从「恒定」变成「会动」** ⇒ 任何依赖 `mtime` 稳定的断言都会变红 ⇒ `test_interrupt_flow` 的期望值**必须重新推导**；
- ⚠️ **既有测试会变成「时序相关」的脆弱测试**：`test_interrupt_flow` 写 `mtime=50` 后只 tick 1 拍就断言 `mtime < mtimecmp(100)` ⇒ 在新语义下 `mtime` 已是 51 ⇒ **靠 49 的余量才通过** ⇒ ⇒ 实施 MUST 把该用例改成「**每次采样前重写 `mtime`**」或把比较值设得足够远（否则它测的是「递增后的 `mtime`」而非「写值」）；
- 新增 capability ⇒ 主 spec 从 **28** 个增至 **29** 个 ⇒ 归档 MUST 带 delta（**禁** `--skip-specs`）；
- D2 的分频器多一个 `ch_reg` + 比较（默认 `TIMER_DIVIDER=1` ⇒ 逻辑上等价于每拍，但**节点**仍在）。

### 风险

- ⚠️ **进位 / 64 位拼接的静默错误**（§1.4 实测两条）⇒ ⇒ MUST 有**判别性**的跨 32 位边界用例（初值 `0xFFFFFFFF` ⇒ 一拍后 `hi` MUST 变 1）+ 变异（把 `select` 换回 C++ 三元 ⇒ 必转红）；
- ⚠️ **分频相位**若实现成「写也复位分频器」⇒ 期望值不可推导 ⇒ MUST 有「写后相位不变」的用例；
- 若实施者选了形态 A 而漏了 `!write` 门控 ⇒ 写 `lo=0xFFFFFFFF` 那拍 `hi` 会莫名 +1 ⇒ 同一条跨边界用例可抓；
- `mtip` 的组合比较（`hi_gt | (hi_eq & lo_ge)`）**不变** ⇒ 但因 `mtime` 会动 ⇒ 复位后 `mtimecmp = 全 1` ⇒ 复位后**不**触发中断（这一条 MUST 有回归断言）。

### 不在本次范围

- **把 CLINT 接入 SoC**（`rv32i_pipeline_top.h` 目前**没有** CLINT 实例）⇒ 独立立项；
- **64 位原子读 / 撕裂检测**（D5）⇒ 已知边界；
- **`mtimecmp` 的语义变更**（保持 R/W、复位全 1）⇒ 不变；
- `CLINT` 的**命名空间**（`ch::chlib`）⇒ 不在本 ADR（ADR-033 的命名空间治理另议）。

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 递增语义（D1 / D2）

- [ ] `TIMER_DIVIDER = 1` 时，连续 N 拍后 `mtime` 逐拍 **+1**（期望值**手推**，禁抄仿真输出）；
- [ ] 跨 32 位边界：初值 `mtime = 0x00000000_FFFFFFFF` ⇒ **一拍**后 MUST 为 `0x00000001_00000000`（**判别性**用例）；
- [ ] `TIMER_DIVIDER = N > 1` 时，**第 N 拍**发生第一次递增、其后每 N 拍一次（D2 相位）；
- [ ] 分频相位**不**被软件写复位（D2）；
- [ ] `TIMER_DIVIDER = N > 1` 时，写拍**恰落在递增边界** ⇒ 该次递增**被丢弃**（写优先）且分频相位**不变** ⇒ ⇒ **判别「写拍复位分频器」与「写拍暂停分频器」两种实现**（两者都满足「不复位」的字面，但产生不同的可观测行为）。

### 5.2 写优先（D3）

- [ ] 写 `mtime` 的那一拍：被写半 = 写值、另一半 = 原值、**且不递增**；
- [ ] 写后的**下一拍**：从写值继续递增（写 50 ⇒ 下一拍 51）；
- [ ] 只写低半 ⇒ 高半**保持**（⇒ 跨边界用例：高半为 5 时写 `lo=0xFFFFFFFF` ⇒ 下一拍 MUST 为 `hi=6, lo=0`）。

### 5.3 复位与比较（D4 / `mtimecmp` 不变）

- [ ] 复位后 `mtime = 0`、`mtimecmp = 全 1` ⇒ `mtip` MUST 为 0；
- [ ] `mtime >= mtimecmp` ⇒ `mtip` 为 1（既有语义**逐项不变**）；
- [ ] `test_interrupt_flow` 的期望值**重新推导**并仍绿；⚠️ 该用例 MUST **每次采样前重写 `mtime`**（否则变成时序相关的脆弱测试 —— §4「负面 / 代价」）。

### 5.4 变异验证（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）

- [ ] M1 把进位的 `select(carry, 1, 0)` 换回 **C++ 三元** ⇒ 跨边界用例**转红**（§1.4 陷阱 1）；
- [ ] M2 **（仅形态 B）** 形态 B 的 `bits_update<32>` 换成手写 `hi | lo`（**漏移位**）⇒ 写路径用例**转红**（§1.4 陷阱 2）；（形态 A 的对应陷阱由 M1 覆盖 ⇒ 选 A 时 M2 不适用）
- [ ] M3 去掉 D3 的「写那一拍不递增」⇒ 写后递增用例**转红**；
- [ ] M4 分频器改成「每拍递增」（忽略 `TIMER_DIVIDER`）⇒ 5.1 的分频用例**转红**；
- [ ] M5 写路径**复位**分频器 ⇒ 「写后相位不变」用例**转红**；
- [ ] M6 把 `mtime` 的递增源整体删掉（回到「静态比较器」）⇒ 5.1 的递增用例**转红**（⇒ 证明该用例**真的**在测递增）。

### 5.5 回归与计数

- [ ] `ctest -LE perf` 零回归；`test_interrupt_flow` 绿；
- [ ] **计数锚点不变**（新用例进**既有** target ⇒ `CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** / `verilator_e2e` **38** / non-perf **191**）；
- [ ] `include/cpu/AGENTS.md` 与 `clint.h` 的文件头注释**与实际行为一致**（不再有「声称」与「实现」不符）。

## 6. 实施追踪

- **change**：**待立项**（planner 建 change，带 `cpu-clint-timer` 的 ADDED delta ⇒ 归档**禁** `--skip-specs`）⇒ 其 `design.md` 承载逐拍推导 / 变异清单 / 探针证据；`tasks.md` 承载可勾选任务（**过程日志不进本 ADR**）；
- **改动面（预期）**：`include/cpu/riscv/clint.h`（+ `examples/riscv-mini/tests/test_interrupt_flow.cpp` 的期望值重推 + `include/cpu/AGENTS.md` + capability delta）；
- **爆炸半径**：**极小**（实例化点 2 处 / 消费者 1 个文件 ⇒ 未接入 SoC）；
- **回滚路径**：**revert 实施 commit** 即可 ⇒ 旧语义 = §1.2（「静态比较器」）⇒ **无迁移负担**（无持久化格式、无公开 API 冻结、无数据兼容面）。
- **归属**：`.rddf/improvements/clint-mtime-autoincrement-2026-10.md`（本 ADR 是它的**强前置** ⇒ 裁决后它才可立项）。

## 7. 参考与审查记录

- **依据**：`include/cpu/riscv/clint.h`（现状：保持 + 可写）· `examples/riscv-mini/tests/test_interrupt_flow.cpp`（唯一消费者）· RISC-V privileged spec 的 `mtime` / `mtimecmp` 语义 · `.rddf/improvements/clint-mtime-autoincrement-2026-10.md` §6（决策门 D1–D5 的**倾向**记录）· 根 `AGENTS.md` §ANTI-PATTERNS（`ch_bool` 的建图期求值 / 未驱动端口）· `include/core/operators_width.h`（`bits` / `bits_update` / `concat`）· `include/core/operators_shift.h`（64 位左移的实现限制）。
- **实测探针**（`/tmp/opencode/probe_clint.cpp` / `probe_clint2.cpp`）：形态 A / B 的语义等价性 + §1.4 的两条陷阱 + 「写优先 / 跨边界」的正确序列（`1/0 → 1/1 → 1/50 → 1/51`）。
- **对抗审稿（critic，`review_profile: arch`，2026-10-30，第 1 轮）**：**APPROVE_WITH_CONDITIONS**（**0 BLOCKER** / `mergeable: yes` / `scope_exceeded: false`）⇒ 2 SHOULD_FIX + 4 NIT 同批闭合：SF1 = **写拍的分频计数器行为未钉死**（「不复位」与「暂停」两种实现都满足字面 ⇒ 产生不同可观测行为）⇒ D2 补「**照常推进 + 该次递增被丢弃**」+ §5.1 补判别性用例；SF2 = **缺回滚路径** ⇒ §6 补「revert 实施 commit / 旧语义 = §1.2 / 无迁移负担」；NIT = capability 数量订正（17→**28**）/ capability 名去自反（`rv32i-` → **`cpu-`**）/ M2 标注「仅形态 B」/ §7 回填本记录。⇒ ⚠️ 审稿未能复核 §1.4 两条陷阱的**实测真实性**（探针在仓外临时目录）⇒ 其判别力由实施期的 M1 / M2 变异回填证明。
