# ADR-063: RV32I B 型立即数的 bit 11 —— 裁决修复范围、重复实现的处理与 golden 翻转策略

- Status: **Accepted**（2026-11-03，用户授权「切 architect 裁 ADR → 回到 builder 修」）· Date: 2026-11-03 · Author: build-agent（architect role）
- Parent: **ADR-046**（RV32I 单源化 —— `include/cpu/` 是微架构唯一权威）· **ADR-035**
  （Verilator 后端 —— 本缺陷在**两后端同构**，故 parity 结构性失明）
- 关联 improvement: `.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
- 关联 capability: `rv32i-isa-semantics`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs` ·
  关联 change: 待建（实施本 ADR）
- 前置证据：路线 C 第 1 / 2 批的 dual 文件（`test_rv32i_stage_dual.cpp:204` ·
  `test_rv32i_timing_dual.cpp:118`）**当前钉住的就是缺陷形态** ⇒ D6 处理它们

---

## 1. Context

### 1.1 触发

路线 C 第 1 批把 `riscv::Rv32iDecoder` 当 dual DUT，用**独立 Python 模型 + 规范编码器**
手推期望值 ⇒ 实测不符 ⇒ 探针确认。该批变异 M5（「把 B 型修好」）**转红** ⇒ 反证了钉点
的判别力。

### 1.2 缺陷的精确定义

RISC-V 的 B 型立即数布局：

```
imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]
```

⇒ `instr[7]` 承载 **`imm[11]`**。**两处实现都把 `instr[7]` 直接 OR 到立即数的 bit 0**
（漏了 `<< 11`）：

```cpp
// rv32i_decoder.h:113 与 stages/id_stage.h:145（生产路径，同款笔误）
bits<7, 7>(io().instruction);          // 🔴 应为 << 11（注释写的是 imm[11]）
```

**三组实测数值**（手算 + 探针一致）：

| 指令 | 编码 | 规范值 | 现实现 | 误差 |
|---|---|---|---|---|
| `beq x1,x2,-8` | `0xFE208CE3` | `0xFFFFFFF8` | `0xFFFFF7F9` | **2047** |
| `beq x1,x2,+2048` | `0x002080E3` | `0x00000800` | `0x00000001` | **2047** |
| `beq x1,x2,+8` | `0x00208463` | `0x00000008` | `0x00000008` | 0 |

第三行解释了**缺陷为何长期未暴露**：`|偏移| < 2048` ⇒ `imm[11] = 0` ⇒ 误置的
`bit 0` 也是 0 ⇒ **无误差**。仓内**所有**固件（含 SoC parity 固件
`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:447,452,594,595`）的分支偏移
都是 `+8` ⇒ 历史 golden **全部不受影响**。

### 1.3 为什么两后端 parity 看不见它

`Rv32iDecoder` / `IdStage` 是**纯组合** ⇒ 同一份 C++ 描述生成 interpreter 求值树与
Verilog ⇒ **两后端一致地错** ⇒ parity 恒绿。同型先例：ADR-047 的「距离 3 读旧值」
⇒ ⇒ **parity 不是正确性证据**，唯一能抓它的是**手推期望值**。

### 1.4 两份重复实现**已出现风格漂移**（D4 的依据）

| 类型 | `rv32i_decoder.h` 移位量容器 | `stages/id_stage.h` 移位量容器 | 等价？ |
|---|---|---|---|
| S / J / U | `ch_uint<7>/<20>/<10>/<11>/<12>` 等 | `ch_uint<3>/<5>/<1>/<4>` 等 | ✅（值相同） |
| B 型 | `ch_uint<12>/<6>/<4>` | `ch_uint<5>/<3>/<1>` | ✅ 除 `bit 11`（**两处都缺**） |

⇒ **除 B 型外逐位等价**，但同一段逻辑被复制成两份、容器写法已分叉 ⇒ 上一轮 U 型缺陷
修时**只改了一份**（decoder 恰好本来就对）⇒ 未暴露代价；本次 B 型两处**同时**错，
是**复制粘贴传播**的直接证据。


### 1.5 归属缺口：capability **没有** B 型条款

