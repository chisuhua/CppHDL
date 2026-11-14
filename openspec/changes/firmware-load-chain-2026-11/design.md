# Design: 固件装载链路（显式映射 + **内存初值装载** + 转储器 / ELF 装载）

> **变更提案** = `proposal.md` · **架构依据** = `docs/adr/ADR-046-*`（SoC 顶层归属 / D10 端口契约）
> ⚠️ **与 `memory-timing-contract` 同属 B3** ⇒ ⇒ MUST 一并实施
> （装载路径要写 D-TCM ⇒ ⇒ D-TCM 的读时序 MUST 先定 ⇒ ⇒ **先做 memory-timing-contract**）。
> ⚠️ **与 R17 的耦合**：R17 已把 `Rv32iPipelineTop` 的 32 个端口具名
> ⇒ ⇒ ~~本项新增 / 改语义的加载端口 MUST 一并具名~~
> ⇒ ⇒ **2026-11-14 用户裁决：L2 改走「路线 A」（内存初值）⇒ ⇒ 本项零新增端口**
> ⇒ ⇒ 该耦合条件因此**失效**（不是放宽，是对象不存在）。
>
> ## ⚠️ **L2 的方案变更（2026-11-14 · 用户裁决 · 记录在案）**
>
> 原设计 = **统一加载端口** `(we, addr[字节], data[size])` ⇒ ⇒ 译码到 I-TCM / D-TCM。
> ⇒ ⇒ 实施前实测出两条**硬约束**：
> 1. **D-TCM 不能加第二个 `ch_mem` 写端口** —— `print_mem_write_port` 每个写端口各发一个
>    `always_ff` 写同一数组 ⇒ ⇒ 两个端口 = Verilog **MULTIDRIVEN**（verilator 直接报错）
>    ⇒ ⇒ 只能做地址 / 数据 / 使能 **mux** 复用现有端口；
> 2. **D-TCM 的 `addr` 被读路径共用**（`sread` / `aread` / `write` 同一端口）
>    ⇒ ⇒ 装载期改它 = 同时改**流水线的读地址** ⇒ ⇒ 只在「装载时流水线在复位」这个**隐式时序前提**下安全。
>
> ⇒ ⇒ 用户裁决：**装载是发射期的事，产物自足** ⇒ ⇒ 改走 **路线 A**：
> 固件作为 `ch_mem::init_data` 传入 ⇒ ⇒ 产物自带 `initial begin … end`
> ⇒ ⇒ **TB 零装载动作** · **零端口变更**。
>
> ⇒ ⇒ 路线 B（运行期装载口）**移出本 change** ⇒ ⇒ 它本质是「**可装载性接口**」（真机 bring-up /
> 调试口），不是「固件装载链」⇒ ⇒ 若需要 ⇒ ⇒ 独立立项（并且 MUST 先解决上面两条约束）。

## 1. 实施顺序

```
L1（显式映射 + 越界诊断）
  ⇒ L2（**内存初值装载** · 路线 A ⇒ 零端口变更）
    ⇒ L3（编码器库化）
      ⇒ L4（转储器 CLI）
        ⇒ L5（ELF 装载）
          ⇒ L6（读回可观测性 · 判据）
```

**为什么这个顺序**：
- **L1 先于 L2** —— 初值镜像的**基址 / 尺寸**依赖映射表（越界 MUST 报错）。
- **L2 与 L1 不再需要「同批」** —— 原约束的理由是「二者都改顶层端口契约」⇒ ⇒ 路线 A **不改端口**
  ⇒ ⇒ 该约束**随之失效**（2026-11-14 裁决）。
- **L3 先于 L4** —— 转储器的输入是**固件清单**⇒ ⇒ 编码器归一后才有一份可信的编码来源。
- **L6 最后** —— 读回判据依赖装载链路已通。

## 2. L1 —— 显式映射表

```cpp
// examples/riscv-mini/src/rv32i_pipeline_top.h（示意）
struct MemRegion { uint64_t base; uint64_t size; };
// ⚠️ size MUST 由容量派生（MUST NOT 手写 16 KB 之类的字面量）
static constexpr MemRegion kItcm{ 0x8000_0000ull, 16ull * 1024ull };
static constexpr MemRegion kDtcm{ 0x8001_0000ull,
                                   1ull << (kTcmAddrWidth + 2) };
```

⚠️ **`dtcm.base == 0x8001_0000`**（沿用 `include/cpu/riscv/address_decoder.h:34`）
⇒ ⇒ 且 **PC 复位值 MUST 由 `kItcm.base` 派生**（禁第二个字面量）。

⚠️ **对齐要求是 16 KB 而不是 4 KB** ⇒ ⇒ 「截断等价的最小判据」MUST 按 **16 KB** 取。

⇒ ⇒ **越界 ⇒ 诊断**（`CHFATAL` / 诊断通道）⇒ ⇒ **禁**静默丢高位。
⇒ ⇒ ⚠️ **受影响地址字面量清单**（改绝对地址后需重推）：`grep -n 'has_store(t, 0x'` 逐条登记
⇒ ⇒ **禁**只报「计数增量」。

## 3. L2 —— **内存初值装载**（路线 A · 2026-11-14 用户裁决）

**机制**（全部复用既有能力，**零新机制**）：

