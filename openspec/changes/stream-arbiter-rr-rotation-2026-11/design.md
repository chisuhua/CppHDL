# Design: stream-arbiter-rr-rotation-2026-11

## 1. 实测记录（**探针输出原文照录**，2026-11-01）

### 1.1 缺陷确认：`stream_arbiter_round_robin` 从不轮转

`/tmp/opencode/p2/probe_rr.cpp`（`build/libcpphdl.a` 链接，`N=4`）：

```
== single_valid (N=4, valid=0010) ==      ← 既有测试的口径
  t  selected  valid
  0     2        1
  ...（8 拍恒为 2）

== two_valid (N=4, valid=1010) ==         ← 通道 0 与 2 持续有效
  t  selected  valid
  0     2        1
  1     2        1
  ...
  7     2        1                        ← 🔴 **恒为 2** ⇒ 通道 0 饿死

== all_valid (N=4, valid=1111) ==
  t  selected  valid
  0     1        1
  ...（8 拍恒为 1）                        ← 🔴 通道 0/2/3 全饿死
```

### 1.2 根因定位：选择器正确，调用方错

`/tmp/opencode/p2/probe_selector.cpp`（`request = 0b1111`，逐项给不同 `last_grant`）：

```
  last_grant=none  ⇒ grant_idx=1 (期望 1)  ✔
  last_grant=idx0  ⇒ grant_idx=1 (期望 1)  ✔
  last_grant=idx1  ⇒ grant_idx=2 (期望 2)  ✔
  last_grant=idx2  ⇒ grant_idx=3 (期望 3)  ✔
  last_grant=idx3  ⇒ grant_idx=0 (期望 0)  ✔
```

⇒ ⇒ **`round_robin_selector` 逐项正确** ⇒ ⇒ 缺陷在
`stream_arbiter_round_robin` **恒传 `last_grant = 0_d`**。

⚠️ **探针自身踩的坑（第三次复现同一元教训）**：初版探针在 `Simulator`
构造**之后**才建 `priority_encoder` 节点 ⇒ 报
`[WARN] Value not found for signal node ID: 519`（节点不在 `data_map_`）
⇒ ⇒ 改为「`Simulator` 前算好 one-hot、C++ 侧解码」后结论稳定
⇒ ⇒ ⇒ **「探针输出的解读也必须被独立复核」再次生效**。

### 1.3 既有覆盖为何是零

`tests/chlib/test_stream.cpp` 的 arbiter 段只测**唯一有效输入**
⇒ 该场景下 round-robin 与 priority **逐位相同** ⇒ ⇒ **零判别力**；
`N=2` 的 SECTION 只有 `width` 断言（注释明写
"avoid relying on current round-robin selector behavior"）。

## 2. 修法设计

### 2.1 形态：反馈寄存器（one-hot）

```cpp
ch_reg<ch_uint<N_INPUTS>> last_grant_reg(0_d, "rr_last_grant");
...
auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, last_grant_reg);
...
last_grant_reg->next = select(arb_result.valid, arb_result.grant, <保持>);
```

**三条设计决定，逐条给理由**：

| 决定 | 理由 |
|---|---|
| **反馈的是 one-hot 而非 binary 索引** | `round_robin_selector` 的 `last_grant` 形参**就是 one-hot**（它内部 `onehot_to_binary` + `has_last_grant = last_grant != 0`）⇒ 直接喂 `grant` **零转换、零新节点** |
| **更新条件 = `arb_result.valid`**（无授予则保持） | 若每拍都更新，则「没有有效输入的拍」会把指针推到 `grant == 0` ⇒ ⇒ 下一拍 `has_last_grant = false` ⇒ start_pos 回到 1 ⇒ ⇒ **轮转相位被静默重置** |
| **删掉死节点 `current_channel`** | Zero-Debt「No dead code」；且它是 **binary 索引**（与 one-hot 反馈**形态不一致**）⇒ ⇒ 留着会诱导后人写成第二种反馈形态 |

### 2.2 不成环的论证