`rv32i-isa-semantics` 有 I 型 / U 型 / LB·LH / JAL·JALR 条款，但 `grep "B 型"` 在**任何**
capability 里**零命中** ⇒ 修好后仍**无契约载体** ⇒ 可无声回归 ⇒ 必须 ADDED（D7）。

---

## 2. Decision

### D1. 修（不冻结现状）

缺陷是**笔误**（注释与代码自相矛盾），不是设计选择；且它使
`|偏移| ≥ 2048` 的条件分支**跳到错误地址**（误差恒 **2047**）⇒ 属生产代码真 bug
⇒ **MUST 修**。不存在「依赖该错误行为」的下游（§1.2 已实测：所有固件偏移 = `+8`）。

### D2. 两处**原地各修一行**，**不做**立即数提取的收敛

`rv32i_decoder.h:113` 与 `stages/id_stage.h:145` **各自**补 `<< 11`。**MUST NOT** 在本
change 把 `IdStage` 改为调用 `Rv32iDecoder` 的逻辑 ⇒ 见 §3 选项 C。

### D3. 修法（具体表达式）

```cpp
(bits<7, 7>(io().instruction) << ch_uint<4>(11_d))    // imm[11]
```

- 移位量容器 `ch_uint<4>` 的**值域 0..15 ⊇ 11** ⇒ 与既有「刚好够」风格一致
  （同文件已有 `<< ch_uint<5>(12_d)` 等）；
- ⚠️ **MUST NOT** 用 C++ 整数直接移位（`<< 11`）—— 框架要求 `ch_uint<N>` 移位量，
  且裸整数会绕过宽度推导；
- ⚠️ **MUST** 保持 `ch_uint<13>` 容器（`imm[12:0]`）与随后的 `sext<32>` 不变。

### D4. **MUST** 补一条「两份实现等价性」判据（防只改一处复发）

这是本 ADR 对 D2 的**代价补偿**，不是可选项：

> 存在一条**可执行**判据，对**同一组指令**断言 `Rv32iDecoder::imm == IdStage::imm`；
> 指令集 **MUST** 覆盖 B 型 `imm[11]=1`（正、负各一）· I/S/U/J 各 ≥1 · `|imm|<2048` 的 B 型。

实现形式由实施者选：① wrapper 组件同时实例化两者并比较（运行期，最强）；② 两份测试
用**同一张指令表 + 同一套期望值**。⇒ **推荐 ①**（`test_rv32i_stage_dual.cpp` 新增 pair）。

### D5. golden 变更清单（**精确，共 3 处**）

| 文件:行 | 现值 | 应改为 | 说明 |
|---|---|---|---|
| `tests/test_rv32i_stage_dual.cpp:204` | `0xFFFFF7F9` | `0xFFFFFFF8` | `beq x1,x2,-8` |
| `tests/test_rv32i_stage_dual.cpp`（`beq x1,x2,+2048` 行） | `0x00000001` | `0x00000800` | `imm[11]=1` 边界 |
| `tests/test_rv32i_timing_dual.cpp:118` | `0xFFFFF7F9` | `0xFFFFFFF8` | `beq x1,x2,-8`（IdStage） |

**不变**（必须显式验证，不是「大概不变」）：`test_rv32i_soc_dual_parity.cpp` 全部
golden（固件偏移全 `+8`）· `test_rv32i_pipeline_behavior.cpp:573`（B 型立即数 = 0）·
**3 个 example**（`examples/riscv-mini/rv32i_phase{1,2,3}_test.cpp` 均读
`decoder.imm`，但断言的 B 型用例 imm 全为 0、S 型为 256 ⇒ 不受影响）· `ctest -LE perf`
的 227 个测试。

⚠️ 若上述「不变」项**有任何一处**真的变了 ⇒ 说明对影响面的判断有误 ⇒
**停止并重新裁决**（不要顺手改 golden）。

### D6. 变异方向**必须反转**（钉正确行为，不钉缺陷）

现状：`tests/test_rv32i_stage_dual.cpp` / `_timing_dual.cpp` 的期望值钉的是**缺陷形态**
⇒ 修复后这两个文件**会转红**（**有意的变更信号**）。修复完成后：

- 把「M5 = 把 B 型立即数修好 ⇒ 转红」**替换**为
  「**把 `<< 11` 去掉（回退到缺陷形态）⇒ 转红**」；
