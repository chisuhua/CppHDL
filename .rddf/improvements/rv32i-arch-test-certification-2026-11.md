---
name: rv32i-arch-test-certification-2026-11
priority: P2
phase: phase-6
category: cpu
type: feature
残余: >-
  **第一步 = S-A.0 环境 spike 未做**（认领 owner + 时限 → S-A go/no-go）⇒ ⇒ 前置 = `rv32i-privileged-csr-2026-11` 的特权面
状态: draft
进度: >-
  "**立项（2026-11-25 · planner）** · 仓内 **R3**（`examples/riscv-mini/RUNNING-ELF.md` §8.6）自 2026-11 起登记为「⬜ 未做」且**至今无草案、无 change** ⇒ ⇒ 本草案补上该缺口 · ⚠️ **第一步 = S-A.0 环境 spike（认领 owner + 时限）→ S-A go/no-go**（**零产品代码改动 · 产物不进仓**；⚠️ **非「零成本」** —— 生成侧 Sail/mise/uv 本机全缺）· ⚠️ **前置**：① **特权面被 `rv32i-privileged-csr-2026-11` 阻塞** ⇒ ⇒ **本项只取非特权子集** · ② **`.text.rvmodel` 落点 = 架构级冲突**（本核取指无区域检查 ⇒ ⇒ MUST 显式放 I-TCM 侧 · 见草案 A.2）· ③ **经 metis 第 1 轮审稿**（3 BLOCKER + 9 建议 + NIT · **全部已处置**）"
主题: RV32I ISA 缺口 —— riscv-arch-test（ACT4）认证级合规证据
依赖: >-
  `rv32i-privileged-csr-2026-11`（**特权面**的硬前置 · improvement · draft）· `rv32i-isa-gap-closure-2026-11`（`OP_SYSTEM` 非法 ⇒ CSR 未实现 · improvement · partial）· **change `riscv-tests-gate-2026-11`（已归档** · `openspec/changes/archive/2026-11-25-riscv-tests-gate-2026-11/` · **R1/R2/R4 已落地** · 本项复用其 env / runner / 门禁骨架）· **change `rv32i-official-rtl-gate-2026-11`（已归档** · 交付物级门禁骨架）· ADR-069（内存时序）· ADR-047（前推）
来源: 用户 2026-11-25 三问（「riscv-mini 可以做 riscv-arch-test 吗」「建议在 riscv-mini 还是 riscv-soc 做」「切 planner 给出后续任务建议和规划」）+ planner 实测（上游 ACT4 事实核对 · 本机工具就绪度 · 核级/SoC 级观测面对比 · 现有门禁基线复跑 4/4 绿）
生成时间: >-
  2026-11-25
evidence: probe:examples/riscv-mini/RUNNING-ELF.md
---

# Improvement: riscv-arch-test（ACT4）认证级合规证据 —— 核级判定主干

**主题**: RV32I ISA 缺口

> 具体对象 = 用 **RISC-V 官方架构认证套件 `riscv-arch-test`（ACT4）** 给本核补一层
> **认证级**证据（期望值来自 **Sail / RVMODEL 参考模型**，不是「测试自己断言」）。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

---

## 架构依据

| # | 权威 / 实测事实 | 对本项的约束 |
|---|---|---|
| ① | `examples/riscv-mini/RUNNING-ELF.md` §8.6 的 **R3** = 「`riscv-arch-test` ACT4 的 `rv32i`-only config」· 状态 **⬜ 未做** · **自 2026-11 起无草案、无 change** | ⇒ ⇒ **本项补的是「已登记但无载体」的缺口** ⇒ ⇒ MUST NOT 与 `riscv-tests`（R1/R2/R4 **已落地**）混为一条线 |
| ② | **实测基线（2026-11-25 · planner 复跑）**：`ctest -R test_rv32i_official_suite` ⇒ **4/4 绿 · 405.6 s** ⇒ ⇒ `riscv-tests` 的 **40/40** 在当前工作树**仍成立** | ⇒ ⇒ 本项的**对照基线**成立 ⇒ ⇒ **不重复做**已覆盖的 `rv32ui` 语义 |
| ③ | **ACT4 = Makefile + Python（uv workspace）· 生成自检测 ELF · 用户自带 TB 逐个跑**（官方 README）· ⚠️ **生成是「两遍」**：先编 `.sig.elf` → 在 Sail 上跑出期望签名 → 编入最终 self-checking ELF | ⇒ ⇒ **判定协议归 DUT 定义**（`rvmodel_macros.h` 的 `RVMODEL_HALT_PASS/FAIL`）⇒ ⇒ **可复用**现有 `tohost` 判定链 · ⇒ ⇒ ⚠️ **无 Sail ⇒ 无期望签名 ⇒ 无 ELF** ⇒ ⇒ 生成侧 Sail 是**硬依赖** |
| ④ | **官方有现成的 RV32I-only 无 CSR 模板** = `config/cores/cvw/cvw-rv32i/`（`include_priv_tests: False # no Zicsr, so no privileged tests` · `<dut>.yaml` 声明 `I` + `Sm` · `rvmodel_macros.h` **刻意不定义 `STANDARD_SM_SUPPORTED`** · halt 写 `tohost = 1` / `3`） | ⇒ ⇒ **直接照抄该目录**（⇒ ⇒ S-A 的第 0 步就**不需要**先写我们自己的 DUT 配置）⇒ ⇒ 且官方模板的判定协议 = `tohost` ⇒ ⇒ 与现有 `test_rv32i_official_suite` **逐字同源** ⇒ ⇒ 负向对照可沿用 `3` |
| ⑤ | `test_config.yaml` 有 **`include_priv_tests`**（默认 `True` · 设 `False` ⇒ 跳过全部依赖特权模式的测试）· **env 的 CSR 启动代码是 `#ifdef STANDARD_SM_SUPPORTED` 条件编译**（`tests/env/rvtest_setup.h`）⇒ ⇒ **不定义该宏 ⇒ 启动代码不执行任何 CSR 指令** | ⇒ ⇒ 本核（无 CSR / 无 trap）**正好对应** `False` + 不定义该宏 ⇒ ⇒ **非特权子集可跑** |
| ⑤b | ⚠️ **UDB 强制要求声明 `Sm`**（即使核无特权态）⇒ ⇒ **必须为 `Sm` 填一批「NO SUCH CSR」的 dummy 参数**（`MISA_CSR_IMPLEMENTED: false` · `NUM_PMP_ENTRIES: 0` · `MTVEC_ACCESS: ro` · `MTVEC_MODES: [0]` · `MISALIGNED_LDST: false` …）⇒ ⇒ 维护者 issue **#2069** 明写「ACT4 应完整支持 I-only 无特权 DUT」（来源 = 上游 `riscv/riscv-arch-test` issue #2069 · 访问日期 **2026-11-25**） | ⇒ ⇒ ⚠️ **「UDB 只声明 `I`」是错的**（会被 UDB 校验直接拒绝）⇒ ⇒ MUST 照抄 `cvw-rv32i` 的完整形态 ⇒ ⇒ **`Sm` 伪声明 ≠ 核实现了 CSR**（参数全是 dummy） |
| ⑤c | **ACT4 的 `I` 子集 = 39 个 ELF**（`tests/rv32i/I/` 下 39 个 `.S` ⇒ **每个 `.S` 一个 ELF** ⇒ ELF 内含**多个 testcase** ⇒ **整份 ELF 一次性判定** ⇒ 无 3.x 的逐 case 编号）· 额外声明 `Zicsr`/`Zifencei`/`Zicntr` 会再多 9 个 | ⇒ ⇒ **判定单位 = ELF**（⇒ ⇒ runner 按 ELF 判 ⇒ ⇒ 与现有「一镜像一判定」形态一致）⇒ ⇒ **纯 I ⇒ 39 个** ⇒ ⇒ 本项的规模上界 = **39** |
| ⑤d | **`link.ld` 的节区要求**：`ENTRY(rvtest_entry_point)` · `.text.init TEST_BASE` · `.text.rvtest` · `.rodata` · `.data` · `.bss`（`__bss_start`/`__bss_end`）· stack（`__stack_bottom`/`__stack_top`）· `.text.rvmodel`；`TEST_BASE` = 复位向量（**不强制 `0x8000_0000`** · 官方先例 picorv32 = `0x0` · cve2 = `0x4000`） | ⇒ ⇒ ⇒ **入口 MUST 是 `rvtest_entry_point`**（⇒ ⇒ 我们的 `rv32i-fw-dump` 硬要求入口 == `0x8000_0000` ⇒ ⇒ ⇒ `TEST_BASE` MUST 取 `0x8000_0000`）⇒ ⇒ ⚠️ **`.text.rvmodel` 的落点是本项的架构级冲突**（见 **A.2**）⇒ ⇒ ⇒ **MUST 显式放 I-TCM 侧**（偏离上游惯例 ⇒ ⇒ 理由须登记） |
| ⑤e | ⚠️ **ACT4 不默认放 `.tohost`** —— 它由 `RVMODEL_DATA_SECTION` 里的 `.pushsection .tohost` 产生 ⇒ ⇒ 作为 **orphan section** 由链接器放置 | ⇒ ⇒ ⇒ **orphan 放置位置不可靠** ⇒ ⇒ MUST 在 `link.ld` 里**显式**给 `.tohost` 钉地址（`0x8001_0000`）⇒ ⇒ 否则判据地址漂移 ⇒ ⇒ ⇒ **判据依赖此地址 ⇒ MUST 显式钉死** |
| ⑥ | **核级 vs SoC 级（实测对比）**：核级 53 端口含 `obs_data_addr` / `obs_data_write_data` / `obs_data_write_en` ⇒ **tohost 判定通道现成**；SoC 级 10 端口 ⇒ **看不到核内部信号**（ADR-072 D1）⇒ 且 SoC 的 I/D-TCM **仍是各 16 KB**（不提供更多内存） | ⇒ ⇒ **判定主干 MUST 落核级** ⇒ ⇒ SoC 级要判 pass/fail 须**先扩根端口面**（10 → 11+）⇒ ⇒ **纯成本换不到额外证据** |
| ⑦ | **本机工具就绪度（实测）**：`riscv64-unknown-elf-gcc` 16.1.0 ✔ · `riscv32-unknown-elf-gcc` 15.2.0 ✔ · `python3` 3.12 ✔ · `make` ✔ · **`sail_riscv_sim` ✘** · **`mise` ✘** · **`uv` ✘** · **`docker` / `podman` ✘** ⚠️ 官方**当前**要求（来源 = 上游 `act4` 分支 README，访问日期 **2026-11-25**）：**GCC 15 / Binutils 2.44**（或 LLVM/Clang 22）· **Sail 0.16** · Python 3.10+ · Ruby/Bundler + UDB gem | ⇒ ⇒ 生成侧是**独立环境工作项**（Sail 需 OCaml 工具链）⇒ ⇒ ⚠️ **官方预构建镜像 `ghcr.io/riscv/act4-build:latest` 是最省路径，但本机无 docker/podman ⇒ ⇒ 本机走不通** ⇒ ⇒ ⇒ ⚠️ **但 CI runner（`ubuntu-22.04`）自带 docker ⇒ ⇒ 「CI 里跑官方镜像」是可行路径**（⇒ ⇒ 本仓 CI 当前**无 docker 先例** ⇒ ⇒ 该 job 属 CI 改动 · 归 builder 域）⇒ ⇒ ⇒ ⚠️ **本机 gcc 是 16.1.0（高于要求的 15）⇒ ⇒ 版本兼容性未验证** ⇒ ⇒ ⇒ ⇒ **⇒ 必须先做 S-A.0 环境 spike**，不要先投环境 |
| ⑧ | 本核 **CSR / `ECALL` / `MRET` 未接入流水线**（`include/cpu/pipeline/stages/id_stage.h:104` ⇒ `OP_SYSTEM` = 已知未实现 ⇒ `illegal_reason = 1`） | ⇒ ⇒ **特权测试不可跑** ⇒ ⇒ 本项 MUST 只取**非特权**子集 |
| ⑨ | 本核 `FENCE` / `FENCE.I` = **NOP**（`include/cpu/pipeline/rv32i_decoder.h:57`）· 非对齐 load/store = **静默对齐** | ⇒ ⇒ 与 `riscv-tests` 的两个已登记排除项（`fence_i` / `ma_data`）**同源** ⇒ ⇒ arch-test 侧 MUST **同样逐项登记**，不得静默缩小 |

