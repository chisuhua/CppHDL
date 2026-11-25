# 在 `riscv-mini` 上跑 ELF 固件（用户指南）

> 适用对象：想把自己编译的 **RV32I ELF** 跑在核级（`Rv32iCoreTop`）或 SoC 级
> （`examples/riscv-soc` 的 `Rv32iSocTop`）上的使用者。
> 本文只讲「怎么跑」；端口契约与集成语义见 `AGENTS.md` 的「IP 集成契约」段。

## 0. 一句话链路

```
your.elf ──[rv32i-fw-dump --format mem-split]──> <dir>/{itcm,dtcm}.mem
         ──[tb_rv32i_soc | tb_riscv_soc  --firmware <dir>]──> 核取指 / 执行
```

**为什么要分两步**：核的固件走 `ch_mem::init_data`（**构造参数**）⇒ ⇒
**不进端口面** ⇒ ⇒ 带固件的产物与基线产物的顶层端口表**逐字相同**
（判据 = `scripts/verify_rv32i_fw_init_rtl.sh` 的 F2 · `scripts/verify_rv32i_elf_e2e.sh` 的 E4a）。

## 1. 前置条件

| 项 | 要求 | 检查命令 |
|---|---|---|
| 交叉工具链 | `riscv64-unknown-elf-gcc`（或 `riscv32-*`，能产 `rv32i` 即可） | `riscv64-unknown-elf-gcc --version` |
| 转储器 | `build/tools/rv32i-fw-dump`（`cmake --build build --target rv32i-fw-dump`） | `ls build/tools/rv32i-fw-dump` |
| 核级 TB | `build/examples/riscv-mini/tb_rv32i_soc` | `cmake --build build --target tb_rv32i_soc` |
| SoC 级 TB（可选） | `build/examples/riscv-soc/tb_riscv_soc` | `cmake --build build --target tb_riscv_soc` |

> ⚠️ 三条硬约束（后面 §5 逐条有表）：**入口 == `0x8000_0000`** · **无重定位** ·
> **代码/数据 MUST 落在两个 16 KB TCM 内**。违反任一 ⇒ `rv32i-fw-dump` **exit 3**（不猜、不截断）。

## 2. 三步跑通（实测过的完整例子）

### ① 写固件（汇编或 C）

```asm
# demo.S —— x1 = mem[0x100]（这就是 .data 里的 0x5A）；x2 = x1 + 1；mem[0x104] = x2
    .section .text.init
    .globl _start
_start:
    lw   x1, 0x100(x0)
    addi x2, x1, 1
    sw   x2, 0x104(x0)
1:  j 1b                    # 停机（自环）

    .section .data
    .align 2
    .word 0x5A
```

### ② 链接脚本（**地址布局由它决定**）

```ld
MEMORY {
  itcm (rx) : ORIGIN = 0x80000000, LENGTH = 16K   /* == riscv::kItcm */
  dtcm (rw) : ORIGIN = 0x80010100, LENGTH = 16K   /* == riscv::kDtcm.base + 0x100（避开 0 号字） */
}
SECTIONS {
  .text : { *(.text.init) *(.text*) } > itcm
  .data : { *(.data*) }               > dtcm
  .bss  : { *(.bss*) *(COMMON) }      > dtcm
}
```

> ⚠️ **入口 MUST 是 `0x8000_0000`**：核的 PC 复位值硬连线为 `riscv::kPcReset`
> （= `kItcm.base`）⇒ ⇒ 入口不在那儿 ⇒ 装出来**无法启动**（`rv32i-fw-dump` 会 exit 3 拦下）。
>
> ⚠️ **`.data` 不要放在 0x8001_0000**：数据侧字节地址 `0` 在核级程序里是合法目标
> （既有固件有 `sw x3, 0(x0)`）⇒ ⇒ 布局上避开 0 号字可减少「读 0 得到 .data 首字」的困惑。

### ③ 编译 + 转储 + 跑

