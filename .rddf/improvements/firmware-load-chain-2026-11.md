---
name: firmware-load-chain-2026-11
priority: P1
phase: phase-6
category: cpu
type: feature
状态: approved
进度: L1–L6 的 5 项裁决已落定（2026-11-11）· B3 未开工 · ⚠️ **前置门 = 公开面登记未做 ⇒ 不得开工**
主题: 已知功能缺口清零
依赖: rtl-emit-product-tooling-2026-11（manifest）· rv32i-canonical-home（SoC 顶层归属）
来源: 用户 2026-11-10 拍板「② 固件转储器作为项目工具能力，并考虑添加 ELF 装载固件能力」+ planner 实测装载链路缺口
生成时间: 2026-11-10
evidence: probe:examples/riscv-mini/src/rv32i_pipeline_top.h:112-133 + include/cpu/pipeline/rv32i_tcm.h:53,100
---

# Improvement: 固件**装载链路**（编码器库化 + 地址映射 + 统一加载端口 + ELF 装载）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **用户裁决（2026-11-10）**: 「② 固件转储器也可以做为项目提供的工具能力，同时再考虑添加一个 ELF 装载固件的能力」
- **依赖判定**: ⚠️ **ELF 装载当前不可行** ⇒ ⇒ 本项 MUST **先**修装载链路（地址映射 + D-TCM 加载通路），ELF 是其**下游**

---

**主题**: 已知功能缺口清零

## 架构依据

