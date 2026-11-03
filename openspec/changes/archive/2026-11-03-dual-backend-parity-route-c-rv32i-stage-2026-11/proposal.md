# 提案：路线 C 第 1 批 —— RV32I 微架构组件（`MemStage` / `ExStage` / `Rv32iDecoder`）进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
  + `.rddf/checks/handoff-2026-11-02-parity-route-ab.md` §4.7（路线 C 的只读盘点）
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（3 个 `DUAL_BACKEND_TEST_PAIR` / 460 断言每后端）+ 1 条 CMake 注册
  + 计数锚点 + **2 个新缺陷的独立立项**
- **关联**：objective `objective-test-coverage-100pct` · capability `dual-backend-test-coverage`
  · ADR-046（RV32I 单源化）

## Why

### ⭐ ① 这条路线本身是「全仓只读盘点」的产物 ⇒ ⇒ **阈值 60 从「不可达」变成「已达成」**

上一轮记录「实测可达上限 = 59 / 阈值 60 不可达」—— 但那个测量的**适用范围**是
「**chlib 语料 + 路线 A 候选清单**」这个**封闭语料**。把盘点范围扩到**全仓 `include/**`** 后
⇒ ⇒ 查出**第 6 类候选（路线 C）**：**13 个 RV32I 组件**在 58 个 macro dual 文件里
**各出现 0 次**（组件层零 dual 覆盖）⇒ ⇒ **本批落地第 60 个** ⇒ ⇒ **「阈值 60」达成**。

### ② 为什么这批**不是**「凑数」：这些组件是 RV32I 微架构的**单一权威**

ADR-046 把 `examples/riscv-mini/src/` 收敛进 `include/cpu/` ⇒ ⇒ **最近 3 个真缺陷全部落在这一层**：

| 缺陷 | 组件 | 修复 change | 当初怎么抓到的 |
|---|---|---|---|
| `LB` 符号扩展用 `0xFF000000`（只填 bit31..24） | `MemStage` | `rv32i-load-byte-lane-2026-10` | **SoC 级固件** |
| U 型立即数用 20 位掩码 + `alu_op` 的 SUB/SRA 误判 | `IdStage` | 同上 change 连带修复 | **SoC 级固件** |
| `strbe` 字节使能（新增输入端口 ⇒ 未驱动 = 全 1） | `DataTCM` | `rv32i-store-byte-enable-2026-10` | **SoC 级** |

⇒ ⇒ 这 3 个缺陷当初**都只被 SoC 级固件抓到** ⇒ ⇒ **组件级覆盖能把它前置**
⇒ ⇒ 这正是 objective §2「dual 覆盖是**缺陷发现机制**」的延伸。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 三个符号在全部 58 个 macro dual 文件里**各出现 0 次**；唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ ⇒ **判别形态不同**（整 SoC 固件轨迹 vs 单 stage 契约）⇒ ⇒ 逐案裁决通过 |
| ② | 断言形态有判别力吗？ | **有** —— 变异 **12/13 抓** + 1 条如实登记的判别力边界（见下） |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 三个组件**本身带顶层端口** ⇒ ⇒ **零 DUT 复制**（直接当 DUT）⇒ ⇒ 成本最低（与 `test_hazard_complete_dual.cpp` 同型） |

## 读模型（期望值的承重前提）

`Simulator::tick()`（`src/simulator.cpp:308-328`）= 边沿前组合求值 → 寄存器提交 → 边沿后组合求值
⇒ ⇒ **tick 后读到的端口 = f(边沿后的寄存器, 本拍输入)**。

⚠️ **本批三个组件全部是纯组合**（`ch_reg` 计数 = 0）⇒ ⇒ 读到的就是**本拍输入的纯函数**
⇒ ⇒ 期望值表**不需要时序建模**。
⚠️ **陷阱 T5 不适用**：三个组件**不发射任何 `always_ff`** ⇒ ⇒ 不存在「codegen 有时序逻辑但
Simulator 时序表为空」的漂移。

## 期望值（**手推**，禁抄仿真输出）

模型 = **独立 Python 实现**（`/tmp/opencode/gen_route_c_table.py`）逐条按实现推导 ⇒ ⇒ **一次通过**。

⭐ **译码器的指令字由 Python 里的「规范编码器」按 RISC-V 规范生成**
（`imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]` 等）⇒ ⇒ **期望值不是从实现反推的**
⇒ ⇒ 这正是「两后端一致 ⇒ 必是同一处推导错」判据能生效的前提。