### A.1 已否掉的形态（不要重提）

| 形态 | 否掉理由 |
|---|---|
| **RISCOF 3.x**（签名比对） | ⇒ ⇒ 3.x 已 **deprecated**（官方 README：默认分支已是 `act4`）· ⇒ ⇒ 3.x 要求 DUT 以 **plugin** 形式暴露「单指令 step + 内存访问」⇒ ⇒ 5 级流水线的核难以给出 step 接口 · ⇒ ⇒ 且 3.x **无 `include_priv_tests` 这类一等开关** ⇒ ⇒ ⇒ **形态不适配 + 已弃用** ⇒ ⇒ ⇒ 双重否掉 |
| **在 SoC 级做判定** | SoC 级看不到 `obs_data_*` ⇒ ⇒ 判不了 pass/fail ⇒ ⇒ 须先扩根端口面（连带 ADR-072 / 端口表 / C6 快照 / 独立 TB 同步）⇒ ⇒ **成本 > 收益** |
| **先扩 TCM 容量**（`kTcmAddrWidth` 12 → 13） | ⇒ ⇒ 全平台内存布局变更（`rv32i_mem_map.h` 是单一真相 ⇒ 连带地址表 / 核 IP 顶层 / 判据）⇒ ⇒ **独立 change**，MUST NOT 与本项混做 |
| **直接跑特权测试** | 被 ⑧ 阻塞 ⇒ ⇒ 须等 `rv32i-privileged-csr-2026-11` 的 P6-2 起 |

### A.2 ⚠️ `.text.rvmodel` 的落点 = **本项的架构级冲突点**（metis BLOCKER-1 · **已实测坐实**）

**事实（实测 · 非推断）**：本核的取指路径 =
`itcm.io().addr <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr);`
（`examples/riscv-mini/src/rv32i_core_top.h:204`）⇒ ⇒ **只做位切片、**无区域检查** ⇒ ⇒
从 `0x8001_xxxx`（D-TCM 区）取指 ⇒ ⇒ **alias 到 I-TCM 的低 16 KB**
（例：`0x8001_0000` ⇒ 字索引 `0x000` ⇒ ⇒ **跳回 I-TCM 复位向量**）。

**冲突**：ACT4 的标准 `link.ld` 把 `.text.rvmodel` 放在 **`.data` 之后** ⇒ ⇒ 落**数据侧** ⇒ ⇒
而 `RVMODEL_HALT_PASS` / `RVMODEL_HALT_FAIL` 是**会被 `jal` 调用的代码**（⇒ ⇒ 含**会被执行的指令**）⇒ ⇒
落 D-TCM ⇒ ⇒ **取指 alias ⇒ 跑飞** ⇒ ⇒ ⇒ **「确认它在 D-TCM 内」不是缓解 · 缓解恰恰相反**。

⇒ ⇒ **本草案的裁决**：`.text.rvmodel` MUST **显式放 I-TCM 侧**（`.text.rvtest` 之后 · 地址仍在 `0x8000_xxxx` 内）
⇒ ⇒ ⚠️ **这是偏离上游布局惯例** ⇒ ⇒ ⚠️ **「`.text.rvmodel` 在 `.data` 之后」是链接脚本**惯例** · 是否框架**硬要求**待 S-A0 核实**
（⇒ ⇒ 若核实为硬要求 ⇒ ⇒ 则本项须走「上游 link.ld 改造 + 偏离登记」双条路，并在 S-A0 的产出里明确记下）。

⇒ ⇒ **连带影响（必须同步进容量判据）**：
1. `.text.rvmodel` 落 **I-TCM** ⇒ ⇒ **I-TCM 的容量判据 MUST 加上它的体积**；
2. ⇒ ⇒ **D-TCM 的容量判据 MUST 相应减去**（⇒ ⇒ 不再包含 `.text.rvmodel`）；
3. ⇒ ⇒ 偏离理由 MUST 登记在 `PROVENANCE.md`（理由 = 本核的**双 TCM 分离**（取指只走 I-TCM）⇒ ⇒ 上游「平坦内存」布局**不可直用**）。

---

## 范围

### IN