`last_grant_reg` 是 **`ch_reg`** ⇒ ⇒ 反馈路径为
`last_grant_reg(寄存器输出) → round_robin_selector(组合) → arb_result.grant → last_grant_reg->next(寄存器输入)`
⇒ ⇒ **寄存器隔断组合环** ✔

⚠️ 本仓的组合环事故（`bf78367`，见根 `AGENTS.md`）形态是「数据源的**叶子**
取 EX 级的**组合输出**」⇒ ⇒ 本修法的叶子是**寄存器** ⇒ 不属于该形态。

### 2.3 ⚠️ 首拍相位与「组合输出在 posedge 后重新求值」（**实测裁决**）

⚠️⚠️ **初值 MUST 是 `0_d`**，理由**两条**：

1. **`ch_reg<T>` 的非零初值在 Verilog codegen 侧失效**（实施期实测，
   见 `include/chlib/stream.h` 的注释）⇒ ⇒ 若非零初值 ⇒ ⇒
   **interpreter / Verilog 双后端分歧** ⇒ ⇒ ⇒ **⇒ 零初值**；
2. 零初值 ⇒ `has_last_grant = false` ⇒ `last_grant_idx = 0`
   ⇒ `start_pos = 1`（**这是选择器的既有语义，本 change 不改它**）。

⚠️⚠️⚠️ **关键实测发现（推翻了我自己的第一版手推）**：
`selected` 是**纯组合输出**（`priority_encoder(arb_result.grant)`，无寄存器）
⇒ ⇒ **posedge 之后 `last_grant_reg` 已推进 ⇒ 组合重新求值**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ `tick()` 之后读到的 `selected` 用的是【已推进】的相位**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 手推金标 MUST 按「posedge 后稳态」递推**（我在 design
初稿按「posedge 前」推，**差一拍**，被探针当场打脸）。

**逐拍递推（`N=4`，读数 = 每拍 `tick()` 后的稳态）**：

`all_valid`（四输入全有效）：

| 拍 | 拍初 `lg` | `start_pos` | grant | 拍末 `lg` = `next` | **`tick()` 后读到的 `selected`** |
|---|---|---|---|---|---|
| tick1 | 0 | 1 | 1 | `1<<1`(idx1) | **2**（lg=idx1 ⇒ start=2） |
| tick2 | idx1 | 2 | 2 | `1<<2`(idx2) | **3** |
| tick3 | idx2 | 3 | 3 | `1<<3`(idx3) | **0** |
| tick4 | idx3 | 0 | 0 | `1<<0`(idx0) | **1** |
| tick5 | idx0 | 1 | 1 | `1<<1` | **2** |

⇒ **实测序列 `2,3,0,1,2,3,0,1`** ✔（周期 4，**覆盖全部通道**）

`two_valid`（仅 0 与 2 有效）：

| 拍 | 拍初 `lg` | `start_pos` | 扫描 1→2→3→0 首个有效 | grant | 拍末 `lg` | **`tick()` 后读到的** |
|---|---|---|---|---|---|---|
| tick1 | 0 | 1 | 2 | 2 | `1<<2`(idx2) | **0**（lg=idx2 ⇒ start=3 ⇒ 3 无效、0 有效） |
| tick2 | idx2 | 3 | 0 | 0 | `1<<0`(idx0) | **2** |
| tick3 | idx0 | 1 | 2 | 2 | `1<<2` | **0** |

⇒ **实测序列 `0,2,0,2,0,2,0,2`** ✔（**交替** ⇒ 通道 0 不再饿死）

⚠️ **「首拍读到几」不是固定值** —— 它取决于 `valid_vector`
（`all_valid` 首拍读 2、`two_valid` 首拍读 0）⇒ ⇒ ⇒
**⇒ 契约 MUST NOT 锁死首拍字面量**（否则是假陈述）
⇒ ⇒ ⇒ **契约只写效果判据**（「持续有效的多个通道 MUST 在有限拍内全部被服务」），
**判别力用例**才逐拍断言（钉住具体相位、防回归）。

## 3. 判别力用例设计（**金标 = 逐拍递推 + 实测对照**）