- ⇒ 只有这样，测试才是在**守护正确行为**，而不是在**守护缺陷**。

⚠️ **新期望值 MUST 由 RISC-V 规范独立推导**（如 §1.2 的手算表），
**MUST NOT** 以 `sim.get_port_value(io.imm)` 的输出为准 —— 抄仿真输出是把期望值
交给被测实现，D6 的反转也就失去意义。

### D7. capability 归属：`rv32i-isa-semantics` **ADDED ×1**

新增 Requirement：**B 型立即数 MUST 按 `imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]`
提取并符号扩展**，含 ≥3 个 Scenario（`imm[11]=1` 正偏移 2048 / 负偏移 -8 /
`imm[11]=0` 的负向对照）。

⚠️ 该文件**已有** I 型 / U 型 / JAL 条款 ⇒ 本 ADR 是**补齐同类条款**，不是新建
capability。⚠️ 归档**禁** `--skip-specs`。

### D8. 明确**不在**本次范围（scope 边界）

| 项 | 为什么不一起做 |
|---|---|
| 把所有移位量容器统一成"冗余宽度"（如一律 `ch_uint<32>`） | 无行为收益、改 RTL 结构、与本缺陷无因果关系 ⇒ 记 backlog |
| 把两份立即数实现**收敛成一份** | 会改组件层次 / 节点名 ⇒ 需重核全部 golden ⇒ 是**独立重构**（ADR-046 的续篇） |
| `stages/id_stage.h` **非自包含**（该 include 不是死的 —— 它为 `OP_*` 常量，只是路径在上一级） | 零行为影响，已独立立项 ⇒ 混入会让 commit 语义不清 |
| `ex_stage.h` 的 `BRANCH_*` 常量与 funct3 相反 | 零消费者、零行为影响，已独立立项 |

---

## 3. 选项对比

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| A | **两处原地各修 1 行**，另加 D4 等价性判据 | ✅ **选中** | 改动面 = 2 行 + 3 处 golden；零 RTL 结构变化；爆炸半径已被 §1.2 实测限定 |
| B | 只修生产路径 `id_stage.h`，`decoder` 保持 | ❌ 否决 | `Rv32iDecoder` 有 **3 个 example 消费者**（`examples/riscv-mini/rv32i_phase{1,2,3}_test.cpp`）⇒ 留着就是给下游递 bug；且**两份实现不一致**本身是更坏的状态 |
| C | 修 + 把立即数提取**收敛成单一实现** | ❌ 否决（本 change） | 收敛要改组件层次/节点名 ⇒ 全部 golden 与 Verilog 缓存 key 变化 ⇒ 成本远超「修笔误」；且 ADR-046 的单源化范围是**文件归属**，不是**逻辑内联**，需要独立裁决 |
| D | 不修，冻结现状 + 文档化 | ❌ 否决 | 生产路径真 bug：任何 `|偏移| ≥ 2048` 的条件分支跳错 2047 ⇒ 与实际使用面（编译器等下游）冲突 |
| E | 修 + 顺带统一所有移位量容器宽度 | ❌ 否决 | 无行为收益；「刚好够」的写法虽脆弱但**当前全部正确**（§1.4 逐项核过）⇒ 属风格收敛，不属缺陷修复 |
| F | 修 + 把立即数提取抽成**共享 inline 纯函数**（代码级去重，非组件级） | ⏸ **推迟**（不做于本次） | 这是比 C 更轻的根治路径：两处调用同一函数 ⇒ 表达式树相同 ⇒ 生成 Verilog 逻辑等价。**但仍需新增一个共享头 + 对 3 个代表性设计（`IdStage` / `Rv32iDecoder` / SoC）做 Verilog diff 验证门**（`test_verilog_gen` 只断言结构、不断言节点名 ⇒ 风险可控但非零）。本次目标是**消除 P1 生产 bug**，不是重构 ⇒ 与 `id-stage-header-not-self-contained-2026-11` 合并为独立议题 |

---

## 4. 后果与代价

### 正面

- 消除一个**生产路径**的真 bug（误差恒 2047，**静默**：无警告、parity 恒绿）；capability
  首次获得 B 型立即数的**契约载体**；D4 补上了「两份重复实现」的唯一已知护栏缺口。

