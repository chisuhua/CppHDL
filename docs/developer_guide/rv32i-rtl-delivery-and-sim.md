# RV32I 核的 RTL 交付与 Verilator 仿真环境

> **一句话**：CppHDL 能把 RV32I 核生成成**可交付的 RTL 产物**（扁平 / 层次化两种形态），
> 并用**同一个独立 Verilog TB** 在 Verilator 上跑出**同一组判据** ⇒ 一条命令即可复现。
>
> ⚠️ **本文只写「产物形态」与「仿真环境」**；微架构（数据通路 / 冒险前推 / 存储层次 /
> 契约归属）**不在此复制** ⇒ ⇒ 唯一权威 =
> [`docs/architecture/rv32i-microarchitecture.md`](../architecture/rv32i-microarchitecture.md) ·
> capability `rv32i-isa-semantics` / `rv32i-privilege` · ADR-046 / 047 / 069 / 072。
> ISA 覆盖与**未覆盖**（特权级）**唯一真相 = 根 `AGENTS.md` 的 §KNOWN LIMITATIONS**。

---

## 1. 交付物清单

| # | 交付物 | 位置 | 说明 |
|---|---|---|---|
| ① | **通用生成器**（库 + 薄 CLI） | `include/rtl_emit/cli.h` · `src/rtl_emit/cli.cpp` | 用户在自己的可执行文件里登记设计 ⇒ `run_emit_cli` |
| ② | 扁平 RTL 发射入口 | `examples/riscv-mini/tb_rv32i_soc.cpp` → `tb_rv32i_soc --emit-rtl` | `toVerilog(path, root, opts)`（带根入口） |
| ③ | **层次化** RTL 发射入口 | 同上二进制 · `tb_rv32i_soc --emit-rtl --hierarchical` | `--hierarchical` ⇒ `toVerilogHierarchical` |
| ④ | 产物清单 | `<out>/manifest.json`（schema `rtl-emit-manifest-v2`） | `port_table[]` = **产品端口面的唯一权威** |
| ⑤ | 独立 Verilog TB | `examples/riscv-mini/tb_rv32i_soc.sv` | 装载自己的小程序 ⇒ 期望值**全由 ISA 语义手推** |
| ⑥ | **仿真环境**（一条命令） | `scripts/rv32i_rtl_sim_env.sh` | 生成 → lint → 编译 → 运行 ⇒ 打印 `RESULT: checks=N fails=0` |
| ⑦ | 固件转储器 / ELF 装载 | `tools/rv32i-fw-dump` | ELF32 + `PT_LOAD` + `.bss` 补零 ⇒ 三格式（`vh` / `mem` / `manifest`） |
| ⑧ | 覆盖登记 + 护栏 | `.rddf/checks/examples-rtl-sim-targets.tsv` · `scripts/verify_examples_rtl_sim.sh` | 四件套（manifest + 产物 + 独立 TB + 登记）逐项断言 |

---

## 2. RTL 设计（产物形态）

### 2.1 顶层端口面（34 个）

**顶层端口表 = 框架 2 + 根组件自有 IO**（ADR-072 D1 收敛）⇒ ⇒ 后代组件的 `ch_out`
一律降为**体内 `logic`**，**不**上顶层 ⇒ ⇒ 端口面**与实现无关**（改内部结构不改端口面）。