### ⚠️ 实测到 2 处「我自己的表错」并当场修正（**第 4 次命中该判据**）

| # | 错误 | 诊断 | 修正 |
|---|---|---|---|
| ① | 译码器表的 `rs2` **手抄错 4 处** | 两后端一致给出正确值（`5` vs 我写的 `0`）⇒ ⇒ 错在**表** | 改**表**，并把三张表改成**机械生成**（Python 直接输出 C++ 初始化器）⇒ 杜绝手抄 |
| ② | `branch_type` 用了 `ex_stage.h` 的 `BRANCH_*` **常量块**（BLT=2） | 而**实现**用 RISC-V funct3（BLT=4）⇒ ⇒ 两后端一致 ⇒ ⇒ 错在**表** | 改**表**（并登记缺陷，见下） |

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 结果 |
|---|---|---|
| M1 | `MemStage`：LB 恒零扩展（去掉符号扩展分支） | 🔴 转红 |
| M2 | `MemStage`：字节道整条链恒取 lane0 | 🔴 转红 |
| M3 | `MemStage`：store 字节使能输出恒 0 | 🔴 转红 |
| M4 | `MemStage`：`is_load` 的**双重**门控被整体去掉 | 🔴 转红 |
| M5 | `MemStage`：SB 字节使能掩码恒取 lane3 的值 | 🔴 转红 |
| M6 | `ExStage`：`OP_STORE` 不再进第二操作数选择 | 🔴 转红 |
| M7 | `ExStage`：LUI 不再覆盖 `alu_result` | 🔴 转红 |
| M8 | `ExStage`：SRA 去掉 `shift_amt != 0` 守卫 | 🟢 **恒绿** ⇒ 见「判别力边界」 |
| M9 | `ExStage`：BLT 条件改成相等 | 🔴 转红 |
| M10 | `ExStage`：JALR 目标不再清 bit0 | 🔴 转红（**补了「奇数目标」向量后才可判别**） |
| M11 | `Rv32iDecoder`：**把 B 型立即数的 bit 11 修好** | 🔴 转红 ⇒ **证明本批的钉点有判别力** |
| M12 | `Rv32iDecoder`：U 型立即数退回 20 位掩码 + 左移 12（2026-10-26 修过的老缺陷形态） | 🔴 转红 |
| M13 | `Rv32iDecoder`：S 型立即数丢掉高 7 位 | 🔴 转红 |

⇒ **12/13 抓**。

### ⚠️ 如实登记 4 条「判别力边界 / 无效变异」（**不是**测试缺口的三条 + 一条真缺口）

1. **M8 恒绿 = 框架语义层面的边界**：`ex_stage.h` 的 SRA `shift_amt != 0` 守卫**不可判别**
   ⇒ ⇒ 机制：框架的 `bv_shl_scalar`（`include/bv/bv_shift.h:22`）对**移位量 ≥ 位宽**
   已返回 **0** ⇒ ⇒ 那道守卫**冗余但无害** ⇒ ⇒ **不是**测试缺口。
2. **M10 初版恒绿 = 我自己的激励缺口**（**已补**）：原 JALR 向量的目标是**偶数**
   ⇒ ⇒ `& ~1` 不可观测 ⇒ ⇒ 补了「奇数目标」向量（`rs1+imm = 0x2005` ⇒ `& ~1` ⇒ `0x2004`）后转红。
3. **M2 / M4 / M6 的初版是无效变异**（**已修正**）：M2 只改了链的**第一**行 ⇒ 后面三行仍覆盖它
   ⇒ 恒绿；M4 只去掉 `is_load` 的**一层**门控 ⇒ 另一层仍在 ⇒ **语义等价** ⇒ 恒绿
   ⇒ ⇒ 改成「整条链」与「两层一起去掉」；M6 用 `ch_bool(false)` 替换 ⇒ **与 `|` 不兼容 ⇒ 编译失败**
   ⇒ ⇒ 改用「永不匹配的 opcode」。
4. **一条真缺口（已补）**：M10 暴露「JALR 的 `& ~1` 之前没有任何**奇数目标**向量」
   ⇒ ⇒ **激励表必须覆盖「被门控的那一位恰好会变」的形态**。

## 🔴🔴 本批挖出 2 个新缺陷（均实测确认 + 独立立项；**本批刻意不修**）

### ① 🔴🔴 `Rv32iDecoder` 的 **B 型立即数 bit 11 被放在 bit 0**（**生产路径也中招**）