### 负面 / 代价

- **两份重复实现仍在** ⇒ D4 是**补偿**不是根治；根治（选项 C）被显式推迟 ⇒ **新增
  一条技术债**（§6 有 backlog 指针）。
- golden 变更使 `test_rv32i_stage_dual.cpp` / `_timing_dual.cpp` **转红** ⇒ 不看 ADR
  直接改期望值会误以为「测试坏了」；D6 的变异反转是唯一防线。
- **测试数不变**（无新 ctest 条目）⇒ 若 D4 用形式 ②（同表同期望），判据强度依赖
  人工保持两张表同步。

### 风险

| 风险 | 缓解 |
|---|---|
| 误判影响面 ⇒ 悄悄改掉本该不变的 golden | D5 的「不变」清单是**可执行断言**：任一变 ⇒ 停止并重新裁决 |
| 只改一处（D2 的最大风险） | D4 判据 + 变异「只回退其中一处」必须转红（见 §5） |
| 修复引入移位宽度推导问题 | §5 要求 `beq x1,x2,+2048` 的 `0x00000800` 精确命中（该值只可能来自 `<< 11`） |

（不在本次范围 = D8 的 4 项）

---

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 语义与重复实现一致性（D1 / D3 / D4）

- [ ] 两处实现都对：`-8` ⇒ `0xFFFFFFF8` · `+2048` ⇒ `0x00000800` · `+8`（负向对照）⇒ `0x8`
- [ ] I / S / U / J 型的既有期望值**逐项不变**（§1.4 证明它们本来就对）

- [ ] 存在一条可执行判据：同一组指令上 `Rv32iDecoder::imm == IdStage::imm`
- [ ] 该判据的指令集覆盖 B 型 `imm[11]=1`（正/负）、I/S/U/J 各 ≥1、`|imm|<2048` 的 B 型

### 5.2 golden 影响面与变异验证（D5 / D6）

- [ ] `test_rv32i_stage_dual.cpp` 的 **2 处** + `test_rv32i_timing_dual.cpp` 的
      **1 处**期望值已改为规范值
- [ ] `test_rv32i_soc_dual_parity.cpp` 与 `test_rv32i_pipeline_behavior.cpp` **未变**
- [ ] `ctest -LE perf` 零回归（当前基线 227/227）


**变异验证**（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）：

| 变异 | 预期 |
|---|---|
| M-a 只回退 `rv32i_decoder.h` 的 `<< 11` | 🔴 必须转红（D4 判据抓） |
| M-b 只回退 `id_stage.h` 的 `<< 11` | 🔴 必须转红（D4 判据抓） |
| M-c 两处都回退（= 缺陷形态） | 🔴 必须转红（§5.1 抓） |
| M-d 把 `<< 11` 写成 `<< 12` | 🔴 必须转红 |
| M-e 把 `sext<32>` 换成 `zext` | 🔴 必须转红（负偏移） |

⚠️ **M-a / M-b 是本 ADR 的核心判据**（直接回答「D4 的补偿是否有效」）；若任一恒绿
⇒ D4 的实现形式太弱 ⇒ **MUST 改用形式 ①**。

### 5.3 契约与计数

- [ ] `rv32i-isa-semantics` 的 ADDED Requirement 已合并（禁 `--skip-specs`）· `openspec
      validate --strict` 通过（CLI 钉 1.4.1）
- [ ] 计数锚点同步（预期 `CTEST_TOTAL` **不变**：无新 ctest 条目）

---

## 6. 参考与审查记录

- ADR-046（单源化）· ADR-035（差异点）· ADR-047 D11（「两后端一致地错」的先例）
- `.rddf/checks/handoff-2026-11-02-parity-route-ab.md` §4.7（路线 C 候选池与判别形态）
- **backlog（D2 的代价）**：立即数提取「两份 → 单一实现」（选项 F）= 独立议题，建议与
  `id-stage-header-not-self-contained-2026-11` 合并（同在一条 include 边上）
- 审查记录：**build-agent 自查一轮**；critic 因 Token Plan 配额不可用（外部限制），
  **未过外部评审** ⇒ 实施前如有异议请指出