| 方向 | 端口 | 位宽 | 含义 |
|---|---|---:|---|
| in | `default_clock` | 1 | **真时钟源**（框架注入） |
| in | `default_reset` | 1 | 复位（**高有效**） |
| in | `top_ext_rst` | 1 | SoC 复位（**低有效**语义：拉低 ⇒ 流水线复位） |
| in | `top_prog_we` | 1 | 固件写使能（**只**接 I-TCM） |
| in | `top_prog_addr` | 32 | 固件写地址（**字地址**） |
| in | `top_prog_data` | 32 | 固件写数据 |
| out | `top_obs_instr_addr` | 32 | 取指字节地址 |
| out | `top_obs_data_addr` | 32 | 访存字节地址 |
| out | `top_obs_data_write_data` | 32 | 写数据 |
| out | `top_obs_data_write_en` | 1 | 写使能 |
| out | `top_obs_data_rdata` | 32 | D-TCM 读数据（**0-cycle** 组合读） |
| out | `top_obs_perf_cycles` | 48 | 周期计数 |
| out | `top_obs_perf_instructions` | 48 | 退休指令数 |
| out | `top_obs_perf_branch_count` | 32 | 分支计数 |
| out | `top_obs_perf_branch_mispredict` | 32 | 分支误预测计数 |
| out | `top_obs_perf_load_use_stalls` | 32 | load-use 停顿拍数 |
| out | `top_obs_perf_control_stalls` | 32 | 控制冒险停顿拍数 |
| out | `top_dbg_id_instr` / `top_dbg_id_pc` | 32 / 32 | ID 级指令 / PC |
| out | `top_dbg_id_valid` | 1 | ID 级有效 |
| out | `top_dbg_id_is_branch` / `_is_load` / `_is_store` | 1 ×3 | EX 级那条指令的类型 |
| out | `top_dbg_branch_taken` | 1 | 分支判定 |
| out | `top_dbg_wb_write_en` / `top_dbg_wb_write_data` / `top_dbg_wb_rd_addr` | 1 / 32 / 5 | 写回事件 |
| out | `top_dbg_exmem_alu_result` / `_imm` / `_rs2_data` | 32 ×3 | EX/MEM 级中间量 |
| out | `top_dbg_exmem_valid` / `top_dbg_mem_valid_out` | 1 ×2 | 有效链 |
| out | `top_dbg_load_use_detected` | 1 | load-use 冒险检测 |
| out | `top_dbg_illegal_reason` | 2 | **0** 合法 · **1** 已知未实现 · **2** 未知 opcode · **3** 保留组合 |

⚠️ **端口名规则** = `top_<C++ 成员名>` ⇒ ⇒ 顶层端口与 C++ 侧**逐字对应**
⇒ ⇒ TB 直接按名连线，**不需要查 port map**。

### 2.2 模块树（层次化形态 = 8 个 module）

```
top
├── IfStage              取指（skid buffer + PC）
├── IdStage              译码 + 立即数（立即数 = Rv32iDecoder）
├── ExStage              执行（算术核心 = Rv32iAlu）
├── MemStage             访存
├── WbStage              写回
├── HazardUnit           冒险检测
└── ForwardingMux_32_    数据前推 mux（chlib::ForwardingMux<32>）
```

⚠️ 成层粒度 = **实例级 `ch_layer`**（ADR-070 D2）⇒ ⇒ 「成层」是**声明**出来的
（`ch_layer<T>` / `CH_HIERARCHY_LAYER`）⇒ ⇒ 默认 inline。

⚠️ **观测盲区**：成层子树的 `ch_out` **不在**顶层 ⇒ ⇒ 顶层读不到
（「子树内部算错但顶层输出恰好相同」这类缺陷**结构性测不出**）⇒ ⇒ 逐拍等价性
**只覆盖顶层可见行为**（输入方向**无盲区**）。

### 2.3 存储映射与装载链路

| 区域 | 基址 | 容量 | 用途 |
|---|---|---|---|
| I-TCM | `0x8000_0000` | **16 KB** | 取指（`sread` = 1 拍）+ 固件装载 |
| D-TCM | `0x8001_0000` | **16 KB** | 数据（`aread` = 0 拍组合读） |

- **PC 复位值 = I-TCM base**（`0x8000_0000`）⇒ 由映射**派生**（`include/cpu/riscv/rv32i_mem_map.h` 单一真相链）
- ⚠️ **地址单位**：流水线侧 `instr_addr` / `data_addr` 是**字节地址** ⇒ ⇒ TCM 索引**右移 2 位**
- ⚠️ **高位截断**：TCM 索引只有 12 位 ⇒ ⇒ 固件与数据**必须**落在 16 KB 内
  （字地址 < `0x1000`）⇒ ⇒ 越界**静默截断**、**零报错**