```
固件（.text / .data 两段字镜像）
  ⇒ TCM 的构造参数（InstrTCM / DataTCM 的 init_words）
    ⇒ ch_mem(init_data, …)                     ← 既有
      ⇒ print_mem_init() → initial begin … end ← 既有（src/codegen_verilog.cpp:1634）
        ⇒ 产物自带初值 ⇒ TB 零装载动作
```

- **零端口变更**：SoC 的 `__io` **一字未动** ⇒ ⇒ 顶层端口表 MUST **逐字相同**（判据 = 产物级 diff）。
- **空镜像（默认）= 零行为变化**：走同一条 `init_memory(…, {})` 路径 ⇒ ⇒ 基线产物**一条 init 赋值都没有**。
- ⚠️ **`init_data` 的口径 = 「字镜像」（下标 = 字索引）**
  ⇒ ⇒ `.data` 的字节地址 0x100 ⇒ 字索引 **64**。
- ⚠️ **越界仍 MUST 报错**：镜像长度不得超过 TCM 深度（L1 的支配范围）。

⇒ ⇒ **不在本 change**：运行期装载口（不重发射即可灌固件）= 「**可装载性接口**」独立立项
⇒ ⇒ 它的前置条件是解决「D-TCM 写端口 MULTIDRIVEN」与「装载期流水线必须在复位」两条约束（见文首）。

## 4. L3 —— 编码器库化

- 新增 `include/cpu/riscv/rv32i_asm_encode.h`（header-only · `constexpr`）
  ⇒ ⇒ 既有 **3 处副本**归一为 **1 份**（`enc_r` / `enc_i` 等定义**唯一**）。
- ⚠️ **保留 `FirmwareEncoderCrossCheck`**（手算常量 vs helper 产出）
  ⇒ ⇒ **防止「库里的编码器自证」**（自证 = 无判别力）。

## 5. L4 / L5 —— 转储器与 ELF

```
tools/rv32i-fw-dump --in <fw.json|elf> --format vh|mem|manifest --out <path>
```

- ELF：解析 header + `PT_LOAD` ⇒ ⇒ `p_paddr` / `p_filesz` / `p_memsz`
  ⇒ ⇒ 按映射写入 ⇒ ⇒ `.bss` 补零。
- ⚠️ **非零重定位 ⇒ 报错**（不是静默忽略）。
- ⚠️ **不解析符号表** ⇒ ⇒ 符号留给未来工具。
- ⚠️ `tools/` 下的二进制 **MUST NOT 链 `examples/**`**（沿用 `rtl-emit-product-tooling` C1 的纪律
  ⇒ ⇒ 工具自测载体独立）。

## 6. L6 —— 装载可观测性

- 装载后**走真实读通路**读回某地址 ⇒ ⇒ TB 断言「装进去的值 == 期望」。
- ⚠️ **不是**「loader 返回成功」⇒ ⇒ 后者对「装错地方」零判别力。

## 7. 验证策略

| 层 | 判据 |
|---|---|
| 映射 | 区域 `{base, size}` 显式 · size 由容量派生 · `dtcm.base == 0x8001_0000` |
| 映射 | **越界投毒自证**：地址设到区域外 ⇒ ⇒ **MUST 有诊断** |
| 装载 | 带 `.data` 的固件可装并**读回**（走真实读通路）⇒ ⇒ 值 == 期望 |
| 装载 | **产物级**：带初值产物 `initial` 块逐字 == 手推镜像 · 基线产物**零** init 赋值 · 端口表逐字相同（`scripts/verify_rv32i_fw_init_rtl.sh` F2–F5） |
| 装载 | `.bss`（`memsz > filesz`）**补零** ⇒ ⇒ 旧内容不留 |
| 装载 | 非零重定位 ⇒ ⇒ **报错**（不是静默忽略） |
| 编码器 | `rv32i_asm_encode.h` 存在 · 3 处副本归一为 1 份 · `FirmwareEncoderCrossCheck` **仍在**且通过 |
| 编码器 | 新头文件有**仓内消费者**（既有测试 + 新工具）⇒ ⇒ 不违反 ZERO-DEBT |
| 工具 | `tools/rv32i-fw-dump` 独立可运行 · 三种输出格式 |
| 不回归 | `ctest -N` 总数不变或有据变更 · 计数锚点同步（用 `scripts/update_test_count_anchors.sh`） |

## 8. 风险与回滚

| 风险 | 缓解 |
|---|---|
| 改 `prog_addr` 语义 ⇒ ⇒ **下游 TB 全断** | 公开面登记**前置**（R6 已做）⇒ ⇒ 迁移指引写明「字索引 → 字节地址」 |
| 地址字面量遗漏 ⇒ ⇒ 某处仍按旧语义 ⇒ ⇒ 静默装错 | 逐条登记清单（禁只报计数增量）· 越界投毒自证 |
| 编码器归一 ⇒ ⇒ 手算交叉校验被「顺手删掉」 | `FirmwareEncoderCrossCheck` **保留**是硬 AC |
| 工具链 `examples/**` ⇒ ⇒ 污染产品面 | 工具自测载体独立（沿用 C1 纪律） |
| 回滚 | 本 change 独立 ⇒ ⇒ 可整体 revert（端口契约随之还原） |