### A.1 实测事实（2026-11-10）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **固件加载端口只接 I-TCM**
| ② | ⚠️ **地址高位被静默截断**
| ③ | ⚠️ **metis 订正（原稿算术错）**：`0x8000_1000 & bits[13:2]` = **0x400（word 1024）**，**不是** word 0；且 `.text` / `.data` 分别进**两块独立 TCM** ⇒ ⇒ **不构成互相覆盖** ⇒ ⇒ 真正会别名的是「**同一块**存储器内相差 0x4000 的地址」（例 `0x8000_0000` vs `0x8000_4000`）⇒ ⇒ 原稿「两者都映射到 word 0 ⇒ 互相覆盖」**作废** | `rv32i_pipeline_top.h:112,121`（`bits<kTcmAddrWidth+1,2>`）· `kTcmAddrWidth = 12` |
| ④ | ⚠️ **metis 订正（原稿「PC 与 base 不一致」是伪命题）**：`pc_reg` 复位值 = `0x80000000`（`stages/if_stage.h:71`）与 `ADDR_ITCM_BASE = 0x80000000`（`address_decoder.h:33`）**本来就一致** ⇒ ⇒ 原稿 L1 的「PC 复位值与区域 base 一致」这条**恒绿** ⇒ ⇒ **作废**；真正的缺陷是「**PC 有效位宽** vs **TCM 索引位宽**」不匹配（截断的**后果**，不是 base 错配）⇒ ⇒ **删掉「改 PC 复位值」这个 work item**（它会改全部 `dbg_id_pc` 期望值却不带来任何交付） | `stages/if_stage.h:71` · `address_decoder.h:33` |
| ⑤ | TCM 规模由**模板参数**决定（默认 `ADDR_WIDTH = 20` = 1M 字 = 4 MB），SoC 用 **12** | `include/cpu/pipeline/rv32i_tcm.h:26,53,70,100` |
| ⑥ | **编码器无库化载体** ⇒ ⇒ 每次要用都得抄一份 | ⚠️ **metis 订正**：全仓 `enc_r/enc_i/enc_s/enc_b/enc_j` **只有 1 份**（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:205-315`）⇒ ⇒ 原稿「已有 3 份」**夸大**（第 3 份在 `/tmp/opencode/dump_fw.cpp`，**仓外临时**）⇒ ⇒ L3 的实际改动 = **一处迁出**（不是「大 diff」） |
| ⑦ | 既有编码器自带「手算常量交叉校验」纪律 ⇒ ⇒ **库化 MUST 保留**该交叉校验（否则 = 自证）<br>⚠️ **metis 订正**：该交叉校验**必须留在消费者测试里**（含**逐字手算字面量**）⇒ ⇒ **MUST NOT** 搬进 `rv32i_asm_encode.h`（搬进去 = 自证）；另需一条**独立**判据：按 RISC-V 规范**逐字段**断言（opcode/funct3/funct7/imm 各段单断）+ 一个**变异负例**（改 funct3 ⇒ 常量必须不等） | 同上 `:584-621` |
| ⑧ | 仓内**已有** MMIO 地址译码组件（6 区域）⇒ ⇒ MUST 复用，MUST NOT 新造<br>⚠️ **metis 订正**：该组件的既有映射 = **ITCM `0x80000000`/64KB · DTCM `0x80010000`/64KB** ⇒ ⇒ 与原稿 L1 提的 `0x8000_4000`/16KB **冲突** ⇒ ⇒ **MUST 裁决其一**<br>⚠️ 且**区域 size ≠ TCM 容量**（`kTcmAddrWidth=12` ⇒ 实际 16 KB）⇒ ⇒ size MUST 由 `1<<kTcmAddrWidth` **派生**，**禁手写** ⇒ 否则「区域内但超容量」的地址会在 TCM 端口**静默回绕** | `include/cpu/riscv/address_decoder.h:14,31-43` |
| ⑨ | 仓内**已有** CLINT（定时器中断）⇒ ⇒ 同上 | `include/cpu/riscv/clint.h`（仅测试消费） |

### A.2 为什么 ELF 不能先做

ELF32 `PT_LOAD` 解析本身很便宜（不依赖 libelf，≈100 行）⇒ ⇒ **难点不在解析**，在**装到哪里**：
- ①（无 D-TCM 加载口）⇒ `.data` 装不进去
- ②（地址截断）⇒ `.text` 与 `.data` **地址冲突**

⇒ ⇒ **先修装载链路，ELF 只是其上的一层薄壳** ⇒ ⇒ 本项把「ELF」作为**子交付**而非独立项。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **L1** | **地址映射**（SoC 侧）：每个存储区域显式声明 `{name, base, size}`（size **由 `1<<kTcmAddrWidth` 派生**）⇒ ⇒ **byte-addressed** ⇒ ⇒ **禁止静默截断**。<br>⚠️ **metis 订正**：① **不改 PC 复位值**（见 A.1④）② 「越界 ⇒ 报错」**必须拆成两件事**：**host 侧 loader/工具**在写之前做检查（`CHFATAL` + 非零退出 · 可机械验收）＋ **RTL 侧**只能给错误**信号/端口**（`describe()` 里拿 `ch_in` 的值判定 ⇒ **建图期恒假 ⇒ 永不触发** ⇒ **禁止**在 `describe()` 里判硬件值） |
| **L2** | **统一加载端口**：SoC 顶层提供**单一**加载接口，**同时**可写 I-TCM 与 D-TCM ⇒ ⇒ 由地址映射**译码**决定落哪个区域 ⇒ ⇒ `.text` / `.data` 用同一套机制<br>⚠️ **metis 订正**：**新增 `ch_in` 端口 ⇒ 全部实例化点（含所有既有 TB 与跨仓下游）MUST 显式驱动**（未驱动读到**未定义值** ⇒ `load_we` 可能是 1 ⇒ 加载期随机写坏 TCM）⇒ ⇒ **推荐不新增端口**：用**ADR 版本化的新顶层组件**（如 `Rv32iSocTopV2`）承载新接口，`Rv32iPipelineTop` 按 ADR-046 D10 **保持冻结**（保住 C6 快照与下游） |
| **L3** | **编码器库化**：新增 `include/cpu/riscv/rv32i_asm_encode.h`（`enc_r/enc_i/enc_s/enc_b/enc_j` + `ADDI/ADD/SUB/SW/SB/SH/LW/LB/LBU/LH/LHU/BEQ/BNE/JAL/JALR/AUIPC/SRA`）⇒ ⇒ 既有 phase 测试与 parity 测试**改为引用**它 ⇒ ⇒ ⚠️ **保留**手算常量交叉校验（纪律不丢） |
| **L4** | **固件转储器工具**：`tools/rv32i-fw-dump`（读固件清单 → 输出 Verilog `$readmemh` 数组 / `.mem` / **装载 manifest**（段地址 / 符号 / 入口 / 字索引映射））⇒ ⇒ manifest 供 TB 生成与 golden 断言**共用**（避免两处解析） |
| **L5** | **ELF 装载**：ELF32 little-endian RISC-V `PT_LOAD` 解析 ⇒ ⇒ 按 L1 的映射把段**写入对应区域** ⇒ ⇒ `.bss` 清零 ⇒ ⇒ 产出装载 manifest |
| **L6** | **固件加载的可观测性**：加载完成后可断言「某地址的内容 == 期望」（供 TB / CI 用）⇒ ⇒ **不是**「加载成功」布尔值 |

### ❌ 不在本项

- **D-TCM 读写时序**（`aread` → `sread`）⇒ 归 `memory-timing-contract-2026-11`（**依赖**：加载后要能按新时序读回）
- **MMIO 设备接入**（CLINT / 外设）⇒ 本项只把**译码面**准备好，**不**接设备 ⇒ ⇒ 复用 `address_decoder.h` 但不改它的设备语义<br>- ⚠️ **metis 追加**：**不得承诺能跑 `riscv-tests`** —— 那批用例假定**单块 256 KB @ `0x80000000`**（`examples/riscv-mini/tests/test_riscv_tests.cpp`）⇒ ⇒ **16 KB TCM 是硬边界** ⇒ ⇒ 「跑真程序」的真实距离比本项大得多 ⇒ ⇒ MUST 在 §Impact 如实登记
- **跑真程序（裸机 / 特权态）** ⇒ ⇒ 需 `rv32i-isa-gap-closure-2026-11`（非法指令 trap 等）
- **任何流水线行为改动** ⇒ 本项只改 SoC 顶层的**存储装配面**

---

## Why

### W.1 「能跑用户态程序」与「能装载真程序」之间差一个装载链路

现状：程序**只能**通过 `prog_*` 逐字写进 I-TCM ⇒ ⇒ 只能装「纯代码、零初始化数据」的程序 ⇒ ⇒ **任何真实程序（哪怕 `int a = 5;`）都装不进去**（`.data` 无处可放）。

### W.2 静默截断是**最坏的一类缺陷**

② ⇒ ⇒ ELF 的 `.text` 与 `.data` 会**互相覆盖** ⇒ ⇒ 程序"能装进去"但**行为错** ⇒ ⇒ **无任何报错**。
⇒ ⇒ 这与仓内既有纪律完全同型（「硬件值 MUST NOT 参与 C++ 控制流」那条：条件恒假 ⇒ 分支体不发 ⇒ 输出恒未驱动 ⇒ **零报错**）⇒ ⇒ 必须把"截断"改成"报错"。

### W.3 编码器抄了 3 份 ⇒ 迟早漂

⑥ ⇒ ⇒ 三份编码器，其中两份是**本轮 planner 为了出 TB 临时抄的** ⇒ ⇒ 这就是"没有载体"的必然结果 ⇒ ⇒ 库化 + **保留**交叉校验 = 唯一正解。

### W.4 装载 manifest 是 TB 与 CI 的共用数据源

用户 ③（examples 独立仿真）需要知道「哪个字放哪」⇒ ⇒ 若 TB 与 CI 各自解析 ELF/固件 ⇒ ⇒ 漂移 ⇒ ⇒ manifest 一次生成、多方消费。

---

## What Changes

### L1 — 地址映射（SoC 侧）

- 新增**显式映射表**（SoC 顶层或一个 `MemoryMap` 组件）：
  | 区域 | base | size | 说明 |
  |---|---|---|---|
  | `itcm` | `0x8000_0000` | 16 KB | I-TCM · PC 起点 |
  | `dtcm` | `0x8001_0000` | `1 << (kTcmAddrWidth + 2)` = **16 KB**（**派生，禁手写**） | D-TCM · 数据<br>🔴 **critic 第 2 轮订正（原公式差 4 倍）**：`kTcmAddrWidth = 12` 是**字索引**宽度（`rv32i_pipeline_top.h:108-109`）⇒ 字节容量 = `1 << (12+2)` = **16 KB**（`1 << 12` = 4096 B = **4 KB**，错）<br>⚠️ **2026-11-10 裁决②**：沿用 `address_decoder.h:34` 的 `ADDR_DTCM_BASE = 0x80010000`（**不**改成 `0x8000_4000`） |
  | （预留 MMIO 区） | — | — | 本项**只留位**，不接设备 |
- ⚠️ **base 与 PC 复位值必须一致** ⇒ ⇒ 消除「0x80000000 只低 12 位有效」的错配。
- **越界 ⇒ 报错**（`CHFATAL` / 诊断）⇒ ⇒ 禁止截断。
- ⚠️ **兼容边界**：SoC 顶层端口契约（ADR-046 D10）**会变**（`prog_addr` 的语义从"字索引"变成"字节地址"）⇒ ⇒ **公开面变更** ⇒ ⇒ 走 `public-api-remove-visibility` 登记 + CHANGELOG + 可能需要 ADR（**MUST 由 architect 裁决** ⇒ 见 Impact）。

### L2 — 统一加载端口

- 顶层加载接口语义：`(we, addr[byte], data[word], size)` ⇒ ⇒ 由 L1 的映射译码到 I-TCM 或 D-TCM ⇒ ⇒ **一次接口装两个区域**。
- ⚠️ **端口数量变化** ⇒ ⇒ SoC 顶层端口契约变化 ⇒ ⇒ 与 L1 同批裁决。

### L3 — 编码器库化

- 新增 `include/cpu/riscv/rv32i_asm_encode.h`（头文件 only · `constexpr`）⇒ ⇒ 既有 `test_rv32i_soc_dual_parity.cpp` 与 `rv32i_phase{1,2,3}_test.cpp` 改为引用 ⇒ ⇒ **删除其本地副本**。
- ⚠️ **保留** `FirmwareEncoderCrossCheck`（手算常量 vs helper 产出）⇒ ⇒ 库化后交叉校验**照旧**（防止"库里的编码器自证"）。
- ⚠️ **零消费者检查**：`rv32i_asm_encode.h` 的消费者 = 既有测试 + 新工具 ⇒ ⇒ 满足仓内 ZERO-DEBT 的「有消费者」要求。

### L4 — 固件转储器工具

```
tools/rv32i-fw-dump --in <fw.json|elf> --format vh|mem|manifest --out <path>
```
- 输入：一个**固件清单**（`{name, words[], base_addr}` 的 JSON）⇒ ⇒ 供「手写汇编 → 固件」的工作流使用。
- 输出：`vh`（Verilog `$readmemh` 可读）/ `mem`（纯 hex）/ `manifest`（段 / 符号 / 入口 / 字索引映射）。

### L5 — ELF 装载

- 解析 ELF32 header + program headers（`PT_LOAD`）⇒ ⇒ 取 `p_paddr` / `p_filesz` / `p_memsz` ⇒ ⇒ 按 L1 映射写入 ⇒ ⇒ `.bss`（`memsz > filesz`）补零。
- ⚠️ **不解析符号表**（本项不需要）⇒ ⇒ 符号留给未来工具。
- ⚠️ **不做重定位**（静态链接的裸机 ELF ⇒ `.rel.dyn` 罕见）⇒ ⇒ 若遇到非零重定位 ⇒ ⇒ **报错**（不是静默忽略）。

### L6 — 装载可观测性

- 装载后提供「读回某地址」的路径（走**真实**读通路）⇒ ⇒ TB 可以断言「装进去的值 == 期望」⇒ ⇒ **不是**「loader 返回成功」。

---

## Acceptance

### 地址映射

- [ ] SoC 显式声明区域 `{base, size}`，且 size **由 TCM 容量派生**（禁手写）；`dtcm.base == 0x8001_0000`（沿用 `address_decoder.h:34`）
- [ ] ⚠️ **截断等价的最小判据**（注意：对齐要求是 **16 KB** 而非 4 KB）
- [ ] ⚠️ **受影响地址字面量清单**（裁决①的连带面，**不许只报计数增量**）：`grep -n 'has_store(t, 0x'` 逐条登记改绝对地址后需重推的值；并**验证**新增 `LUI`→`SW(base)` 的 RAW 已被前推档位 1 覆盖（否则 `load_use_stalls`/`control_stalls` 也会变）
- [ ] **越界访问 ⇒ 报错**（不再静默截断）⇒ ⇒ 投毒自证：把地址设到区域外 ⇒ MUST 有诊断（不是静默丢高位）<br>⚠️ **metis 订正**：错误 MUST 由 **host 侧**检查给出（`CHFATAL` + 非零退出）⇒ ⇒ **RTL 侧**若也要报错 ⇒ MUST 是**新增端口**（非 `describe()` 内的 C++ 判定）⇒ ⇒ 两件事 MUST 分开验收
- [ ] ⚠️ **metis 订正**：原稿「`.text` 与 `.data` 不再互相覆盖」在旧实现下**也成立**（分进两块 TCM）⇒ ⇒ **恒绿 · 已删除**。改为可判别形态：**同一块**存储器内相差 `0x4000` 的两个地址 MUST **不别名**（探针：同一条加载总线写 `base+0` 与 `base+0x4000` 两个不同值 ⇒ 断言互不覆盖）+ 边界值 `base+size` MUST 报错

### 装载

- [ ] **D-TCM 有加载通路**（当前**完全没有**）⇒ ⇒ 一个带 `.data` 的固件可被装载并读回
- [ ] 加载后读回某地址 == 期望值（走真实读通路，非 loader 返回值）
- [ ] `prog_*` 的**地址语义变更**已在 CHANGELOG + 公开面登记；SoC 端口契约变化已由 architect 裁决（ADR 或 spec delta）

### 编码器

- [ ] `include/cpu/riscv/rv32i_asm_encode.h` 存在；既有 3 处编码器副本**归一**为 1 份（仓内 `enc_r`/`enc_i` 等定义**唯一**）
- [ ] ⚠️ `FirmwareEncoderCrossCheck`（手算常量交叉校验）**仍在**且通过
- [ ] 新头文件有**仓内消费者**（既有测试 + 新工具）⇒ ⇒ 不违反 ZERO-DEBT

### 工具

- [ ] `tools/rv32i-fw-dump` 可独立运行；输出 `vh` / `mem` / `manifest` 三种格式
- [ ] ELF32 little-endian RISC-V 的 `PT_LOAD` 可装载；`.bss` 补零
- [ ] 非零重定位 ⇒ **报错**（不静默忽略）
- [ ] 装载 manifest 含 段地址 / 入口 / 字索引映射 ⇒ ⇒ TB 与 CI 共用同一份

### 不回归

- [ ] 既有 ctest 全绿（编码器归一 MUST NOT 改任何编码结果 ⇒ ⇒ 既有 golden 全绿）—— ⚠️ **数值 MUST 引用 `CTEST_TOTAL` 锚点**，**禁**写死「257」（本项新增测试后该数字即过期）
- [ ] 既有 `test_rv32i_soc_dual_parity` 的 **20 项 golden** 仍全绿
- [ ] ⚠️ **metis 订正**：原稿引用 `/tmp/opencode/rv32i_rtl/tb_rv32i.sv` 的 20/20 —— **仓外临时产物 · CI 不可复现** ⇒ ⇒ **不可验收**，**已删除**。替代：由 `examples-rtl-sim-coverage-2026-11` 把该 TB **仓库化**后再作为端到端证据

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-firmware-loading`**（**新**） | ADDED | 装载链路契约：显式地址映射（byte-addressed · 越界报错 · 禁静默截断）· 统一加载接口（I-TCM + D-TCM 同源）· ELF32 `PT_LOAD` 装载语义 · 装载 manifest schema · 装载可观测性（读回断言） |
| `rv32i-isa-semantics` | MODIFIED | 追加「SoC 存储映射 MUST 显式声明 · PC 复位值 MUST 与映射 base 一致」⇒ ⇒ **不**改既有 ISA 语义条款 |
| `cpu-cache-subsystem` | **不动** | 本项不碰 cache |
| `rtl-emit-portability`（I1 新增） | **不动** | 但 SoC 端口契约变化 ⇒ ⇒ manifest 快照 MUST 同步 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `examples/riscv-mini/src/rv32i_pipeline_top.h` | **端口契约变化**（`prog_addr` 语义 + 可能新增加载端口）⇒ ⇒ **ADR-046 D10 的 SoC 端口契约**受影响 ⇒ ⇒ **MUST 由 architect 裁决**（本 improvement 只提出规划，不改 ADR 决策） |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 加载函数改写（旧：逐字写 `prog_*`；新：按区域写）⇒ ⇒ golden **期望值不变**（程序本身不变）⇒ ⇒ 但**取指填充期不变量**可能因装载方式变化而变 ⇒ ⇒ MUST 重新实测并**手推**核对 |
| `include/cpu/riscv/rv32i_asm_encode.h`（新） | 既有 3 处编码器副本归一 ⇒ ⇒ 大 diff 但**零行为变化** |
| `include/cpu/riscv/address_decoder.h` | 复用（可能小幅扩展以支持「byte 地址 + 区域判定」）⇒ ⇒ 该文件**零流水线消费者** ⇒ ⇒ 本项给它**第一个真实消费者** ⇒ ⇒ 顺带消除一项技术债 |
| 计数锚点 | `CTEST_TOTAL` **可能 +1**（新工具的冒烟测试）⇒ ⇒ 同步 `AGENTS.md` |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **PC 复位值改动** ⇒ ⇒ 全部 `dbg_id_pc` 期望值变 ⇒ ⇒ 时序金标重算 | 本项把 PC base **显式化**（消除隐式截断）⇒ ⇒ 期望值 MUST **手推**（禁抄仿真输出，仓内纪律） |
| **SoC 端口契约变化** ⇒ ⇒ 跨仓下游破 | 走 ADR + 公开面登记 + CHANGELOG ⇒ ⇒ **MUST 由 architect 先裁决** |
| **ELF 装载引入「看起来装好了其实装错」** | ① 越界报错（禁截断）· ② 装载后读回断言（L6）· ③ manifest 可审计 |
| **与 `memory-timing-contract` 的时序耦合**（D-TCM 改 `sread` 后加载写回要晚一拍） | 本项**先**做（加载路径与时序正交）⇒ ⇒ 时序项 MUST **在**本项之后改 ⇒ ⇒ 或两者同批但**顺序执行** |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `memory-timing-contract-2026-11` | **时序耦合** ⇒ ⇒ 加载路径先做；`aread`→`sread` 后装载写回的可见性变了 ⇒ ⇒ 两项 MUST **顺序执行**（不并行） |
| `rtl-emit-product-tooling-2026-11` | SoC 端口契约变化 ⇒ ⇒ manifest 快照 MUST 同步 |
| `rv32i-isa-gap-closure-2026-11` | 本项提供装载能力，但**真程序**（含 trap / CSR）还需 ISA 项 ⇒ ⇒ **依赖** |

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **原稿事实③算术错**：`0x8000_1000 & bits[13:2]` = **0x400**（word 1024），不是 word 0；且 `.text`/`.data` 进**两块独立 TCM** ⇒ ⇒ **不构成互相覆盖** | A.1③ 重写；验收「不再互相覆盖」**恒绿 · 已删**，换成「**同一块**存储器内相差 `0x4000` 的两个地址 MUST 不别名」的判别性探针 |
| 2 | 🔴 **「PC 复位值与 base 不一致」是伪命题**：`if_stage.h:71` 与 `address_decoder.h:33` **本来就都是 `0x80000000`** | A.1④ 重写；**「改 PC 复位值」这个 work item 整个删除**（它会改全部 `dbg_id_pc` 期望值却不带来任何交付） |
| 3 | `address_decoder.h` 的既有映射 = **ITCM `0x80000000`/64KB · DTCM `0x80010000`/64KB** ⇒ 与原稿 L1 冲突；且**区域 size ≠ TCM 容量**（16 KB）⇒ 区域内超容量会**静默回绕** | A.1⑧ 补记；L1 明确「size **由 `1<<kTcmAddrWidth` 派生**」；映射取值列入**待裁决** |
| 4 | 🔴 **「越界 ⇒ `CHFATAL` 报错」在硬件上物理不可实现**（`describe()` 里拿 `ch_in` 的值判定 ⇒ 建图期恒假 ⇒ 永不触发） | L1 明确**拆成两件事**：host 侧检查（`CHFATAL` + 非零退出 · 可机械验收）＋ RTL 侧只能给**错误信号/端口**；并**禁**在 `describe()` 里判硬件值 |
| 5 | 「编码器已有 3 份」**夸大**（全仓只有 1 份；第 3 份在仓外 `/tmp`） | A.1⑥ 订正 ⇒ L3 的实际改动 = **一处迁出**（不是「大 diff」） |
| 6 | `rv32i_phase{1,2,3}_test` 是 `riscv_mini_unit_test` ⇒ **无 `add_test`** ⇒ 不在 257 里、**从不执行** | A.1 补记 ⇒ 它们**不是**回归证据，也**不是**「会被破坏」的对象 |
| 7 | 交叉校验常量若搬进库里 ⇒ **变成自证** | A.1⑦ 补记：交叉校验**必须留在消费者测试**（含逐字手算字面量）+ 另加**逐字段**断言 + **变异负例** |
| 8 | 验收引用 `/tmp/opencode/...` ⇒ **不可验收**（仓外 · CI 不可复现） | 该验收项**已删** ⇒ 改由 `examples-rtl-sim-coverage` 把 TB 仓库化后再作证据 |
| 9 | 「257 个 ctest 全绿」**自相矛盾**（本项会加测试 ⇒ 数字即过期） | 改为**引用 `CTEST_TOTAL` 锚点** |
| 10 | 缺连带修改清单：`rv32i-commit-path` + `verilator-rv32i-e2e` 的 `prog_*` requirement · `verify_no_duplicate_rv32i_headers.sh:164` 的 C6 快照 · `include/cpu/AGENTS.md` 的头文件计数（C5） | 登记为 Impact 的必办项（见正文 Capabilities / Impact） |