- **S-A0（环境 spike · **认领 owner 与时限** · metis BLOCKER-3）**：⚠️ **S-A 依赖生成侧工具链 ⇒ ⇒ 而该依赖本草案原先被划进 OUT ⇒ ⇒ 关键路径上出现一个没人认领的洞** ⇒ ⇒ **本项 MUST 把它显式认领**：
  - **owner** = builder（**CI job 路线**）/ planner + 用户（**本地 spike 路线** · 需预算）
  - **时限**：本地 spike **≤ 0.5 天**（⇒ ⇒ 超时 ⇒ ⇒ **如实记录并停止** ⇒ ⇒ 不得无限投入）
  - **产出**：**一份可复现命令 + 成功/失败记录**（成功 ⇒ ⇒ 后续 S-A 可跑；失败 ⇒ ⇒ **本项降级为「不立项」并记录失败原因**）
  - **两条路径（择一，按此顺序）**：
    - **路径 ①（推荐 · 本机零安装）**：**新增一个 `workflow_dispatch` 手动触发的 CI job**（`ubuntu-22.04` runner **自带 docker**）⇒ ⇒ 跑官方预构建镜像 `ghcr.io/riscv/act4-build:latest` ⇒ ⇒ 在 CI 里生成 ELF + 量尺寸 ⇒ ⇒ ⚠️ **本仓 CI 当前无 docker 使用先例** ⇒ ⇒ **该 job 本身是 CI 改动（归 builder 域）** ⇒ ⇒ ⚠️ **零产品代码改动，但非零 CI 改动**
    - **路径 ②（兜底）**：本地装 Sail 0.16（需 OCaml 工具链）+ `mise`/`uv` + Ruby-UDB ⇒ ⇒ ⚠️ **本机 gcc 16.1.0 高于官方要求的 15 ⇒ ⇒ 兼容性未验证** ⇒ ⇒ ⇒ **若版本不兼容 ⇒ ⇒ 记录并停止**
  - ⇒ ⇒ **无论走哪条，S-A0 的结论 MUST 落进 `PROVENANCE.md`**（含**工具链版本锁**：gcc / sail / uv / udb 各是什么版本）
- **S-A（go/no-go · **零产品代码改动 · 产物不进仓**）**：**照抄官方 `config/cores/cvw/cvw-rv32i/`**（现成的 RV32I-only 无 CSR 模板）⇒ ⇒ 用它跑通生成侧工具链（**前置 = S-A0**）⇒ ⇒ **生成 ELF** ⇒ ⇒ **只量尺寸**（`.text` / `.data` / `.bss` / `.text.rvmodel`）⇒ ⇒ 判定 **16 KB I-TCM / 16 KB D-TCM 装不装得下**
  - ⚠️ 若装不下 ⇒ **第一杠杆 = `TESTCASES_PER_FILE`**（生成侧参数 · 切细 ⇒ 每 ELF 更小）⇒ ⇒ **不动硬件**
  - ⚠️ 若 `TESTCASES_PER_FILE` 调到最小仍装不下 ⇒ ⇒ 才考虑「扩 TCM 容量」⇒ ⇒ 但那是**独立 change**（本项只出「容量结论 + 建议」，不实施）
  - ⚠️ **MUST NOT** 拿「官方 cvw-rv32i 的 ELF」当我们的判据资产 ⇒ ⇒ 它是**工具链验证**，不是我们内存布局下的产物 ⇒ ⇒ 尺寸结论 MUST 在**我们自己的 `link.ld`** 下重测
- **S-B（DUT 适配面 · 5 个文件）**：
  - `test_config.yaml`（`compiler_exe` / `ref_model_exe` / `udb_config` / `linker_script` / `dut_include_dir` / **`include_priv_tests: False`**）
  - `<dut>.yaml`（**UDB**）：`implemented_extensions` = **`{I, "= 2.1"}` + `{Sm, "= 1.12.0"}`**（⚠️ **`Sm` 仅为承载 `MXLEN` 的 dummy 声明** ⇒ ⇒ **不代表核实现了 CSR**）· `MXLEN: 32` · `PHYS_ADDR_WIDTH: 32` · `M_MODE_ENDIANNESS: little` · **`MISA_CSR_IMPLEMENTED: false`** · `NUM_PMP_ENTRIES: 0` · `MTVEC_ACCESS: ro` · `MTVEC_MODES: [0]` · **`MISALIGNED_LDST: false`** ⇒ ⇒ ⚠️ **不**声明 `Zicsr` / `Zifencei` / `Zicntr` ⇒ ⇒ **不生成**相应测试（判据 = 产出目录里**没有** `Zicsr*` / `Zifencei*`）
  - `rvmodel_macros.h`（照 `cvw-rv32i` 形态）：⚠️ **刻意不定义 `STANDARD_SM_SUPPORTED`**（⇒ ⇒ env 的 CSR 启动代码被 `#ifdef` 屏蔽 ⇒ ⇒ 运行期不碰任何 CSR）· 定义 `RVMODEL_HALT_PASS` ⇒ 写 `1` 到 `tohost` / `RVMODEL_HALT_FAIL` ⇒ 写 `3` · `RVMODEL_DATA_SECTION` ⇒ `.pushsection .tohost` · `RVMODEL_IO_*` ⇒ **可留空**（⇒ ⇒ 诊断打印**非必需** ⇒ ⇒ 想要人可读诊断再另加 store 观测面方案）
  - `link.ld`（**双 TCM 重排**）：`ENTRY(rvtest_entry_point)` · `TEST_BASE = 0x8000_0000`（**MUST** —— 我们的 PC 复位值硬连线为 `riscv::kPcReset`）· `.text.init` / `.text.rvtest` @ `0x8000_0000`（I-TCM）· **`.tohost` 显式钉 `0x8001_0000`**（D-TCM 区首 · ⚠️ **MUST 显式** ⇒ orphan placement 不可靠）· `.rodata` / `.data` / `.bss` / stack / **`.text.rvmodel`（MUST 在 `.data` 之后 ⇒ ⇒ 落数据侧 ⇒ ⇒ MUST 确认在 D-TCM 内）**
  - `sail.json`（**Sail 模型配置**）：⚠️ **`memory.regions` MUST 与 `link.ld` 的 `RAM_ORIGIN` / `RAM_LENGTH` 一致**（⇒ ⇒ 不一致 ⇒ Sail 算不出期望签名）⇒ ⇒ ⇒ **双 TCM 布局对 Sail 不构成障碍**（`0x8000_0000` / `0x8001_0000` 只是普通物理地址）
- **S-C（核级判定主干）**：复用 `examples/riscv-mini/tests/test_rv32i_official_suite.cpp` 的 runner 形态（观测 `obs_data_addr == 0x8001_0000` 的写值）⇒ ⇒ **新增镜像目录** + **新增 ctest target**
- **S-D（交付物级门禁）**：仿 `scripts/verify_rv32i_official_rtl.sh`（发射 → lint → 编译 → 运行 → 判 `tohost`）⇒ ⇒ 含**负向对照**（手写镜像写 `3` ⇒ MUST 如实读到 `3`）
- **S-E（能力边界声明 · 非「排除测试」）**：
  - ⚠️ **口径订正（实测上游）**：ACT4 的能力边界由 **UDB 声明**决定（CTP：「Only test features whose behavior is specified for the given parameters」）⇒ ⇒ **非对齐访问 = 在 `<dut>.yaml` 声明 `MISALIGNED_LDST: false`**（⇒ ⇒ 框架据此**不生成**非对齐测试）⇒ ⇒ **不是**靠「排除某个测试」
  - `Zifencei` / `Zicsr` = **不声明该扩展** ⇒ ⇒ 框架**不生成** ⇒ ⇒ 亦非「排除」
  - ⇒ ⇒ **本项 MUST NOT 出现「排除项」清单** ⇒ ⇒ ⇒ **MUST 改成「UDB 能力声明清单」**（每条 = 扩展名 + 声明值 + 理由 + 产出侧的可核对证据）⇒ ⇒ ⇒ 与 `riscv-tests` 的「排除项登记」**是两套不同机制**（那边是**我们删测试**，这边是**我们声明能力**）⇒ ⇒ ⇒ ⇒ **MUST NOT 混用口径**

### OUT（不做 · 已登记归属）