```bash
W=/tmp/rv32i-fw && mkdir -p $W

# 1) 编译（-nostdlib：没有 libc ⇒ 自己写 _start）
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib \
    -T demo.ld -o $W/demo.elf demo.S

# 2) ELF → 两个按区域的初值镜像（行号 = 区域字索引 · 空洞补零）
./build/tools/rv32i-fw-dump --in $W/demo.elf --format mem-split --out $W/img
#   OK：装载 5 字 · 入口 0x80000000 · 区域 2 个 ⇒ /tmp/rv32i-fw/img/{itcm,dtcm}.mem

# 3) 核级跑（解释器路径）
./build/examples/riscv-mini/tb_rv32i_soc --firmware $W/img
#   FIRMWARE-TRACE: retired=14 fetch0=0x10002083 wb=1:0x5a,2:0x5b stores=0x104:0x5b
#   RESULT: checks=5 fails=0

# 4) SoC 级跑（10 条产品面判据不跑，只跑固件两条不变量）
./build/examples/riscv-soc/tb_riscv_soc --firmware $W/img
#   SOC-FIRMWARE-TRACE: store_count=1 writeback_count=2
#   RESULT: checks=2 fails=0
```

> 上面两行 trace 的**期望值来源**：`x1 = 0x5A`（.data）· `x2 = 0x5B`（+1）·
> `store 0x104 = 0x5B` —— 纯 ISA 语义推导，不抄仿真输出。
## 3. 三层跑法：该用哪一个

| 层 | 命令 | 能看到什么 | 适合判什么 |
|---|---|---|---|
| **核级** | `tb_rv32i_soc --firmware <dir>` | `dbg_*` / `obs_*` 全部内部观测 | ISA 语义 · 流水线行为 · 写回序列 · store 地址/数据 |
| **SoC 级** | `tb_riscv_soc --firmware <dir>` | 只有根端口（`store_count` / `writeback_count` / `gpio_out`） | 端到端「固件在 SoC 里真的在跑」 |
| **RTL 级** | `tb_rv32i_soc --emit-rtl --firmware <dir> --out <out>` | 产物 `top.v` 的 `initial` 块 | 让**独立 Verilog TB**（`tb_rv32i_soc.sv`）吃到同一份固件 |

> ⚠️ 三层里**核级与 SoC 级走解释器**（`Simulator`），RTL 级走 Verilator/外部仿真器。
> 三者可以互相印证（同一份固件 + 同一组手推期望值）。

### 核级 TB 的固件模式判什么（5 条 · **固件无关**）

| 判据 | 内容 |
|---|---|
| F1a / F1b | 至少取到一条指令，且**首个取指字 == 镜像首字**（⇒ 固件真的进了 I-TCM） |
| F2 | 不得取指到 `0x00000000`（未知 opcode ⇒ 流水线会冻结） |
| F3 | 退休计数 > 0（⇒ 真的在跑） |
| F4 | 至少一次写回（⇒ load 通路通到 WB） |

⇒ ⇒ TB **不硬编码你的固件期望值**（那是固件专属的）⇒ ⇒ 它只判「装载链路成立」。
你的固件语义请读 `FIRMWARE-TRACE:` 行自己核对（或写一个像 `scripts/verify_rv32i_elf_e2e.sh` 那样的护栏）。

### SoC 级 TB 的固件模式判什么（2 条）

| 判据 | 内容 |
|---|---|
| G1 | `store_count` / `writeback_count` 单调不减 |
| G2 | `writeback_count > 0`（⇒ 固件在 SoC 里真的执行了） |

## 4. 装载约束（`rv32i-fw-dump` 是 **fail-closed** 的）

| 约束 | 违反后果 | 为什么 |
|---|---|---|
| 入口 `e_entry` MUST == `0x8000_0000` | **exit 3** + 诊断 | PC 复位值硬连线为 `riscv::kPcReset` ⇒ 否则装出来必砖 |
| 不得含非空 `SHT_RELA` / `SHT_REL` | **exit 3** + 诊断 | 本装载器不做重定位；静默忽略会让固件跑错 |
| 段 MUST 落在 I-TCM / D-TCM（各 16 KB）内 | **exit 3** + 诊断 | 越界 ⇒ 高位被丢 ⇒ 装到**错误**地址 |
| 多区域固件 MUST 用 `--format mem-split` | `mem` / `vh` ⇒ **exit 3** | 无地址的字流表达不了「哪段属于哪个区域」 |
| 程序尾部 MUST 不落到 `0x00000000` | 流水线冻结（不是报错） | I-TCM 初值 0 = 未知 opcode ⇒ 核在 ID 级冻结 |

> 最后一条是**唯一不 fail-closed 的坑**：`rv32i-fw-dump` 会把区域内的空洞补零，
> 但**程序末尾之后**仍是 0 ⇒ ⇒ 请以 `j .`（自环）结尾，别让它跑飞。
> （`tb_rv32i_soc --firmware` 会把 itcm 镜像补到 64 字 NOP，所以一般不会撞上。）