### ⚠️ 待用户裁决

1. ✅ **已裁决（2026-11-10 · 用户「裁决②」）**：**地址映射取值** ⇒ ⇒ **沿用** `address_decoder.h` 的 `ADDR_ITCM_BASE = 0x80000000` / `ADDR_DTCM_BASE = 0x80010000`（**不**改成 `0x8000_4000`）⇒ ⇒ ⇒ ⚠️ 但**声明与实现不一致**（address_decoder 声明 64 KB · TCM 实际 **16 KB**）⇒ ⇒ ⇒ **截断等价**要求 **16 KB 对齐**（`0x80010000 % 0x4000 == 0` ✓ · `0x80005000 % 0x4000 == 4096` ✗ ⇒ ⇒ 4 KB 对齐的 static_assert **无判别力**）⇒ ⇒ ⇒ 详见 §L1 映射表
2. ✅ **已裁决（2026-11-11 · 用户拍板）**：**CPU 看到的是「区域内偏移」** ⇒ ⇒ CPU 侧的 `SW/LW 0x100(x0)` **保持**区域内偏移 ⇒ ⇒ **零固件重算** ⇒ ⇒ 绝对地址的代价面（每条访存指令都要 `LUI` 出基址 ⇒ `0x8000_4100` 装不进 I 型 12 位立即数 ⇒ 全部固件与 golden 重算）**不发生**。<br>⇒ ⇒ ⚠️ **两种地址约定并存（MUST 写进契约）**：<br>　① **CPU 侧** = **区域内偏移**（SoC 层负责加基址）⇒ ⇒ 管线 / 固件 / golden **零改动**；<br>　② **Loader 侧**（`prog_addr`）= **绝对字节地址**（loader 必须能跨 I-TCM 与 D-TCM 两个区域 ⇒ 必须有绝对地址才能被 L1 的映射表路由）⇒ ⇒ ⇒ **两者是不同边界上的不同约定，不是矛盾** ⇒ ⇒ 契约 MUST 分别写明。
3. ✅ **已裁决（2026-11-11 · architect 裁 · 用户提问澄清）**：**不新增加载端口** —— 只改 `prog_addr` 的**语义**。<br>**推导（实测）**：① 现状 `prog_we`/`prog_addr`/`prog_data` **直连 `InstrTCM`**（`examples/riscv-mini/src/rv32i_pipeline_top.h:113-115`）⇒ ⇒ **D-TCM 零加载通路** ⇒ ⇒ 「纯代码、零初始化数据」的程序能装、`int a = 5;` **装不进去**（`.data` 落在 D-TCM）—— 这才是「统一加载口」的真实动因；② 用户的模型（「TB 先加载好固件、再启动仿真」）**正是** loader 的工作流（L4 `tools/rv32i-fw-dump` + L5 ELF 装载）⇒ ⇒ 模型无误；③ ⚠️ 原稿 L2 之所以写「端口数量变化」，是因为它把 **`size` 当成了独立端口**（`(we, addr[byte], data[word], size)`）⇒ ⇒ 但 **`size` 可由 `addr[1:0]` 推出**（AXI `AWSIZE` 同款约定；仓内 `DataTCM` 的 `strbe` 本来就靠 `addr[1:0]` 推车道，`include/cpu/pipeline/rv32i_tcm.h:80-86`）⇒ ⇒ **端口数量可以不变**。<br>⇒ ⇒ **落点**：`prog_addr` 语义由「I-TCM 字索引」改为「**绝对字节地址**」+ SoC 侧按 L1 映射表译码到 I-TCM / D-TCM + 由 `addr[1:0]` 推 `strbe`（`DataTCM` 已有 `strbe` 端口 ⇒ 不用加）。<br>⇒ ⇒ ✅ **连带收益**：**ADR-046 D10 的「32 端口冻结」不受影响**（数量不变）⇒ ⇒ **不需要 `Rv32iSocTopV2`** ⇒ ⇒ 只走「**行为变更（非符号移除）**」公开面登记（`prog_addr` 语义变 ⇒ 下游 TB MUST 更新）。<br>⇒ ⇒ ⚠️ **附带契约**：I-TCM **无** `strbe` ⇒ ⇒ **I-TCM 区只支持整字写**（`.text` 本就字对齐）⇒ ⇒ loader MUST 对该区用整字写。