- ⚠️ **`prog_*` 只接 I-TCM** ⇒ ⇒ 带 `.data` 的程序**装不进去** ⇒ ⇒ 用「路线 A」
  （`ch_mem::init_data` ⇒ 产物里出现 `initial begin mem[i] = … end`），
  或让固件自己用 `sw` 写

### 2.4 时钟与复位

- **真时钟源 = `default_clock`**（框架注入）⇒ ⇒ TB 里接 `clk` 即可
- ⚠️ **`ext_clk` 已被删除**（ADR-046 R8 改判 · 用户裁决 A2）⇒ ⇒ 它是**死端口**
  （被赋值但零消费者）⇒ ⇒ 「SoC 由 `ext_clk` 驱动」这个直觉**是错的**
- 复位：`default_reset`（**高有效**，框架）+ `top_ext_rst`（SoC 级）

### 2.5 ISA 边界

- **已覆盖**（用户态 RV32I 全集 + `FENCE`）⇒ ⇒ 唯一真相 = 根 `AGENTS.md` §KNOWN LIMITATIONS
- 🔴 **未覆盖 = 特权级**：`CSR` 指令 / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口接线
  ⇒ ⇒ capability `rv32i-privilege` 目前只是**范围声明**（组件 `ExceptionUnit` 存在但**未接线**）
  ⇒ ⇒ **需要独立立项**（ADR-072 判为 G2）
- ⚠️ **非法指令是「冻结」不是「当 NOP」**：一进 ID 级即**冻结**（PC 不前进 · 永不退休）
  ⇒ ⇒ 固件**尾部必须填 NOP**（`0x00000013`）⇒ ⇒ 否则跑到未初始化字就挂住

---

## 3. Verilator 仿真环境

### 3.1 一条命令

```bash
bash scripts/rv32i_rtl_sim_env.sh
```

输出（实测）：

```
── ［flat］① 生成 RTL 产物
   module 数：1（top)
   顶层端口：34（manifest port_table[] = 34）
✅ [flat] lint 干净（退出码 0 · 零 %Error）
✅ [flat] RESULT: checks=24 fails=0
── ［hier］① 生成 RTL 产物
   module 数：8（IfStage IdStage ExStage MemStage WbStage HazardUnit ForwardingMux_32_ top)
   顶层端口：34（manifest port_table[] = 34）
✅ [hier] lint 干净（退出码 0 · 零 %Error）
✅ [hier] RESULT: checks=24 fails=0
✅ rv32i_rtl_sim_env: PASS
```

⚠️ **两种形态跑出同一组判据** ⇒ ⇒ 「层次化 ≡ 扁平」在**产品路径**上得到证据
（不只在 `tests/test_hierarchy_equivalence.cpp` 的用例路径上）。

### 3.2 手动四步

```bash
# ① 生成（形态二选一）—— ⚠️ **发射入口 = TB 自身**（`--emit-rtl` 模式）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl                 --out out/flat --emit-hygiene
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical   --out out/hier --emit-hygiene

# ② lint
verilator --lint-only --top-module top out/hier/top.v

# ③ 编译独立 TB
verilator --binary --timing -CFLAGS -fcoroutines --x-assign unique \
          --top-module tb_rv32i_soc -Mdir out/obj \
          examples/riscv-mini/tb_rv32i_soc.sv out/hier/top.v -o sim

# ④ 运行
./out/obj/sim            # ⇒ 末尾打印 RESULT: checks=24 fails=0
```

### 3.3 判据与期望源

