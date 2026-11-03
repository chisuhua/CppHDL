---
name: rv32i-branch-type-constants-contradict-impl-2026-11
priority: P3
source: 路线 C 第一批（`ExStage` 双后端 parity）实施期实测挖出
phase: phase-5
category: cpu
type: docfix
created_at: 2026-11-03
evidence: probe:include/cpu/pipeline/stages/ex_stage.h
---

# Improvement: `ex_stage.h` 的 `BRANCH_*` 常量与**实现**（及 RISC-V 规范）**相反**

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03 · change `header-self-containment-fix-2026-11`，commit `8e7367f`、归档 `0bb1fb4`）⇒ ⇒ **处置 = 物理删除**（不是订正数值）：该常量块**零消费者**（`grep -rn "BRANCH_BEQ\|BRANCH_BNE\|BRANCH_BLT\|BRANCH_BAU" include/ src/ tests/` **无输出**），而留着它会持续诱导后人用它去比较 RISC-V 的 `funct3`（`BLT=4` 恰好等于错的 `BRANCH_LTU=4` ⇒ 「看起来能用」）。`ex_stage.h:48-53` 现只留一段注释记录「曾有过、为何删」。⇒ 判据 = 上述 grep 零输出（无同名符号残留）。
- **来源**：路线 C 第一批的**手推期望表**与实现不符 ⇒ ⇒ 追查后定位。

## 1. Why (背景与动机)

`include/cpu/pipeline/stages/ex_stage.h:42-48` 的常量块：

```cpp
constexpr unsigned BRANCH_EQ   = 0;   // BEQ
constexpr unsigned BRANCH_NE   = 1;   // BNE
constexpr unsigned BRANCH_LT   = 2;   // BLT
constexpr unsigned BRANCH_GE   = 3;   // BGE
constexpr unsigned BRANCH_LTU  = 4;   // BLTU
constexpr unsigned BRANCH_GEU  = 5;   // BGEU
```

🔴 而**同一个文件**里的实现（`describe()` 的分支条件多选器）用的是
**RISC-V funct3 编码**：

```cpp
branch_cond = select(io().branch_type == ch_uint<3>(0_d), rs1_eq_rs2, ...);   // BEQ
branch_cond = select(io().branch_type == ch_uint<3>(1_d), rs1_ne_rs2, ...);   // BNE
branch_cond = select(io().branch_type == ch_uint<3>(4_d), rs1_lt_rs2_signed, ...);   // BLT
branch_cond = select(io().branch_type == ch_uint<3>(5_d), rs1_ge_rs2_signed, ...);   // BGE
branch_cond = select(io().branch_type == ch_uint<3>(6_d), rs1_lt_rs2_unsigned, ...); // BLTU
branch_cond = select(io().branch_type == ch_uint<3>(7_d), rs1_ge_rs2_unsigned, ...); // BGEU
```

⇒ ⇒ **同一个头文件里，常量块与实现互相矛盾** ⇒ ⇒ 而**实现是对的**
（RISC-V 规范：BEQ=000 · BNE=001 · BLT=100 · BGE=101 · BLTU=110 · BGEU=111
⇒ 0/1/4/5/6/7）。

### 实测证据

路线 C 第一批的手推期望表**最初用了常量块的编码**（BLT=2）⇒ ⇒
**两后端一致地**给出 `branch_taken = 0`（而期望 1）⇒ ⇒ 按「两后端一致 ⇒ 同一处推导错」
的判据 ⇒ ⇒ 错在**期望表**（用了错的常量）⇒ ⇒ 改用实现/规范的编码后**两后端吻合**。

⇒ ⇒ 这同时是「**两后端一致 ⇒ 是我的推导错**」这条判据的**又一次生效实例**
（历史：第 4 次）。

### 影响面

- ✅ **零行为影响**：全仓**没有任何消费者**引用这 6 个常量
  （实测：`grep -rn "BRANCH_EQ\|BRANCH_NE\|BRANCH_LT\|BRANCH_GE\|BRANCH_LTU\|BRANCH_GEU"`
  只命中 `ex_stage.h` 的**定义处**）⇒ ⇒ **纯误导性 API**。
- ✅ **生产路径是对的**：`rv32i_pipeline.h:262` 把
  `ex_stage.io().branch_type <<= id_stage.io().funct3` ⇒ ⇒ 喂进去的是
  **RISC-V funct3** ⇒ ⇒ 与实现的编码一致 ⇒ ⇒ 流水线行为**正确**。
- ⚠️ **危害是「未来的使用者」**：任何人照着常量块写 `branch_type` 的驱动
  ⇒ ⇒ **BLT/BGE/BLTU/BGEU 四个条件全部错**（而且错得**很隐蔽**：BEQ/BNE 仍对）。

## 2. What Changes (变更范围)

- **`ex_stage.h` 的常量块**：改成与实现（= RISC-V funct3）一致：

  ```cpp
  constexpr unsigned BRANCH_EQ   = 0;   // BEQ  (funct3 = 000)
  constexpr unsigned BRANCH_NE   = 1;   // BNE  (funct3 = 001)
  constexpr unsigned BRANCH_LT   = 4;   // BLT  (funct3 = 100)
  constexpr unsigned BRANCH_GE   = 5;   // BGE  (funct3 = 101)
  constexpr unsigned BRANCH_LTU  = 6;   // BLTU (funct3 = 110)
  constexpr unsigned BRANCH_GEU  = 7;   // BGEU (funct3 = 111)
  ```

- ⚠️ **零行为变更**（无消费者）⇒ ⇒ **不需要 ADR** ⇒ ⇒ 可与任何 change 同行。
- ⚠️ **命名诚实性**：本仓的「命名诚实性登记表」目前只覆盖 `chlib` 域
  ⇒ ⇒ 本条**不在**该表的范围内 ⇒ ⇒ 靠**代码评审 + 本 draft** 兜住。

## 3. Acceptance (验收标准)

- [ ] 常量块的 6 个值与实现的 `branch_type` 分派**逐位一致**
- [ ] 常量块旁**注明**「= RISC-V funct3 编码」
- [ ] 全仓仍**零消费者**（不需要加消费者）
- [ ] **变异证据**：把 `BRANCH_LT` 改回 `2` ⇒ ⇒ 若将来有人开始用它 ⇒ 必错
      ⇒ ⇒ **本条判据用「静态 grep 实现里的分派编码」**（不是运行期断言）

## 4. Capabilities (契约归属)

- capability `rv32i-isa-semantics` **MODIFIED ×1**（可选）：写明
  「`branch_type` 的编码 MUST 是 RISC-V funct3（BEQ=0 · BNE=1 · BLT=4 · BGE=5 ·
  BLTU=6 · BGEU=7）」⇒ ⇒ 防止再次漂移。
- ⚠️ **可执行判据**：`tests/test_rv32i_stage_dual.cpp` 的 `ExStage` pair
  **已按实现/规范的编码**断言 4 个分支条件（BLT/BGE/BLTU/BGEU）
  ⇒ ⇒ ⇒ **该测试本身就是这条契约的可执行判据**（若实现改了编码 ⇒ 转红）。

## 5. Impact (影响面)

- **生产消费者**：**0** ⇒ ⇒ **爆炸半径为零** ⇒ ⇒ 可以独立修。
- ⚠️ **本批（parity 批）不改它**：本批钉住的是**实现**的行为（正确的一侧）
  ⇒ ⇒ 常量块的订正**零行为变更** ⇒ ⇒ 可与本批同行，也可在后续 change 单独做。