## 5. 怎么判断「跑对了」

| 观测 | 在哪看 | 怎么读 |
|---|---|---|
| 写回序列 | `FIRMWARE-TRACE:` 的 `wb=rd:value,...` | 逐项对照你手推的期望值 |
| store 序列 | `FIRMWARE-TRACE:` 的 `stores=addr:data,...` | 同上 |
| 退休条数 | `retired=` | 应 ≈ 程序条数（流水线填满有几拍延迟） |
| 首个取指 | `fetch0=0x...` | MUST == 你 ELF 的第一个字 |
| SoC 计数 | `SOC-FIRMWARE-TRACE:` | `store_count` = store 条数 · `writeback_count` = 写回条数 |

> 判据纪律：**期望值 MUST 手推**（RV32I ISA 语义），**禁**从 trace 抄 ——
> 那会把实现缺陷固化成期望值（`AGENTS.md` 的 ANTI-PATTERNS 明写）。

## 6. 故障排查

| 症状 | 最可能的原因 | 处置 |
|---|---|---|
| `FATAL：固件镜像为空` | `--out` 目录里没有 `itcm.mem`（或 ELF 的 `.text` 不在 I-TCM 内） | 看 `rv32i-fw-dump` 的 `区域 N 个` 输出；核链接脚本 |
| `RESULT: checks=5 fails=1`（F1b 红） | 首个取指字 ≠ 镜像首字 ⇒ 镜像没进内存 / 入口不在 `0x8000_0000` | 查链接脚本的 `ORIGIN` |
| `retired` 停住不动 | 撞到未初始化字（0 = 未知 opcode）⇒ 流水线冻结 | 固件以 `j .` 结尾 |
| 程序「不动」且 `retired=0` | 做了 **MMIO 访问**（核级 TB 把 8 根 `axil_*` tie 0 ⇒ 永不应答） | 核级固件 MUST NOT 碰 `0x8010_0000` 起的地址 |
| 用了 `csrrw` / `mret` / `ecall` | **CSR / 特权指令未实现**（`dbg_illegal_reason = 1`） | 见 §8（这也是官方 riscv-tests 的主要障碍） |
| `rv32i-fw-dump` exit 3 且提到 `SHT_RELA` | 你的链接把重定位留下来了 | 用 `-nostdlib` + 自定义链接脚本，或 `-Wl,--no-relax` / 静态链接 |

## 7. 现成的固件与护栏（可以照抄）

| 载体 | 内容 |
|---|---|
| `src/rv32i_fw_demo.h` | **带 `.data` 的示例固件**（I-TCM / D-TCM 两段字镜像 · 编码手推）⇒ `--emit-rtl --firmware-init` 用它 |
| `scripts/verify_rv32i_elf_e2e.sh` | 整条链路 E0–E6：转储 → 核级读回 → SoC 级计数 → 产物端口面/initial → 投毒自证 → **真工具链 leg**（本机有 `riscv64-unknown-elf-gcc` 就自动生效） |
| `scripts/verify_rv32i_fw_dump.sh` | 转储器本身的 D0–D12（ELF 解析 / `.bss` 补零 / 越界 / 重定位 / 多区域） |
| `scripts/verify_rv32i_fw_init_rtl.sh` | 产物自带初值的 F0–F5（含「零端口变更」与「确定性」） |

> 这三条护栏是「你的固件/工具坏了」与「核坏了」的分界线 ⇒ ⇒ 先跑它们再怀疑核。
## 8. 跑 RISC-V **官方测试集**（现状 · 实测 · 路线）

> ⚠️ 本节写的是「**能跑什么、为什么、还差什么**」。跑官方测试集的**完整 runner**
> 目前**还没有**进仓（见 §8.6 的路线）；本节给的是已实测的边界与下一步。

### 8.1 官方有两套，别混

