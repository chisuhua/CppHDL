---
name: rv32i-b-type-immediate-bit11-misplaced-2026-11
priority: P1
source: 路线 C 第一批（`Rv32iDecoder` / `MemStage` / `ExStage` 双后端 parity）实施期实测挖出
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-11-03
---

# Improvement: RV32I **B 型立即数的 bit 11 被放在 bit 0**（分支目标错 2047）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03，ADR-063 + change `rv32i-b-type-immediate-fix-2026-11` ⇒ 已归档 `openspec/changes/archive/2026-11-03-rv32i-b-type-immediate-fix-2026-11/`，commit `3986d99`）
- **来源**：路线 C 第一批的**手推期望表**（独立 Python 模型按 RISC-V 规范推导）
  ⇒ ⇒ 与实现不符 ⇒ ⇒ 用**独立探针**实测确认。

## 1. Why (背景与动机)

RISC-V 的 B 型立即数字段布局是：

```
imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]
```

⇒ 即 `instr[7]` 承载的是 **`imm[11]`**（13 位立即数的第 11 位）。

🔴 **两处实现都把 `instr[7]` 直接 OR 到立即数的 bit 0**（**漏了 `<< 11`**）：

### ① `include/cpu/pipeline/rv32i_decoder.h:109-114`

```cpp
ch_uint<13> imm_b_base =
    (bits<31, 31>(io().instruction) << ch_uint<12>(12_d)) |  // imm[12]
    (bits<30, 25>(io().instruction) << ch_uint<6>(5_d)) |    // imm[10:5]
    (bits<11, 8>(io().instruction) << ch_uint<4>(1_d)) |     // imm[4:1]
    bits<7, 7>(io().instruction);                            // ⚠️ 注释写「imm[11]」，
                                                           //    代码却**未移位** ⇒ 落在 bit 0
```

⇒ ⚠️ **注释与代码自相矛盾**（注释明写 `imm[11]`，代码把它放在 bit 0）⇒ 这是**笔误**，
不是设计选择。

### ② 🔴🔴 `include/cpu/pipeline/stages/id_stage.h:140-145`（**生产路径**）

```cpp
ch_uint<13> imm_b_base =
    (bits<31, 31>(io().instruction) << ch_uint<5>(12_d)) |
    (bits<30, 25>(io().instruction) << ch_uint<3>(5_d)) |
    (bits<11, 8>(io().instruction) << ch_uint<1>(1_d)) |
    bits<7, 7>(io().instruction);        // ⚠️ 同样漏了 << 11
```

⇒ ⇒ **`id_stage` 自己算立即数**（`Rv32iDecoder` **不被流水线使用** ⇒ 见 §5）
⇒ ⇒ ⇒ **生产路径的分支目标计算受影响**：
`idex_imm` → `ex_stage` 的 `branch_target_calc = io().pc + io().imm`
⇒ ⇒ ⇒ **任何 |偏移| ≥ 2048 字节的条件分支都会跳到错误地址**。

### 实测证据（独立探针 `/tmp/opencode/probe_dec_btype.cpp`，不入仓）

```
[probe] beq x1,x2,+8     instr=0x00208463  spec_imm=8     impl_imm=8     OK
[probe] beq x1,x2,-8     instr=0xFE208CE3  spec_imm=-8    impl_imm=-2055 <<< MISMATCH
[probe] beq x1,x2,-2048  instr=0xFE0000E3  spec_imm=-2048 impl_imm=-2079 <<< MISMATCH
```

⇒ ⇒ 误差 = 「**bit 11 丢失**（−2048）+ **bit 0 被误置**（+1）」⇒ ⇒ 净偏差 **−2047**。

### 为什么从未暴露

① **`|imm| < 2048` ⇒ `imm[11] = 0` ⇒ `instr[7] = 0` ⇒ 误置的 bit 0 = 0 ⇒ 无误差**
   ⇒ ⇒ **仓内所有小固件（分支偏移都在几十字节内）完全不受影响**；
② 仓内**唯一**的 B 型立即数消费者是**仓外** riscv-tests 二进制 ⇒ CI 全 SKIP；
③ `Rv32iDecoder` 的**唯一**消费者是 `examples/riscv-mini/` 的 3 个 phase 测试
   ⇒ ⇒ 它们只做「译码字段对不对」的检查，而**没有对照 RISC-V 规范**。

⇒ ⇒ **典型的「零判别力 ⇒ 长期假绿」**。

## 2. What Changes (变更范围)