| 项 | 归属 / 理由 |
|---|---|
| **特权测试面**（Exceptions / Interrupts / PMP / Zawrs / `Zicsr`） | ⇒ ⇒ 被 `rv32i-privileged-csr-2026-11`（P6-2..P6-7）阻塞 ⇒ ⇒ **本项只出「解锁条件」**，不实施 |
| **RISCOF 3.x 路线** | 形态不适配（见 A.1） |
| **SoC 级判定面** | 观测面不可见 ⇒ ⇒ 触发条件 = 「SoC 根端口面扩容」被独立立项 |
| **生成侧工具链的落地**（装 Sail / mise / uv） | ⇒ ⇒ **独立环境工作项**（本项只声明依赖 + 给可复现命令）⇒ ⇒ ⚠️ **但由 S-A.0 显式认领 owner + 时限 + 产出**（⇒ ⇒ 否则 S-A 的前置**无人负责** ⇒ ⇒ metis BLOCKER-3）⇒ ⇒ ⇒ **S-A.0 的产出 = 可复现命令 + 成功/失败记录**（⇒ ⇒ **不是**「把环境装好」） |
| **扩 TCM 容量** | ⇒ ⇒ 独立 change |
| **替换 / 并入现有 `riscv-tests` 门禁** | ⇒ ⇒ 两套套件**并存**（`riscv-tests` = 单元级自断言 · arch-test = Sail 期望值）⇒ ⇒ 判定面不同，MUST NOT 合并 |

### 写面（按批分级）

| 批 | 写点 |
|---|---|
| **S-A** | **只读**（生成物落 `/tmp/opencode/arch-test-probe/` ⇒ ⇒ **不进仓**） |
| **S-B** | 新目录 `examples/riscv-mini/tests/riscv-arch-test/{env,config}/`（**新文件** ⇒ ⇒ 零写点冲突） |
| **S-C** | 新文件 `examples/riscv-mini/tests/test_rv32i_arch_suite.cpp`（**新文件**）+ 新镜像目录 ⇒ ⇒ 与现有 `test_rv32i_official_suite.cpp` **零重叠** |
| **S-D** | 新脚本 `scripts/verify_rv32i_arch_rtl.sh`（**新文件**）⇒ ⇒ 仿 `verify_rv32i_official_rtl.sh` |
| **S-E** | `examples/riscv-mini/tests/riscv-arch-test/PROVENANCE.md`（**新文件**） |

⇒ ⇒ **写点全部是新增文件** ⇒ ⇒ 与 phase-6 的 **B3 / B4 / R15 / R16 / R3** 写点**零重叠** ⇒ ⇒ **可全程并行**（仅收口阶段不并行 —— 见 §Impact）。

---

## Why

**可验证目标（user story）**：在本仓的 RV32I 5 级流水线核上，跑通**一批由 RISC-V Sail 参考模型算出期望值**的自检测 ELF ⇒ ⇒ 全部 PASS ⇒ ⇒ 得到一层**「期望值来自官方参考模型」**的合规证据（现有 `riscv-tests` 的期望值来自**测试自己**，不是参考模型）。

1. **现有证据的天花板是「单元级自断言」**：`riscv-tests` 的 40 个 `rv32ui-p-*` 覆盖面好（RV32I 整数指令语义全覆盖 · 实测 4/4 绿），但**期望值写在测试体内** ⇒ ⇒ 它验证的是「核符合测试的预期」，不是「核符合 RISC-V 规范的预期」⇒ ⇒ 两者的差别在**期望值的来源**。
2. **这是官方对「架构合规」的正式载体**：`riscv-arch-test` 是 RISC-V International 托管的**架构认证套件**（取代 `riscv-compliance` · `riscv-ctg` 已并入）⇒ ⇒ 现有 `riscv-tests` 门禁**不能**替代它。
3. **落地成本已被上游把到很低**：ACT4 把「DUT 适配」压缩成 **5 个文件**（`test_config.yaml` / `<dut>.yaml` / `rvmodel_macros.h` / `link.ld` / `sail.json`）⇒ ⇒ 且**判定协议归 DUT 定义**（`rvmodel_macros.h`）⇒ ⇒ **不需要改上游任何测试体**（与 `riscv-tests` 的「自带 env · 只替换 4 个 harness 宏」同一手法）。
4. **判据链可复用**（**复用成本 ≈ 0** —— 现有 `test_rv32i_official_suite.cpp` 已经跑通了「观测 `tohost` 写值 ⇒ 判 pass/fail」⇒ ⇒ **同一套 runner 形态**可直接承接（`tohost` 地址仍是 D-TCM 区首 ⇒ ⇒ 判据链与现有 40/40 逐字同源）⇒ ⇒ ⚠️ **但**失败判据**不可直接照抄**（⇒ ⇒ arch-test 的失败路径含 `ebreak` ⇒ ⇒ 本核会冻结 ⇒ ⇒ 需加 `dbg_illegal_reason` 判据 ⇒ ⇒ 见 S-C.3））。
5. **它同时是「CSR 特权级」的验收抓手**：arch-test 的特权面（`Zicsr` / Exceptions / Interrupts）**正是本核当前的能力缺口**（`OP_SYSTEM` 零消费者）⇒ ⇒ 一旦 CSR 落地，本项的覆盖面**自动扩展**而不必重做工具链 ⇒ ⇒ ⇒ **本项的真正价值在「把工具链搭好 · 让 CSR 落地即刻兑现」**。

---

## What Changes