| 面 | 内容 |
|---|---|
| TB 的固件 | TB **自带**小程序（7 个字：2×`addi` + `nop` + `add` + `sw` + `jal` 自环）⇒ ⇒ **不依赖任何仓内金标** ⇒ ⇒ 改金标时不会红 |
| 期望值 | **手推**（ISA 语义）⇒ ⇒ 期望源登记 = `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` ⇒ ⇒ **禁抄仿真输出** |
| 判据数 | `checks=24`（6 个语义场景：复位 PC / 每个字都被取到 / pc +4 / 写回 x1=42·x2=11·x3=53 / 写存储 53@0 / 退休计数单调） |
| **C++ 侧** | **同 24 条判据**的**独立实现** = `build/examples/riscv-mini/tb_rv32i_soc`（默认模式 · 解释器路径）⇒ ⇒ 两侧**互为跨后端佐证**（期望值**同一份登记**）⇒ ⇒ ⚠️ 「两后端一致」**不是**正确性证据 ⇒ ⇒ 期望值 MUST **手推** |
| 护栏 | `scripts/verify_examples_rtl_sim.sh`（四件套 + 形态判据 ⑧'）+ `--self-test`（8 条投毒各自转红） |

### 3.4 固件怎么进产物（三条路，SoC 只能走第一条）

#### 路线 A —— `init_data`（**SoC 的唯一可行路径**）

固件作为 TCM 的 `ch_mem::init_data` **构造参数**传进去 ⇒ ⇒ 产物里自动出现
`initial begin mem[i] = … end` ⇒ ⇒ **TB 零装载动作** · **顶层端口表一字不变**。

```bash
# ① ELF → 字镜像（每行一个 8 位 hex）
build/tools/rv32i-fw-dump --in fw.elf --format mem --out fw.mem

# ② 把字镜像塞进 SoC 构造参数（你的发射器里）
auto itcm_init = load_hex_words("fw.mem");     // → std::vector<uint32_t>
return std::make_unique<Rv32iPipelineTop>(nullptr, "top", itcm_init, /* dtcm */ {});
```

> 仓内示例 = `examples/riscv-mini/tb_rv32i_soc.cpp` 的 **`--emit-rtl --firmware-init` 模式**
> （固件**硬编码**在 `examples/riscv-mini/src/rv32i_fw_demo.h` · 手推的字镜像）⇒ ⇒
> 产物级判据 = `scripts/verify_rv32i_fw_init_rtl.sh`（F0–F5）· 固件链路判据 =
> `scripts/verify_rv32i_fw_dump.sh`（D0–D6）。
> ⚠️ **层次化形态暂无 fw_init 变体**（本次不做）⇒ ⇒ 需要时按同一模式加一个即可
> （`run_emit_cli` 的 `--hierarchical` 与 `init_data` 正交）。

#### 路线 B —— `$readmemh`（**SoC 用不了**）

`rv32i-fw-dump --format vh` 产出的是 `$readmemh("<file>", <数组名>)` 片段 ⇒ ⇒
**数组名由 TB 提供** ⇒ ⇒ 只适用于「**mem 暴露在 TB 可见位置**」的设计。

⚠️ **SoC 的 TCM 在 DUT 内部** ⇒ ⇒ TB **不可达** ⇒ ⇒ 路线 B 对本 SoC **无效**
⇒ ⇒ 固件必须走路线 A。

#### 装载口（`prog_*`）—— 第三条路

顶层的 `prog_we` / `prog_addr` / `prog_data` **只**接 **I-TCM** ⇒ ⇒
TB 可以在复位期逐字写入（仓内 TB 就是这么做的 ⇒ 期望值全由 ISA 语义手推）。
⇒ ⇒ 但它**装不了 `.data`**（D-TCM 无装载口）⇒ ⇒ 带 `.data` 的程序仍需路线 A。

### 3.5 常见陷阱（实测踩过的）