`rv32i_decoder.h:109-114` 与 **`id_stage.h:140-145`**（**生产路径**）都写成：

```cpp
    (bits<11, 8>(io().instruction) << ch_uint<4>(1_d)) |     // imm[4:1]
    bits<7, 7>(io().instruction);                            // ⚠️ 注释写「imm[11]」却**未移位**
```

⇒ RISC-V 规范要求 `instr[7]` 落在立即数的 **bit 11** ⇒ ⇒ **|偏移| ≥ 2048 的条件分支跳错地址**。

**探针实测**：

```
[probe] beq x1,x2,+8     instr=0x00208463  spec_imm=8     impl_imm=8     OK
[probe] beq x1,x2,-8     instr=0xFE208CE3  spec_imm=-8    impl_imm=-2055 <<< MISMATCH
[probe] beq x1,x2,-2048  instr=0xFE0000E3  spec_imm=-2048 impl_imm=-2079 <<< MISMATCH
```

⇒ 误差 = 「bit 11 丢失（−2048）+ bit 0 误置（+1）」⇒ 净偏差 **−2047**。
⇒ **为什么从未暴露**：`|imm| < 2048` ⇒ `imm[11]=0` ⇒ `instr[7]=0` ⇒ 误置的 bit 0 = 0 ⇒ **无误差**
⇒ ⇒ 仓内小固件（偏移都在几十字节内）**完全不受影响** ⇒ ⇒ 长期「假绿」。

⇒ **本批处置**：**钉住当前实现的行为**（修复时本文件会转红 ⇒ 那是一次**有意的**行为变更信号；
先例：`test_arithmetic_advance_dual.cpp` 对 `carry_save_adder` 的 carry 截断的处理）
⇒ ⇒ **独立立项**：`.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
⇒ ⇒ **修复属独立 change**（行为变更 ⇒ 需 ADR ⇒ 两处 `<< 11` MUST 一起改，否则两个组件的
`imm` 会不一致）。

### ② `ex_stage.h` 的 `BRANCH_*` **常量块**与**实现**（及 RISC-V 规范）**相反**

常量块写 `BLT=2 · BGE=3 · BLTU=4 · BGEU=5` ⇒ ⇒ 而实现用 **RISC-V funct3**
（`BLT=4 · BGE=5 · BLTU=6 · BGEU=7`）⇒ ⇒ **同一个头文件里自相矛盾**。
⇒ **零消费者**（全仓只命中定义处）⇒ ⇒ **零行为影响**，但**纯误导性 API**
⇒ ⇒ 独立立项：`.rddf/improvements/rv32i-branch-type-constants-contradict-impl-2026-11.md`
（**零行为变更 ⇒ 不需 ADR**）。

## ⚠️ 三个组件的**死输入端口**（未驱动 = 未定义值 ⇒ MUST 显式驱动）

| 组件 | 死端口（`describe()` 从不读） |
|---|---|
| `MemStage` | `rs2_data` · `is_store` |
| `ExStage` | `funct3` · `funct7` |

⇒ ⇒ 本文件对**每一个**输入端口都显式驱动 ⇒ ⇒ **不依赖「未驱动也无所谓」的碰巧通过**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_rv32i_stage_dual.cpp`（**新**） | 3 个 pair（`MemStage` 24 组 · `ExStage` 28 组 · `Rv32iDecoder` 17 组）= **460 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目 |
| `.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`（**新**） | 缺陷 ① 的独立立项（5 段） |
| `.rddf/improvements/rv32i-branch-type-constants-contradict-impl-2026-11.md`（**新**） | 缺陷 ② 的独立立项（5 段） |
| 台账 | plan §5 墙钟 · objective §2.1bis（现状 59 → **60** · 「不可达」结论推翻）· handoff §1/§4.7/§5 · feature fragment 的验收勾选 |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**920 == 2 × 460**
- [x] 变异 **12/13 抓** + 1 条如实登记的判别力边界（+ 3 条无效变异的设计教训）
- [x] 期望值**手推**（独立 Python 模型 + 规范编码器 ⇒ **一次通过**）
- [x] **零 DUT 复制**（三个库组件直接当 DUT）
- [x] 每个输入端口（含**死端口**）都显式驱动
- [x] 全量重建后 `ctest -LE perf` **226/226** 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 全部 `scripts/verify_*.sh` exit 0（含命名诚实性自检 3/3）
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