| 步 | 内容 | 判据（前瞻 · 可机械核对） |
|---|---|---|
| **S-A.1** | **前置 = S-A.0**（环境 spike · 认领 owner + 时限）⇒ ⇒ 照抄官方 `config/cores/cvw/cvw-rv32i/` ⇒ ⇒ **跑通生成侧工具链** ⇒ ⇒ 产出 **N 个 ELF**（`I` 子集 · **N = 生成清单记录值 · 默认 39** · ⚠️ **`TESTCASES_PER_FILE` 调小 ⇒ N 会变 ⇒ ⇒ N MUST 记录**) ⇒ ⇒ **只量尺寸** | N 个 ELF 存在 + **打印每个 ELF 的 `.text` / `.data` / `.bss` / `.text.rvmodel` 字节数**（用 `objdump -h` / `readelf -S` · 手算复核） |
| **S-A.2** | ⚠️ **go/no-go 判定**（**容量** · **MUST 在我们自己的 `link.ld` 下重测** ⇒ ⇒ 官方 cvw-rv32i 的 ELF **不是**我们布局下的产物） | **五条判据**：<br>**① 容量（I-TCM）**：`.text.init` + `.text.rvtest` + **`.text.rvmodel`**（⚠️ **`.text.rvmodel` 落 I-TCM 侧** ⇒ ⇒ **计入 I-TCM** ⇒ ⇒ 见 A.2）≤ **16 KB − 1 KB**（⚠️ **余量 = 1 KB，现在定死** ⇒ ⇒ 理由：单 ELF 含多 testcase · 尺寸随 `TESTCASES_PER_FILE` 变动 ⇒ ⇒ **MUST NOT 事后填 0**）<br>**② 容量（D-TCM）**：`.tohost` + `.rodata` + `.data` + `.bss` ≤ **16 KB − 1 KB**（⚠️ **`.text.rvmodel` 不计入 D-TCM** ⇒ ⇒ 因它落 I-TCM）<br>**③ 机械门（现成的）**：N 个 ELF **逐个**过 `rv32i-fw-dump --format mem-split` ⇒ ⇒ **exit 3 数 == 0**（⚠️ 该工具对越出 TCM 区域的节区 **fail-closed** ⇒ ⇒ 比手算字节数更机械 · 且**直接模拟真实装载**）<br>**④ 运行时预算**：估算「单 ELF ~100 s ⇒ N 个 ≈ **65 分钟**」⇒ ⇒ **MUST 给出总时长上限 + CI 落点建议**（⚠️ **「装得下但跑不完」是与容量独立的第二种 no-go** ⇒ ⇒ ⇒ 且现有 40 镜像实测 405.6 s ≈ 10 s/镜像 ⇒ ⇒ ⇒ ACT4 单 ELF 含多 testcase ⇒ ⇒ ⇒ **耗时不可按 10 s/镜像外推**）<br>**⑤ 装不下 ⇒ ⇒ `TESTCASES_PER_FILE` 调小后重测**（**记下调小值 + 新的 N**）⇒ ⇒ **五条结论 MUST 落进 `PROVENANCE.md`** |
| **S-A.3** | ⚠️ **go/no-go 判定**（**期望值来源**） | ⇒ ⇒ **必须有 Sail 产出的期望签名**（生成是「两遍」⇒ ⇒ 无 Sail ⇒ **连 ELF 都没有**）⇒ ⇒ ⚠️ **「测试自断言」不算**（那 = 现有 `riscv-tests` 的形态 ⇒ ⇒ 无增量价值）⇒ ⇒ **本步红 ⇒ 本项降级为「不立项」并记录理由** |
| **S-B.1** | `test_config.yaml` | 6 个字段齐 + **`include_priv_tests: False`** |
| **S-B.2** | `<dut>.yaml`（UDB） | `implemented_extensions` = `{I, "= 2.1"}` + **`{Sm, "= 1.12.0"}`（dummy · 仅为 MXLEN）** · `MXLEN: 32` · `PHYS_ADDR_WIDTH: 32` · `M_MODE_ENDIANNESS: little` · **`MISA_CSR_IMPLEMENTED: false`** · `NUM_PMP_ENTRIES: 0` · `MTVEC_ACCESS: ro` · `MTVEC_MODES: [0]` · **`MISALIGNED_LDST: false`** ⇒ ⇒ **不**声明 `Zicsr` / `Zifencei` / `Zicntr` ⇒ ⇒ **生成侧据此不产相应测试**（⚠️ **判据 MUST 与产出总数绑定**（防恒绿）：「产出 ELF 数 **== N**（N = 生成清单记录值 · 默认 39）**且** 产出目录里**没有** `Zicsr*` / `Zifencei*`」⇒ ⇒ ⇒ **「没有 Zicsr*」单独看是恒绿形态**（框架若因配置错误一个文件都不产 ⇒ 该判据照样过）⇒ ⇒ ⇒ ⇒ **必须与 N 同时断言** · 且 `udb validate` 通过） |
| **S-B.3** | `rvmodel_macros.h` | ⚠️ **不定义 `STANDARD_SM_SUPPORTED`**（⇒ ⇒ env 的 CSR 启动代码被 `#ifdef` 屏蔽）· `RVMODEL_HALT_PASS` ⇒ 写 `1` · `RVMODEL_HALT_FAIL` ⇒ 写 `3`（⇒ ⇒ 与 `cvw-rv32i` 同值 ⇒ ⇒ **负向对照可直接沿用 `3`**）· `RVMODEL_DATA_SECTION` ⇒ `.pushsection .tohost` |
| **S-B.4** | `link.ld`（**双 TCM**） | `ENTRY(rvtest_entry_point)` · `TEST_BASE = 0x8000_0000`（**MUST**）· `.text.init` / `.text.rvtest` @ `0x8000_0000`（I-TCM）· **`.text.rvmodel` 也放 I-TCM 侧**（⚠️ **偏离上游惯例** · 理由见 **A.2** ⇒ ⇒ **MUST 登记**）· **`.tohost` 显式 @ `0x8001_0000`**（D-TCM 区首 · ⚠️ **MUST 显式** ⇒ orphan placement 不可靠）· `.rodata` / `.data` / `.bss` / `__bss_start` / `__bss_end` / `__stack_bottom` / `__stack_top` ⇒ ⇒ ⚠️ **MUST 与 Sail 侧共用同一份**（ACT4 要求 test-visible 符号地址两侧一致）⇒ ⇒ ⇒ **双 TCM 布局对 Sail 不构成障碍**（`0x8000_0000` / `0x8001_0000` 只是普通物理地址） |
| **S-B.5** | `sail.json` | `memory.regions` **MUST 与 `link.ld` 的 `RAM_ORIGIN` / `RAM_LENGTH` 一致**（⇒ ⇒ 不一致 ⇒ Sail 算不出期望签名） |
| **S-C.1** | 新增镜像目录（`rv32i-fw-dump --format mem-split` 产物） | 镜像存在 + ⚠️ **复现判据的语义修正**（metis SHOULD_FIX）：**不是**「镜像与上游 ELF 逐字节 diff」（`.mem` 与 `.elf` **不可能**逐字节 diff）⇒ ⇒ **而是**：「**同一份 ELF** 经 `rv32i-fw-dump` **重新生成**的镜像 vs **入库镜像** `diff -r` == 0」⇒ ⇒ ⚠️ **执行环境**：该检查需要生成侧工具链 ⇒ ⇒ **只能本地跑 · CI 不覆盖** ⇒ ⇒ ⇒ **这一条本身也要登记**（不是「CI 门禁」） |
| **S-C.2** | 新增 ctest target（**多个独立二进制** · 与现有门禁同型 —— 单个 ELF 跑 ~100 s ⇒ 300 s 上限内必须拆分） | `ctest -R test_rv32i_arch_suite` ⇒ 全绿 ⇒ ⇒ **总时长 < 明写上限**（S-A.2 判据 ④ 给出）⇒ ⇒ ⚠️ **两个计数锚点都要同步**：**`CTEST_TOTAL`**（新增 ctest target）+ **`EXAMPLES_TEST_SOURCES`**（⚠️ 新文件 `examples/riscv-mini/tests/test_rv32i_arch_suite.cpp` 命中该计数口径 (c)「`examples/**/tests/` 下的任意 `.cpp`」⇒ ⇒ **24 → 25** ⇒ ⇒ ⇒ 原稿**漏了这条**） |
| **S-C.3** | ⚠️ **失败路径必须区分三类**（⚠️ **本项与现有 `riscv-tests` 门禁的**关键差异** ⇒ ⇒ runner 的判据逻辑**不可直接照抄**） | ⚠️ **三类**不是互斥**（metis SHOULD_FIX）—— 一个 ELF 可**先写 `tohost = 3` 再执行 `ebreak`**（失败码先报告再停机是常见形态）⇒ ⇒ ⇒ **改为优先级制**：**② > ① > ③**（先查 illegal，再查 tohost 写，最后才计时）⇒ ⇒ ⇒ **同现时 MUST 两条都记录**（日志里分别留痕）⇒ ⇒ ⇒ **判 FAIL 的结论不变**<br>**① 正常失败** = `tohost` 写 `3` ⇒ 判 FAIL · **② `ebreak` 失败路径**（ACT4 的 failure code 可能执行 `ebreak` ⇒ ⇒ 本核 `OP_SYSTEM` = **已知未实现** ⇒ `illegal_reason = 1` ⇒ ⇒ **流水线冻结** ⇒ ⇒ ⇒ **MUST 在冻结那一拍就判 FAIL** ⇒ ⇒ ⇒ **MUST NOT 等超时**）· **③ 其它挂死** = 既无 `tohost` 写又无 illegal ⇒ 判 FAIL（**另设一类**）⇒ ⇒ ⇒ ⚠️ **`ebreak` ⇒ `illegal_reason` 的观测面 MUST 用核级 `dbg_illegal_reason`**（SoC 级不可见 ⇒ ⇒ 又一条「核级」的硬理由）· ⚠️ **`dbg_illegal_reason` 是核级顶层端口**（`rv32i_core_top.h:113`）⇒ ⇒ 「进 ID 级即冻结」⇒ ⇒ 信号**保持有效** ⇒ ⇒ runner 逐拍轮询**无采样竞态** ⇒ ⇒ ⇒ 但 **冻结后 `obs_data_*` 的写会停止** ⇒ ⇒ ⇒ **③ 的超时兜底仍必需**（⇒ ⇒ 与 ② 的判别顺序 = **先查 illegal 再计时**） |
| **S-C.4** | ⚠️ **负向对照**（**防恒绿**） | 手写镜像写 `3` ⇒ ⇒ 运行器 MUST 如实读到 `3`（读到 `1` ⇒ ⇒ **门禁红**）⇒ ⇒ **与现有门禁逐字节同源** ⇒ ⇒ **另加一条**：手写镜像执行 `ebreak` ⇒ ⇒ 运行器 MUST 判 FAIL（**不得**判超时）⇒ ⇒ ⇒ 证明 ② 那条判据**有判别力**（不是恒红） |
| **S-D.1** | 交付物级脚本（仿 `verify_rv32i_official_rtl.sh`） | 流程 = 发射（带镜像）→ lint（退出码 0 + 零 `%Error`）→ 编译 → 运行 → 判 `tohost` ⇒ ⇒ **lint 是门禁的一环** |
| **S-E.1** | **能力声明清单**逐条登记（`PROVENANCE.md`）——⚠️ **不是「排除项」清单**（metis SHOULD_FIX：原稿 S-E 段已声明「MUST NOT 出现排除项清单」· 而本行却仍写「排除项逐条登记」⇒ ⇒ **改名没改干净的残留** ⇒ ⇒ 本行已改正） | 每条 MUST 含 **6 个字段**：**① 扩展名 / ② 声明值（UDB）· ③ 理由 · ④ 产出侧的可核对证据**（例如「产出目录里没有 `Zicsr*`」⇒ ⇒ ⚠️ **必须与产出总数 N 同时断言**）· **⑤ 上游 riscv-arch-test 的 commit SHA** · **⑥ 生成命令全文 + 工具链版本锁**（gcc / sail / uv / udb 各是什么版本生成的这批镜像）⇒ ⇒ ⚠️ **MUST NOT** 保留「排除理由（能力对齐 vs 跑不过）」这个口径 ⇒ ⇒ ⇒ 「跑不过」与「能力对齐」并列**会诱导实施者把「跑不过」伪装成「声明」** ⇒ ⇒ ⇒ ⇒ **禁** · ⚠️ **另登记代价**：`RVMODEL_IO_*` 留空 ⇒ ⇒ 失败时**无法定位到 ELF 内哪个 testcase 挂**（整份 ELF 一次性判定）⇒ ⇒ **MUST 登记此代价 + 预案**（失败 ⇒ 手工二分 / 临时开 IO） |