| # | 陷阱 | 后果 | 规避 |
|---|---|---|---|
| 1 | 给 `ext_clk` 接线 | 该端口**已删** ⇒ `PINMISSING` **致命** | 时钟接 `default_clock` |
| 2 | 按 `top_m_unnamed_output_1` 这类名字接线 | D1 后**不再是**顶层端口 ⇒ `PINNOTFOUND` | 用 `top_<成员名>`；以 manifest `port_table[]` 为准 |
| 3 | 固件地址超 16 KB | **静默截断**（高位丢）⇒ 零报错 | 字地址 < `0x1000` |
| 4 | 固件尾部不填 NOP | 跑到未初始化字（= 0 = 未知 opcode）⇒ **冻结挂住** | 尾部填 `0x00000013` |
| 5 | 依赖 TB 里的 `$isunknown` | Verilator 会把 X **优化成已知值** ⇒ 恒 false ⇒ **装饰性断言** | 用内容判据（§3.3） |
| 6 | 忘了驱动某个输入 | 读到**未定义值**（实测可能是全 1 / 0）⇒ 测试「碰巧」过 | 逐条核对端口驱动表 |
| 7 | 用 `prog_*` 装 `.data` 段 | `prog_*` **只**接 I-TCM ⇒ `.data` 装不进去 | 路线 A（`init_data`）或固件自己 `sw` |
| 8 | 直接抄仿真输出当期望值 | **循环论证** ⇒ ⇒ 缺陷一起错 | 期望值**手推** |
| 9 | 看到产物里有**空的** `initial begin end` 块 | **不是**缺陷 ⇒ ⇒ 那是 `ch_mem` 的初值位（`init_data` 为空 ⇒ 无赋值）⇒ ⇒ 用「路线 A」装固件时它会**被填上** | `grep -n -A2 '^\s*initial' top.v` |

---

## 4. 换成你自己的设计

```cpp
// 你的可执行文件里：
#include "rtl_emit/cli.h"

const ch::rtl_emit::DesignFactory factory =
    [](ch::core::context *) -> std::unique_ptr<ch::Component> {
    return std::make_unique<MyTop>(nullptr, "top");   // 根组件 · 名字即顶层 module 名
};

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    args.emplace_back("--hierarchical");   // 可选：层次化形态（默认扁平）
    return ch::rtl_emit::run_emit_cli(args, factory);
}
```

```bash
./my_emitter --out out --emit-hygiene
```

⇒ ⇒ 产物形态（`top.v` + `manifest.json`）与仓内**完全一致** ⇒ ⇒ 你的 TB 也能按
`port_table[]` 里的名字连线 ⇒ ⇒ **不需要**改任何东西。

⚠️ **层次化形态的两条硬约束**（发射器会 fail-closed 拒绝 ⇒ 退出码 1）：
① 成层组件**不自驱动** `ch_in`（后代悬空输入恒被拒绝 · ADR-072 D1-f）
② **跨模块 `ch_mem`** 被拒（mem 本体不得跨 module）

---

## 5. 与 CI / 护栏的关系

| 判据 | 载体 | 触发 |
|---|---|---|
| 四件套（manifest `port_table[]` == 产物端口表 · 逐字逐序） | `scripts/verify_examples_rtl_sim.sh` ⑧ | CI（`pr-verilator-fast` / `nightly-verilator`） |
| 层次化**形态**（≥ 2 module + 根 `top`） | 同上 **⑧'**（判别力自证 = 投毒 8 `flatten_hier`） | 同上 |
| 登记守恒（`28 == 10 ∪ 18` · 三桶） | `scripts/verify_examples_rtl_sim_coverage.sh` | CI（无 verilator 依赖） |
| 产物级 lint / 确定性 / module 树同构 | `scripts/verify_hierarchical_rtl.sh` R1–R8 | CI（`hier_rtl_products` 语料） |
| 扁平产物卫生产物 | `scripts/verify_flat_rtl_emit.sh` F1–F8 | CI |
| 固件初值链路 | `scripts/verify_rv32i_fw_init_rtl.sh` F0–F5 | CI（不需 verilator） |
| D-TCM 读口时序结构 | `scripts/verify_dtcm_read_timing.sh` W0–W4 | CI（不需 verilator） |
| **本仿真环境**（人工 / 本地） | `scripts/rv32i_rtl_sim_env.sh` | **未进 CI**（本次不做：与护栏重复，且会让 CI 墙钟 +N） |