| 套件 | 定位 | 判定机制 | 对无 CSR 的 RV32I 核 |
|---|---|---|---|
| **`riscv-tests`**（`riscv-software-src/riscv-tests`） | **官方单元测试**（每指令一个自检测）· RISC-V International 托管 | `tohost`：**写 1 = pass**；fail 写 `(TESTNUM<<1) \| 1` | 测试体是纯 RV32I，但**上游 env 需要 CSR**（见 8.5） |
| **`riscv-arch-test`**（`riscv/riscv-arch-test` · 分支 `act4`） | **官方架构认证**套件（取代 `riscv-compliance`；`riscv-ctg` 已并入） | ACT4：Sail 算期望值 → 生成自检测 ELF；3.x：RISCOF 签名比对 | ACT4 的 config **可以**声明「无标准 M 模式 CSR」（不定义 `STANDARD_SM_SUPPORTED`） |

`riscv-tests` 的 **`-p` / `-v`**：`p` = 物理内存（M 模式 bare metal）· `v` = 虚拟内存（S 模式，`env/v` 用 `scall`）
⇒ ⇒ **只有 `-p` 与我们的核同形**。`rv32mi`（CSR/陷阱/PMP）与 `rv32si`（S 模式/页表）都**不能**跑。

### 8.2 判定机制：只看 `tohost`（**这正是我们双 TCM 也能用的原因**）

- `tohost` 的**地址来自链接脚本**（`env/p/link.ld` 里 `.tohost` 段，典型 **`0x8000_1000`**），不是硬编码。
- pass 路径：`li gp, 1; sw gp, 0(t5)` ⇒ `tohost = 1`。
- fail 路径：`slli gp, gp, 1; ori gp, gp, 1; sw gp, 0(t5)` ⇒ `tohost = (TESTNUM<<1) | 1`（**奇数且 > 1**）。

⇒ ⇒ 我们**不需要读那块内存**（它在 I-TCM 地址段、而 store 走 D-TCM）
⇒ ⇒ 只要在**核的 store 观测面**上盯 `obs_data_addr == 0x8000_1000` 的写值即可判 pass/fail。

### 8.3 本机实测（预编译 `rv32ui-p-*` / `rv32um-p-*` · 48 个）

```bash
# 对每个 ELF：转镜像 → 跑核 → 抓 tohost 写（探针 60 行，见 §8.6 的下一步）
for e in <elfdir>/*; do
  ./build/tools/rv32i-fw-dump --in $e --format mem-split --out /tmp/img/$n
  ./probe /tmp/img/$n 6000        # 观测 store 到 0x80001000 的值
done
```

| 结果 | 计数 | 测试 |
|---|---|---|
| **PASS** | **33 / 40** | `add` `addi` `and` `andi` `auipc` `beq` `bge` `bgeu` `blt` `bltu` `bne` `jal` `jalr` `lui` `or` `ori` `simple` `sll` `slli` `slt` `slti` `sltiu` `sltu` `sra` `srai` `srl` `srli` `sub` `xor` `xori` · **`ld_st`** · **`st_ld`** · **`sw`** |
| **FAIL** | **7 / 40** | `lb` `lbu` `lh` `lhu` `lw` `sb` `sh` —— **全部是「依赖 `.data` 初值」的那一类**（见 8.4） |
| （负向对照） | **0 / 8** | `rv32um-p-*`（`mul` `mulh` `mulhsu` `mulhu` `div` `divu` `rem` `remu`）—— **全部 TIMEOUT**（冻结在 35 条指令处）⇒ ⇒ 本核未实现 M 扩展 ⇒ **本该如此**；这同时证明这套判据**不是「什么都 pass」** |

> **为什么 `sw` / `ld_st` / `st_ld` 反而 PASS**：它们**自己写、自己读**（store 与 load 都落在同一个 D-TCM 别名地址上）
> ⇒ ⇒ 自洽；而 `lb`/`lw`/`sb`/`sh` 读的是**链接期就写好的 `.data` 初值** ⇒ ⇒ 撞上 8.4 的布局问题。

⇒ ⇒ 失败**不是 ISA 缺陷**，是**内存布局不匹配** —— 见 8.4。
> ⚠️ 提醒：`rv32um-p-*`（mul/div/rem）**不属于 RV32I** ⇒ 在只实现 RV32I 的核上它们
> 本来就不该 pass（它们恰好也是本套件的**天然负向对照**）。

### 8.4 失败的根因（已用正/负对照证明）

官方 `env/p/link.ld` 假设**单一平坦内存**：

```
.text.init @ 0x8000_0000   →  .tohost @ 0x8000_1000  →  .data @ 0x8000_2000
```