### ⚠️ 两条**顺序硬约束**

1. **S-A.0 → S-A → S-B/S-C/S-D**（**环境 spike 认领 + 尺寸结论出来之前不许铺开**）⇒ ⇒ 否则镜像可能装不下 / 或工具链根本装不上 ⇒ ⇒ 白做。
2. **S-C/S-D → 不得与 phase-6 的收口阶段并行** ⇒ ⇒ ⚠️ **本项实际只触及 `AGENTS.md` 的 2 个计数锚点**（**`CTEST_TOTAL`** · **`EXAMPLES_TEST_SOURCES`**）⇒ ⇒ ⇒ **其余 3 个**（`VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS`）**与本项无 contention**（⇒ ⇒ 原稿罗列「5 个」是**过度声明**）⇒ ⇒ ⇒ ⇒ **锚点编辑权 = 最后合入者** ⇒ ⇒ 合入前 MUST **全量重建并重算**。

---

## Acceptance

- [ ] ⚠️ **S-A.0 环境 spike 有 owner + 有时限 + 有产出**（metis BLOCKER-3）⇒ ⇒ **owner** = builder（CI job 路线）/ planner + 用户（本地路线 · 需预算）· **时限** = 本地 spike **≤ 0.5 天**（⇒ ⇒ 超时 ⇒ 如实记录并停止）· **产出** = **可复现命令 + 成功/失败记录**⇒ ⇒ ⚠️ **S-A.0 失败 ⇒ ⇒ 本项降级为「不立项」并记录失败原因**（⇒ ⇒ **不是**无限投入）
- [ ] **S-A 的 go/no-go 结论 MUST 落进 `PROVENANCE.md`**（**五条**：容量 I-TCM / 容量 D-TCM / **`rv32i-fw-dump` exit 3 数 == 0** / **运行时长上限 + CI 落点** / `TESTCASES_PER_FILE` 调小值）⇒ ⇒ 且「期望值来源」那条 **MUST 是 Sail 产出的期望签名**（生成是「两遍」⇒ ⇒ 无 Sail ⇒ **连 ELF 都产不出** ⇒ ⇒ 而「测试自断言」= **本项无增量价值** ⇒ ⇒ 降级为「不立项」）⇒ ⇒ 且容量结论 **MUST 含 `.text.rvmodel`**（**且 MUST 按 A.2 的裁决计入 I-TCM 侧**）
- [ ] **DUT 适配 5 文件齐**：`test_config.yaml`（含 **`include_priv_tests: False`**）· `<dut>.yaml`（`{I}` + **`{Sm}` dummy** · `MISA_CSR_IMPLEMENTED: false` · **`MISALIGNED_LDST: false`** · 不声明 `Zicsr`/`Zifencei`/`Zicntr`）· `rvmodel_macros.h`（**不定义 `STANDARD_SM_SUPPORTED`** · PASS 写 `1` / FAIL 写 `3`）· `link.ld`（`TEST_BASE = 0x8000_0000` · **`.tohost` 显式钉 `0x8001_0000`** · ⚠️ **`.text.rvmodel` 显式放 I-TCM 侧**（偏离上游惯例 ⇒ ⇒ 理由 MUST 登记））· `sail.json`（`memory.regions` 与 link.ld 一致）⇒ ⇒ 且 **Sail 侧与 DUT 侧共用同一份 `link.ld`**（test-visible 符号地址两侧一致）
- [ ] **核级门禁绿**：新增 ctest target 跑完**全部** `rv32i/I` ELF（**N 个** · **N = 生成清单记录值 · 默认 39** · ⚠️ **`TESTCASES_PER_FILE` 调小 ⇒ N 会变 ⇒ ⇒ MUST 用记录值而非写死 39**）⇒ ⇒ **全部 `tohost == 1`** ⇒ ⇒ **两个锚点都同步**（**`CTEST_TOTAL`** + **`EXAMPLES_TEST_SOURCES`**（24 → 25））⇒ ⇒ ⚠️ **期望值抽样判据改写**（metis SHOULD_FIX）：**不是**「手推复核 ≥1 条」（本项期望值来自 Sail = **外部 oracle** ⇒ ⇒ 仓内「禁抄仿真输出」反模式防的是**同源双后端**失明 ⇒ ⇒ 不直接适用）⇒ ⇒ **而是抽验装载链**：抽 1 个 ELF ⇒ ⇒ **`objdump -h` 的节区地址/尺寸 vs 镜像首字/字数逐一对得上**（⇒ ⇒ 这才是本项真正的可判点）
- [ ] ⚠️ **三类失败判据按优先级生效且各有判别力**（⚠️ **不是互斥** · metis SHOULD_FIX）：**优先级 ② > ① > ③**（先查 illegal → 再查 tohost 写 → 最后计时）⇒ ⇒ `tohost == 3` ⇒ FAIL · **`dbg_illegal_reason != 0` ⇒ FAIL（`ebreak` 路径 · MUST 在冻结那一拍判 · MUST NOT 等超时）** · 其它挂死 ⇒ FAIL ⇒ ⇒ **同现时 MUST 两条都记录** ⇒ ⇒ 且 **两条负向对照**（手写镜像写 `3` ⇒ MUST 如实读到 `3`；手写镜像执行 `ebreak` ⇒ MUST 判 FAIL 而非超时）⇒ ⇒ ⇒ **任一条恒绿 ⇒ 门禁红**
- [ ] **交付物级门禁绿**：RTL 产物侧同样判 `tohost` ⇒ ⇒ **lint 是门禁的一环**（退出码 0 + 零 `%Error`）⇒ ⇒ CI 落点与现有 `verify_rv32i_official_rtl.sh` 同型
- [ ] ⚠️ **能力边界用「UDB 声明」表达**（**不是**「排除项」清单）⇒ ⇒ 每条 = **扩展名 / 声明值 / 理由 / 产出侧的可核对证据（须与产出总数 N 同时断言）** ⇒ ⇒ ⇒ **MUST NOT** 与 `riscv-tests` 的「排除项登记」**混用口径**（那是「我们删测试」· 这里是「我们声明能力」）⇒ ⇒ ⇒ 且 MUST 显式登记「**`Sm` 是 dummy 声明 · 不代表核实现 CSR**」⇒ ⇒ ⇒ 且 `PROVENANCE.md` MUST 含 **上游 commit SHA + 生成命令全文 + 工具链版本锁**（gcc / sail / uv / udb）⇒ ⇒ ⇒ 且 MUST 登记「`RVMODEL_IO_*` 留空 ⇒ 失败时无法定位到哪个 testcase」这一代价 + 预案
- [ ] ⚠️ **特权面解锁条件 MUST 写进本草案**：`rv32i-privileged-csr-2026-11` 的 **P6-2（CSR 指令）** 落地后 ⇒ ⇒ 本项**可解锁** `Zicsr` / 特权子集 ⇒ ⇒ **本项本身不实施特权面**（且 ACT4 侧需同步去掉 `include_priv_tests: False` + **定义** `STANDARD_SM_SUPPORTED` ⇒ ⇒ ⇒ 那是**另一个 change**）
- [ ] ⚠️ **生成侧环境是独立工作项**（但**已被 S-A.0 显式认领 owner + 时限**）：本项 MUST NOT 把「装 Sail 0.16 / mise / uv / Ruby-UDB」混进**实施** ⇒ ⇒ 只声明依赖 + 给可复现命令 ⇒ ⇒ 且 MUST 记录「本机就绪度 = **1/4**」+「官方 Docker 镜像路径**本机不可用**（但 CI runner 自带 docker ⇒ ⇒ 路径 ① 可行 · 且本仓 CI **无 docker 先例** ⇒ ⇒ 该 job 是 CI 改动）」+「本机 gcc 16.1.0 **高于**官方要求的 15 · 兼容性未验证」这三个实测事实（**不得**写「环境已就绪」）