## critic 第 1 轮（plan, build）—— **REJECT**（随整体）· mergeable: no · scope_exceeded: false

> **轮次**：1/2 · 审查者：critic

### ✅ 「硬件零改动」这一半**经 critic 验算成立**

`0x8000_0000 & 0x3FFF == 0`、`0x8001_0000 & 0x3FFF == 0`（两基址均 **16 KB 对齐**）⇒ `bits<13,2>` 截断 **≡** `(addr-base)>>2` ⇒ **硬件零改动**成立。

### ⚠️ SHOULD_FIX（已采纳）：但「golden 增量 ≈ +1 退休 / +1 写回」**低估了影响面**

| 影响面 | 证据 |
|---|---|
| golden 里**所有地址字面量断言**都会变：`has_store(t, 0x100/0x104/0x108/0x101/0x122, …)` 比较的是 `kIdxDataAddr` ⇒ 从 `0x100` 变为 `0x80010100` 等 | `test_rv32i_soc_dual_parity.cpp:570-576` |
| `dbg_exmem_alu_result` / `obs_data_addr` 同理 | 同上 |
| 需**验证**（非假定）新增 `LUI`→`SW(base)` 的 RAW 已被前推档位 1 覆盖 | 否则 `load_use_stalls` / `control_stalls` 也会变 |

⇒ ⇒ **这不是「+1/+1」，而是地址字面量整体重写** ⇒ ⇒ 直接动摇「golden 只重算一次」这条排序依据。
**处置**：验收 MUST 列「受影响字面量清单」（`grep -n "has_store(t, 0x"`），而非只报计数增量。

### ⚠️ SHOULD_FIX（已采纳）：①（截断等价）MUST 补**最小判据**

这是整个 B2 golden 策略的**承重前提**，且依赖「基址 ≥16KB 对齐」这一**未来可被改动**的常量（`address_decoder.h:33-34` 一旦改成非对齐值即静默失效）。
**处置**：① 一条定点用例断言

### ⚠️ SHOULD_FIX（已采纳）：L1 表仍写 `dtcm = 0x8000_4000`，与决策②冲突

同一 change 的 Map 表与实际取值不一致 ⇒ 实施方无法判断。**处置**：按 metis#3 的订正改表（沿用 `0x80010000`）。