- **`rv32i_decoder.h` 与 `id_stage.h` 的 `imm_b_base`**：`bits<7, 7>(instr)`
  MUST 移位到 **bit 11** ⇒ ⇒ 形如
  `(bits<7, 7>(io().instruction) << ch_uint<11>(11_d))`。
  ⚠️ **移位量的位宽**要能装下 11（现有写法用 `ch_uint<5>(12_d)` 等「够宽即可」的惯例
  ⇒ ⇒ `ch_uint<4>(11_d)` 已够（0–15））。
- ⚠️ **ADR 需求**：这是**行为变更**（分支目标会变）⇒ ⇒ 需独立 ADR
  ⇒ ⇒ **不在 parity 批里做**。
- ⚠️ **配套**：修好后 MUST 重新核对
  ① `examples/riscv-mini/tests/test_pipeline_exec.cpp` 的 golden（它断言译码字段）
  ② `examples/riscv-mini/rv32i_phase{1,2,3}_test.cpp` 的打印/断言
  ③ 仓内小固件的分支偏移（**不受影响** ⇒ golden 不变）
  ④ `Rv32iDecoder` / `IdStage` 的双后端 parity 用例**会转红** ⇒ ⇒ 那是一次
     **有意的**行为变更信号（本批已把当前行为钉住 ⇒ 见 §3）。

## 3. Acceptance (验收标准)

- [ ] `beq x1, x2, -8` ⇒ `imm == -8`（`0xFFFFFFF8`）
- [ ] `beq x1, x2, -2048` ⇒ `imm == -2048`（`0xFFFFF800`）
- [ ] `beq x1, x2, +8` ⇒ `imm == 8`（**不变** ⇒ 回归护栏）
- [ ] **正偏移 ≥ 2048**（`imm[11]=1` 的正偏移）⇒ MUST 也正确
      （⚠️ 当前实现对正偏移同样错 ⇒ 判据 MUST 覆盖**两个方向**）
- [ ] `id_stage` 的 `imm` 与 `Rv32iDecoder` 的 `imm` **逐位一致**
      （⇒ 判据：同一指令字分别喂两个组件 ⇒ `imm` 相等）
- [ ] **变异证据**：把 `<< 11` 去掉 ⇒ ⇒ parity 用例 MUST 转红
      （⇒ ⇒ 本批钉住的「当前行为」就是这条变异 ⇒ **它已有判别力**）
- [ ] ⚠️ **本批（parity 批）**把**当前（错误）行为**钉住 ⇒ 修复时用例会转红
      ⇒ ⇒ 转红是**有意的**行为变更信号（先例：`test_arithmetic_advance_dual.cpp`
      对 `carry_save_adder` 的 carry 截断的处理）

## 4. Capabilities (契约归属)

- capability `rv32i-isa-semantics` **MODIFIED ×1**（或新建
  `rv32i-branch-immediate`）：写明「B 型立即数 MUST 按
  `imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]` 提取」+「`|imm| ≥ 2048` 的
  分支 MUST 落在正确目标」两条 Requirement/Scenario。
- ⚠️ **命名诚实性**：`Rv32iDecoder` 的头注释写「符号扩展立即数」⇒ ⇒ 修好前是**说谎**的
  ⇒ ⇒ 登记表（`.rddf/checks/chlib-naming-honesty-registry.md`）若收录它，MUST 写明
  当前偏差。

## 5. Impact (影响面)

- 🔴 **生产路径受影响**：`id_stage.h` 的 `imm_b` ⇒ ⇒ `idex_imm` ⇒ ⇒
  `ex_stage` 的 `branch_target = pc + imm` ⇒ ⇒ ⇒ **|偏移| ≥ 2048 的条件分支跳错地址**。
- ⚠️ **`Rv32iDecoder` 本身不是流水线的一部分**（全仓实例化点只有
  `examples/riscv-mini/rv32i_phase{1,2,3}_test.cpp` 与
  `examples/riscv-mini/tests/test_pipeline_exec.cpp`）⇒ ⇒ 它是**库组件 / 示例用**，
  ⇒ **同一处笔误被复制了两份**（`rv32i_decoder.h` 与 `id_stage.h`）
  ⇒ ⇒ ⚠️ **修的时候 MUST 两处一起改**（否则两个组件的 `imm` 会不一致）。
- **爆炸半径**：仓内小固件**不受影响**（偏移 < 2048）⇒ ⇒ golden 不变 ⇒ ⇒
  ⇒ **回归风险低**；但**真实程序**（大函数 / 长回跳循环）会踩。
- ⚠️ **本批的处置**：parity 批**只钉住当前行为** + **登记本 draft**
  ⇒ ⇒ 修缺陷属**独立 change**（需 ADR）。