---

## Capabilities

| capability | 变化 |
|---|---|
| **零 spec 变更** | ⇒ ⇒ **本项不改任何 capability**（只加**证据层** ⇒ 不改 ISA 语义 · 不改产品契约）⇒ ⇒ ⇒ **先例 = `openspec/changes/archive/2026-11-25-riscv-tests-gate-2026-11/`**（同类：加官方测试门禁 · 归档**无** `specs/`）⇒ ⇒ ⇒ ⇒ **判定口径由 `examples/riscv-mini/tests/riscv-arch-test/PROVENANCE.md` 承载**（不是 spec） |
| ~~`rv32i-isa-semantics`~~ | ⚠️ **已删（metis BLOCKER-2）** —— 原稿写「新增 capability `rv32i-arch-compliance`」+「零 spec delta」⇒ ⇒ **二者互斥**（无 ADDED delta ⇒ ⇒ 该 capability **永不落进 `openspec/specs/`** ⇒ ⇒ 是 vapor capability）⇒ ⇒ ⇒ **且「`verify_openspec_delta.sh` 会红」是误述** ⇒ ⇒ 实测 `scripts/verify_openspec_delta.sh:108` = `if [ -d "$dir/specs" ]` ⇒ ⇒ **`specs/` 缺席时该脚本直接跳过 = 零信号**（既不会红 · 也无规则检查「proposal 是否明写无 spec 变更」⇒ ⇒ 那是**约定俗成 · 非门禁**）⇒ ⇒ ⇒ ⇒ **本项选边 = 零 delta**（先例 = `openspec/changes/archive/2026-11-25-riscv-tests-gate-2026-11/`）⇒ ⇒ ⇒ ⇒ ⇒ **判定口径由 `PROVENANCE.md` 承载** ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **若将来要让判定口径进 spec** ⇒ ⇒ 需**真实 ADDED delta** ⇒ ⇒ ⇒ 那就是**另一个 change** |

> ⚠️ **「排除项登记义务」这个措辞已删**（metis SHOULD_FIX）⇒ ⇒ 本项的能力边界用 **UDB 声明**表达（见 S-E）⇒ ⇒ **不是**「排除项」清单 ⇒ ⇒ ⇒ 与 `riscv-tests` 侧的「排除项登记」是**两套不同机制**（那边 = 我们删测试 · 这边 = 我们声明能力）⇒ ⇒ ⇒ ⇒ **MUST NOT 混用口径**。

---

## Impact

### 影响面

| 面 | 影响 |
|---|---|
| **产品代码** | **零**（`include/**` / `src/**` **不改一行**）⇒ ⇒ ⇒ 本项是**纯证据层** ⇒ ⇒ ⇒ 风险面极小 |
| **新增文件** | `examples/riscv-mini/tests/riscv-arch-test/**`（env / config / images / `PROVENANCE.md`）· `examples/riscv-mini/tests/test_rv32i_arch_suite.cpp` · `scripts/verify_rv32i_arch_rtl.sh` |
| **改动的既有文件** | `improvement-suggestions.md`（索引登记）· `examples/riscv-mini/RUNNING-ELF.md` §8.6（R3 行状态）· `examples/riscv-mini/AGENTS.md`（RUN 段加一条命令）· 根 `AGENTS.md`（`CTEST_TOTAL` 锚点） |
| **计数锚点** | **2 个**：**`CTEST_TOTAL`**（新增 ctest target ⇒ +K）· **`EXAMPLES_TEST_SOURCES`**（新文件 `examples/riscv-mini/tests/test_rv32i_arch_suite.cpp` 命中计数口径 (c)「`examples/**/tests/` 下的任意 `.cpp`」⇒ ⇒ **24 → 25**）⇒ ⇒ ⚠️ **两者都必须与 phase-6 的收口批次串行**（锚点编辑权 = 最后合入者）⇒ ⇒ ⇒ 其余 3 个锚点与本项**无 contention** |

### 风险

| # | 风险 | 缓解 |
|---|---|---|
| **R1** | **16 KB I-TCM 装不下**（ACT4 一个 test file 含多个 testcase ⇒ 体积未知 ⇒ ⇒ 现有 `rv32ui-p-*`（每个几十条指令）**不能**作为容量证据） | ⇒ ⇒ **S-A 先量**⇒ ⇒ 第一杠杆 = `TESTCASES_PER_FILE`（生成侧参数）⇒ ⇒ **不动硬件** |
| **R2** | **Sail 环境装不上**（无 `docker` / `podman` ⇒ 只能本机装 ⇒ 需 OCaml 工具链） | ⇒ ⇒ S-A.3 的 go/no-go 会**如实暴露**⇒ ⇒ 若不可得 ⇒ ⇒ **本项降级为「不立项」并记录理由**（⇒ ⇒ **诚实失败优于半吊子**） |
| **R3** | **期望值退化为「测试自断言」** ⇒ ⇒ **零增量价值**（= 现有 `riscv-tests` 的形态） | ⇒ ⇒ S-A.3 把这条设成**硬门** ⇒ ⇒ 宁可不立项 |
| **R4** | **覆盖面反而更窄**（非特权子集 vs 现有 40 个已含 `fence_i` / `ma_data` 排除） | ⇒ ⇒ **如实登记**（MUST NOT 声称「覆盖面更大」）⇒ ⇒ 本项的价值定位 = **期望值的权威性**，不是覆盖面 |
| **R5** | **与 `riscv-tests` 门禁混淆** ⇒ ⇒ 「两套都绿」的含义被误读 | ⇒ ⇒ 能力名与 `PROVENANCE.md` MUST 明写「两套并存 · 判定面不同」 |
| **R6** | ⚠️ **失败路径的 `ebreak` ⇒ 本核会冻结**（`OP_SYSTEM` = 已知未实现 ⇒ `illegal_reason = 1` ⇒ ⇒ 流水线不前进）⇒ ⇒ runner 若只等 `tohost` ⇒ ⇒ **每条失败用例都变超时**（⇒ ⇒ 判据失真 + 单测上限超时） | ⇒ ⇒ **S-C.3 三类互斥判据**（`tohost == 3` / `illegal_reason != 0` / 其它挂死）⇒ ⇒ 且 **② MUST 在冻结那一拍判 FAIL**（**不等超时**）⇒ ⇒ ⇒ ⚠️ **判据面 MUST 用核级 `dbg_illegal_reason`** |
| **R7** | ⚠️ **`.text.rvmodel` 的落点 = 架构级冲突**（**已实测坐实 · 见 A.2**）⇒ ⇒ 上游把它放 `.data` 之后（数据侧）⇒ ⇒ 而本核取指**只切片、无区域检查**（`rv32i_core_top.h:204`）⇒ ⇒ D-TCM 取指 ⇒ **alias 回 I-TCM 复位向量** ⇒ ⇒ 而 `RVMODEL_HALT_*` **是会被 `jal` 调用的代码** ⇒ ⇒ ⇒ **落 D-TCM = 跑飞** | ⇒ ⇒ **草案裁决：`.text.rvmodel` MUST 显式放 I-TCM 侧**（⇒ ⇒ 偏离上游惯例 ⇒ ⇒ **理由 MUST 登记**）⇒ ⇒ ⇒ S-A.2 的容量判据 ①（I-TCM）**MUST 含它**、判据 ②（D-TCM）**MUST 不含它**⇒ ⇒ ⇒ ⚠️ **原稿把风险当成了缓解**（「确认它在 D-TCM 内」）⇒ ⇒ ⇒ ⇒ **已订正** |
| **R8** | ⚠️ **`.tohost` 的 orphan 放置**（ACT4 默认不显式放 `.tohost` ⇒ ⇒ 位置由链接器决定 ⇒ ⇒ 可能落进 I-TCM ⇒ ⇒ 判据地址漂移） | ⇒ ⇒ link.ld **MUST 显式**钉 `0x8001_0000`（S-B.4）⇒ ⇒ ⇒ 判据依赖此地址 ⇒ ⇒ ⇒ **MUST NOT** 依赖 orphan 行为 |