⚠️ 金标**不是**抄仿真输出：§2.3 已按「posedge 后稳态」**逐拍手工递推**
（表在 §2.3），递推结果与探针实测**逐项一致** ⇒ ⇒ **两份独立来源互证** ✔

### 3.1 用例 A：`two_valid` ⇒ **交替**（钉住「通道 0 不再饿死」）

场景：`N=4`，输入 **0 与 2 持续有效**（1、3 无效），`output.ready` 恒 1。

**金标（6 拍，逐拍）**：`0, 2, 0, 2, 0, 2`

⇒ ⇒ 修复前该序列是 **`2, 2, 2, 2, 2, 2`** ⇒ ⇒ **判别力成立**。

### 3.2 用例 B：`all_valid` ⇒ **周期 4，覆盖全部通道**（钉住「真轮转」）

场景：`N=4`，四个输入全有效，`output.ready` 恒 1。

**金标（8 拍，逐拍）**：`2, 3, 0, 1, 2, 3, 0, 1`

⇒ ⇒ 修复前是 **`1, 1, 1, 1, 1, 1, 1, 1`** ⇒ ⇒ **判别力成立**。

⚠️ **为什么两个用例都要**：用例 A 只覆盖 {0,2} 两个通道 ⇒ 若实现「指针只在
0/2 之间跳」它**仍会绿**；用例 B 覆盖四个通道且**连续递增** ⇒
⇒ ⇒ ⇒ **两者互补**，缺 A 抓不到「交替」退化，缺 B 抓不到「只跳有效通道」。

### 3.3 用例 C：唯一有效输入 ⇒ 必须被选中（**既有语义的回归锁**）

场景：`N=4`，仅输入 2 有效。**金标**：`selected == 2`（每拍）。

⇒ ⇒ 这条**既有测试已覆盖**（`tests/chlib/test_stream.cpp` 的
「选中唯一有效的输入」）⇒ ⇒ **本 change 不重复添加**，但 MUST 保证它**仍绿**。

### 3.4 反向判据（钉住「轮转真的发生了」）

把反馈断回常量 `0_d`（= **修复前的形态**）⇒ ⇒ 用例 A / B 的断言 MUST **转红**。

⇒ ⇒ 由**护栏自检的原地投毒**实现（见 §5）⇒ ⇒
⚠️ **它是纯行为判据** ⇒ ⇒ 自检的「静态判据也必须红」对**不适用**
（同 P1-a 的 `drop-multidrive-guard` 类）⇒ ⇒ ⇒ **MUST 归入「仅行为」类别**。

## 4. 实施记录（2026-11-01）

### 4.1 实际改动（`git show --name-only 57ec89a` 核对）

| 文件 | 改动 |
|---|---|
| `include/chlib/stream.h` | `stream_arbiter_round_robin`：删 `current_channel` 死节点 + 加 `last_grant_reg` + 反馈 + `valid` 条件更新 |
| `include/chlib/stream_arbiter.h` | `stream_arbiter_sequence` 的头注释与结果 struct 注释（**零行为变更**） |
| `tests/chlib/test_stream.cpp` | **新增 TEST_CASE** `[stream][arbiter][round_robin][rotation]`（3 SECTION / **38 断言**） |
| `scripts/lib/stream_ready_ownership.py` | 投毒 `rr-feedback-const` |
| `scripts/verify_stream_ready_ownership.sh` | 自检加该投毒 + 归入「仅行为判据」类别 |
| `openspec/changes/.../` | proposal / design / tasks / specs（**新 capability**） |
| `.rddf/improvements/stream-arbiter-rr-rotation-2026-10.md` | 台账标注已实施 + 新发现（sequence 与 priority 重复） |

### 4.2 实施期被推翻的推断（⚠️ 记下来防复发）