而本核是**双 TCM**：`kItcm = 0x8000_0000` · `kDtcm = 0x8001_0000`
⇒ ⇒ `.data`（在 0x8000_2000）被 `rv32i-fw-dump` 判进 **I-TCM** 区域
⇒ ⇒ 而核的 `lw`/`lb` **走数据侧（D-TCM）** ⇒ ⇒ 读到 **0** ⇒ ⇒ 第一个用例就 fail。

**对照实验**（同一段汇编，只改 `.data` 的链接地址）：

| `.data` 地址 | `rv32i-fw-dump` 判定 | 核 `lw` 读到的值 |
|---|---|---|
| `0x8001_0000`（D-TCM） | 2 个区域 · `dtcm.mem` = `0000005a` | **`0x5a`** ✅ |
| `0x8000_2000`（I-TCM 段） | 1 个区域 · 值落 `itcm.mem` 索引 2048 | **`0x0`** ❌ |

### 8.5 另一个坑：上游 `env/p` 需要 CSR（本机那批 ELF 没有）

- 现行上游 `env/p/riscv_test.h` 的 `RVTEST_CODE_BEGIN` 无条件执行 `csrw mtvec` /
  `csrwi mstatus` / `csrw mepc` / **`mret`**，且 `RVTEST_PASS` 用 `ecall` 报结果
  ⇒ ⇒ **从上游源码重新构建的 `rv32ui-p-*` 在我们核上会立刻冻结**（实测：`csrw mtvec` 开头
  ⇒ ⇒ `retired=0`、无任何写回）。
- 本机那 48 个 ELF **不含任何 CSR 指令**（`objdump` 全量 grep 为 0）⇒ 是**替换了 env 宏**的变体。
- 官方态度（`riscv-tests` issue **#368** · Andrew Waterman）：
  「The tests themselves do not make use of privileged-architecture features, but their **test environment** does. … **replace those macro definitions** with something appropriate for your platform.」
  ⇒ ⇒ **正路 = 自带一个 env（30 行宏），不改测试体**。

### 8.6 「官方测试跑在核上」—— **R1 / R2 已落地**（2026-11-25 · change `riscv-tests-gate-2026-11`）

| # | 状态 | 做什么 | 判据（**实测**） |
|---|---|---|---|
| **R1** | ✅ **已落地** | 自带 `env/`（`RVTEST_CODE_BEGIN` / `PASS` / `FAIL` / `DATA_BEGIN` 四个宏，**无 CSR**）+ `link.ld`（`.data`/`.bss`/`.tohost` → `0x8001_0000`） | 从上游 `.S` 重建 **40 个** `rv32ui-p-*` ⇒ **40/40 pass**（**排除 2 个**：`fence_i` = 无 Zifencei 扩展 · `ma_data` = 主动式多 hart ⇒ ⇒ **登记**在 `tests/riscv-tests/PROVENANCE.md` §3） |
| **R2** | ✅ **已落地** | **runner**（`tests/test_rv32i_official_suite.cpp` · **4 个独立二进制**）：装载**入库**镜像 → 跑核 → 观测 `tohost` 写 ⇒ pass/fail | `ctest -R test_rv32i_official_suite` ⇒ 4 个条目全绿（**40/40**）· **负向对照** = 运行器 MUST 如实读到 `tohost` 的 FAIL 值（手写镜像写 3 ⇒ 读到 3） |
| **R3** | ⬜ **未做** | 若需「认证级」证据：`riscv-arch-test` ACT4 的 `rv32i`-only config（`include_priv_tests: False` · 不定义 `STANDARD_SM_SUPPORTED`） | 39 个 `rv32i/I` 自检测 ELF 跑通（生成侧需 Sail 0.16 + UDB） |
| **R4** | ✅ **已落地**（2026-11-25 · change `rv32i-official-rtl-gate-2026-11`） | **交付物级门禁**：官方套件 ALSO 跑在 **RTL 产物**上（`tb_rv32i_official.sv` + `scripts/verify_rv32i_official_rtl.sh`）⇒ ⇒ 流程 = 发射（带镜像）→ **lint** → 编译 → 运行 → 判 `tohost` | `--full` ⇒ **40/40**（`tohost` 全 `0x1`）· **负向对照** = 手写镜像写 3 ⇒ 如实读到 3（读到 1 ⇒ **门禁红**）· CI 落点 = `pr-verilator-fast`（抽样）· `nightly-verilator`（全量） |

**资产位置**（**单一真相** = `tests/riscv-tests/PROVENANCE.md`）：