### 与既有未闭任务的关系（**不重复**）

| 既有任务 | 关系 |
|---|---|
| **phase-6 P6 / `rv32i-privileged-csr-2026-11`**（⬜ · CSR 特权级） | ⇒ ⇒ **本项的特权面被它阻塞** ⇒ ⇒ 本项**只出解锁条件**，不实施 |
| **phase-6 R7 / R11**（P6-0 侦察 · FENCE 裁决） | ⇒ ⇒ R11 **已闭**（ADR-071 Accepted）⇒ ⇒ 与本项零重叠 |
| **`rv32i-soc-platform-2026-11`**（🟡 in-progress · 下一批 = **S4a**） | ⇒ ⇒ **写点零重叠**（本项全在新文件）⇒ ⇒ **可并行** ⇒ ⇒ ⚠️ 但 **S5（IRQ）被 privilege 双重阻塞** ⇒ ⇒ ⇒ **CSR 同时是 S5 与本项特权面的共同前置** ⇒ ⇒ ⇒ **CSR 的优先级因此被本项进一步确认** |
| **`rv32i-isa-gap-closure-2026-11`**（partial） | ⇒ ⇒ 其 G 系列（未知 opcode / FENCE 显式化）**已落地** ⇒ ⇒ 本项**不重复** ⇒ ⇒ 但其「CSR 不在内」的边界 ⇒ ⇒ ⇒ 本项**不得**顺手把 CSR 塞进来 |

---

## 下一步（**planner 域 · 已排期**）

| 序 | 动作 | 归属 | 前置 |
|---|---|---|---|
| **1** | **S-A.0 环境 spike**（**认领 owner + 时限 + 产出** · 见 S-A.0）⇒ ⇒ **本项的入口** | builder（CI job 路线）/ planner + 用户（本地路线） | — |
| **2** | **S-A 尺寸 + 期望值来源 go/no-go**（**零产品代码改动 · 产物不进仓**） | planner → builder | S-A.0 |
| **3** | 依 S-A 结论**裁决本项是否立项**（绿 ⇒ **注册进 `improvement-suggestions.md` 索引** + 交 `rdd-planner` 审；红 ⇒ **记录降级理由**并关闭） | planner | S-A |
| **4** | S-B → S-C → S-D（适配 **5** 文件 → 核级门禁 → 交付物级门禁） | builder | S-A 绿 |
| **5** | **CSR 特权级 P6-2**（**独立 change** · 不在本项内）⇒ ⇒ 落地后**解锁**本项的 `Zicsr` / 特权子集 | builder | — |
| **6** | SoC 平台 **S4a**（**独立 change** · 与本项写点零重叠 ⇒ ⇒ 可并行）⇒ ⇒ **S5（IRQ）仍被 CSR 阻塞** | builder | — |

> ⚠️ **本草案不实施任何一步** —— 它是**规划载体** ⇒ ⇒ 实施由 `rdd-builder` P0 立项后开始。

---

## 实施期实测回填（**2026-11-25 · builder** · 追加订正 · **不改写上文历史叙述**）

### F1 · S-A.0 走了**第三条路径**（零安装）⇒ ⇒ 产出 = 判据 ①②

⚠️ **规划给的 S-A.0 只有两条路径**（① CI job 跑官方镜像 · ② 本地装 Sail）⇒ ⇒
**实施期发现第三条路径**：上游 `tests/env/signature.h` 是**纯宏模板**（不含 Sail 产出的期望值数值）
⇒ ⇒ 期望签名**只影响 `.data` 的运行时内容**、**不影响 `.text`** ⇒ ⇒
⇒ ⇒ **用本机 `riscv64-unknown-elf-gcc` + 官方 `cvw-rv32i` 的 `rvmodel_macros.h`，`-c` 汇编（不链接）即可拿到 `.text` 的精确体积**
⇒ ⇒ **39/39 个 `.S` 全部汇编成功** ⇒ ⇒ **零安装 ⇒ 零 Sail 依赖 ⇒ 零产品代码改动 ⇒ 产物全在 `/tmp/opencode/`（不进仓）**。

⇒ ⇒ **这不构成 `plan_drift`**（plan 的结构与 AC 均未变 · 只是判据 ①② 提前拿到了）⇒ ⇒ 故**继续执行并回写**，不停下交 planner。

### F2 · 判据 ①② 的实测结论（**上游默认切分下 = NO-GO**，但**可由杠杆救**）

| 判据 | 实测 | 判定 |
|---|---|---|
| ① I-TCM 容量 | 最大 `.text*` 合计 = **115,280 B（112.6 KB）**（`I-jal-00`）vs **16 KB** | ❌ **超 7.0 倍** |
| ② D-TCM 容量 | 最大 `.data` = **41,888 B（40.9 KB）** vs **16 KB** | ❌ **超 2.6 倍** |

⇒ ⇒ **但两区都随 testcase 数线性增长**（`.text.rvtest` ≈ 71.3 B/tc · `jal` = 956 B/tc（每 testcase **2 个** `RVTEST_SIGUPD`）· `.data` ≈ 4,840 B 固定 + 90.6 B/tc）⇒ ⇒
⇒ ⇒ ⇒ **`TESTCASES_PER_FILE` 是有效杠杆** ⇒ ⇒ 外推 **≈ 15~20 testcase/分片**（瓶颈 = `I-jal-00`）⇒ ⇒ **所需 ELF 总数 ≈ 109**（vs 默认 39 ⇒ **×2.8**）⇒ ⇒ **运行时外推 18 min（@10 s/ELF）~ 182 min（@100 s/ELF）** ⇒ ⇒ ⇒ **跨一个数量级 ⇒ ⇒ 判据 ④ 必须实测**。

⚠️ **探针局限 MUST 随结论一起读**（`rvtest_config.h` 是手造最小版 · `signature_stub.h` 含 4,096 B · 期望签名真实体积未测）⇒ ⇒ **但结论方向不受影响**（超一个数量级 ⇒ ⇒ 任何合理偏差都救不了）。

### F3 · 判据 ③④⑤ 仍阻塞 ⇒ ⇒ 综合判定 = **CONDITIONAL GO（未完成）**

| 判据 | 状态 |
|---|---|
| ③ `rv32i-fw-dump` exit 3 数 == 0 | ⛔ **未判**（需真实 ELF ⇒ 需 Sail） |
| ④ 运行时上限 + CI 落点 | ⛔ **未判**（需实测速率 ⇒ 外推 18~182 min **跨一个数量级**） |
| ⑤ 期望值来源 = Sail | ⛔ **未判**（无 Sail ⇒ 连 ELF 都产不出） |

⇒ ⇒ ⇒ **容量这条最关键的判据已不再是未知，且找到了明确杠杆** ⇒ ⇒ **值得投入 Sail 环境**
⇒ ⇒ ⇒ **但** 判据 ④ 一旦落在 100 s/ELF 量级 ⇒ ⇒ **≈ 182 min** ⇒ ⇒ **那将是第二种 no-go**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 判据 ④ 是投入前的最后一道闸门** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 建议：投入环境 spike 的第一个产出就同时量 ③④**（生成即量 ⇒ 不必等实现）
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⚠️ 「是否投 0.5 天环境 spike 换 ③④ 两条实测判据」属**用户 / architect（planner 域）**⇒ ⇒ ⇒ ⇒ ⇒ **builder 不自决**。

### F4 · 资产落点

**单一真相 = `examples/riscv-mini/tests/riscv-arch-test/PROVENANCE.md`**（本轮新建）
⇒ ⇒ 含：S-A.0 零安装侦察的**全部实测数字 + 复现命令 + 探针局限 + 判定表 + 环境就绪度 + 许可登记 + 未闭清单**。