| # | 我的推断 | 实测结果 |
|---|---|---|
| ① | 手推金标 `2,0,2,0,...`（按 posedge **前**的 `last_grant`） | ❌ 实际是 `0,2,0,2,...` ⇒ ⇒ 因为 `selected` 是**纯组合输出** ⇒ posedge 后重新求值用**新相位** |
| ② | 探针可以直接读 `priority_encoder(grant)` | ❌ `Value not found for signal node` ⇒ 节点建在 `Simulator` 构造**之后**（不在 `data_map_`）⇒ 改「Simulator 前算 one-hot、C++ 侧解码」 |
| ③ | Python 脚本首行可以写 `//` 注释 | ❌ `SyntaxError` ⇒ 当场失败、未写盘（纪律救回） |

⇒ ⇒ ⇒ **⇒ 第 ①② 两条都是「探针/金标自身的缺陷」，不是实现缺陷**
⇒ ⇒ ⇒ ⇒ **⇒ 「探针输出的解读必须被独立复核」第三次生效**。

### 4.3 既有调用点复核（**零破坏**，逐项）

| 调用点 | 断言 | 轮转后 |
|---|---|---|
| `examples/stream/stream_arbiter_demo.cpp` | **无断言**（只打印） | ✔ 不变 |
| `samples/stream_arbiter_example.cpp` | `selected ∈ {0,2}`（有效即可） | ✔ 仍满足 |
| `samples/spinalhdl_stream_example.cpp` | `selected` 仅传递，无断言 | ✔ 不变 |
| `tests/chlib/test_stream.cpp:731`（唯一有效输入 2） | `sel == 2` | ✔ 唯一有效者必被选中 |
| `tests/chlib/test_stream.cpp:820`（`N=2`） | **只做 `width` 断言** | ✔ 不变 |

### 4.4 验收实测

```
ctest -LE perf            => 100% tests passed, 0 failed out of 194
scripts/verify_*.sh       => FAIL=0（全部）
--self-test               => 9 种投毒全部被抓住 + 还原后复绿
run_all_samples.sh        => PASS: 39  FAIL: 0
run_all_ported_tests.sh   => PASS: 28  FAIL: 0
verify_openspec_delta.sh  => 4 条全绿
openspec validate --specs --strict => 21 失败 = 基线（零新增）
ctest -N                  => 205 / 194 / 38（锚点不变）
```

## 5. 护栏

⚠️ **是否需要新建 `scripts/verify_*.sh`？**

**结论：不需要**，理由：本 change 的判据是**纯行为**的（逐拍授予序列），
且已有 `scripts/verify_stream_ready_ownership.sh` 的
**原地投毒自检框架**（`--self-test`）承载「变异是否被抓」的证明
⇒ ⇒ 把本 change 的变异（反馈断回 `0_d`）**加入该护栏的投毒清单**
⇒ ⇒ 免费获得「先确认编译成功 + 还原后复绿 + 静态/行为双判据」的全套纪律
⇒ ⇒ ⇒ **MUST NOT** 再造一个只有 1 条判据的新脚本
（本仓病症「护栏自身有盲区」的反面：**护栏碎片化**同样会漏）。

⚠️ 但该护栏的 `STREAM_H` 投毒**只覆盖 `stream.h`** ⇒ ⇒ 本 change 的变异也落在
`stream.h` ⇒ ⇒ **可直接复用**（`POISON_TARGETS` 的 sidecar 机制已在 P1-a 建好）。

## 6. 退化分析与风险

| 面 | 分析 |
|---|---|
| **既有断言** | 已逐个核对（proposal §Impact）⇒ ⇒ **零破坏** |
| **中央死节点** | `current_channel` 删除 ⇒ ⇒ 节点数**减少**（不影响任何输出） |
| **生成 Verilog** | 新增 **1 个寄存器**（`rr_last_grant`）⇒ ⇒ 顶层端口**不变**（`ch_reg` 是内部状态） |
| **资源** | `+1` 个 `compute_idx_width`… 实际是 `ch_uint<N_INPUTS>`（one-hot）⇒ `N` 个触发器 |
| **双后端** | 寄存器 + 组合选择 ⇒ 两后端同构；⚠️ **不做** parity 测试（属 P3，独立立项） |
| **时序语义** | 反馈经 posedge ⇒ 授予决策**组合**依赖 `last_grant_reg` 的当前值 ⇒ 同一拍内稳定 ⇒ 不引入新延迟 |