| 项 | 位置 |
|---|---|
| 自带 env | `tests/riscv-tests/env/riscv_test.h`（**只替换 4 个 harness 宏** ⇒ **测试体一字未改**） |
| 链接脚本 | `tests/riscv-tests/env/link.ld`（**双 TCM 布局**） |
| 40 份镜像 | `tests/riscv-tests/images/rv32ui-p-*/{itcm.mem,dtcm.mem}`（`rv32i-fw-dump --format mem-split` 产物） |
| 复现脚本 | `tests/riscv-tests/env/build.sh`（重建 + 与入库镜像**逐字节**比对） |
| 上游许可 | `tests/riscv-tests/LICENSE.riscv-tests`（BSD-3-Clause） |
| **负向对照镜像**（**非官方** · 手写）| `tests/riscv-tests/negative/{itcm.mem,dtcm.mem}`（4 个字 · 登记在 `PROVENANCE.md` §5.1）|

> ⚠️ **两个必须知道的事实**：
> ① **官方套件暴露了一个核的缺陷**（不是 ISA 语义问题）：MEM 级反压期间，
>    旁路**档位**寄存器没有跟着冻结 ⇒ ⇒ 被冻结指令的操作数被别的指令的旁路值改写
>    ⇒ ⇒ 修法与判据见 `tests/test_rv32i_stall_integrity.cpp`（3 个形态）
>    与 `openspec/specs/rv32i-data-forwarding/spec.md`。
> ② **官方套件只跑解释器后端**：该核的设计含 **128 bit** 的 mem 节点 ⇒ ⇒ JIT 整体跳过
>    ⇒ ⇒ ≈ 36 拍/秒 ⇒ ⇒ 40 个测试 ≈ 7 分钟 ⇒ ⇒ 门禁**拆成 4 个独立二进制**
>    （每个 ≈ 100 s ⇒ ⇒ 落在仓内的**单测 300 s 上限**之内）。
>    ⚠️ **2026-11-25 补充**：第 ② 条只说「**核级**门禁跑解释器」⇒ ⇒ **交付物级门禁（R4）**
>    走 **Verilator**（`--binary --timing` ⇒ 需真 verilator）⇒ ⇒ 单个测试 **0.14 s**
>    （冷跑墙钟 ≈ 10 min · verilator 编译 15 s/个主导）。
>
> ⚠️ **为什么要两层门禁**（R2 + R4 **并存**，不是二选一）：
> 解释器与 codegen 由**同一份 C++**生成 ⇒ ⇒ **同构缺陷两后端一致地错** ⇒ ⇒
> **codegen 侧缺陷**（初值块 / 端口映射 / 时序形状 / 宽度）**只**在产物级可见。
> ⇒ ⇒ **实证**：R4 的初版跑出来 **40/40 个带固件产物的 lint 全红**
> （`WIDTHEXPAND` ⇒ `codegen_verilog.cpp` 的 mem 初值块写 `1'b0` ⇒ ⇒ 产物按仓内口径
> **编译直接失败**）⇒ ⇒ 而 R2 对此**结构性失明** ⇒ ⇒ 修法 = 写**目标宽度**的零字面量。

> ⚠️ **现状的诚实说明**：仓内已有的 `tests/test_riscv_tests.cpp`（49 用例 / 43 skip）
> **不测核** —— 它自带一个 C++ ISA 解释器（`Rv32iInterpreter`），跑的是那颗解释器。
> 「官方测试跑在**核**上」的入口 = **`ctest -R test_rv32i_official_suite`**。

## 9. 相关载体（都读过再动手）

| 载体 | 内容 |
|---|---|
| `AGENTS.md` 的「IP 集成契约」段 | 端口面 / 复位极性 / 事务语义 / 错误语义现状 / `irq` 缺失 / 已知限制 |
| `src/rv32i_fw_demo.h` | 带 `.data` 的示例固件（`--emit-rtl --firmware-init` 用它） |
| `scripts/verify_rv32i_elf_e2e.sh` | 整条 ELF 链路的 E0–E6（含真工具链 leg） |
| `scripts/verify_rv32i_fw_dump.sh` | 转储器 D0–D12 |
| `scripts/verify_rv32i_fw_init_rtl.sh` | 产物自带初值 F0–F5 |
| `include/cpu/riscv/rv32i_mem_map.h` | **地址映射单一真相**（两个 TCM + 7 个 MMIO 段） |