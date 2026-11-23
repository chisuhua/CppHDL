# Tasks: ELF → init_data 端到端装载链路（方案 a）

> **实施顺序固定**（见 `design.md` §3）。⚠️ 每条判据 MUST 带**投毒/反向对照**
> （「判据恒绿」是本批要治的**根因**）。
> ⚠️ 与 `riscv-soc-skeleton-2026-11` **无冲突**（后者只动 SoC 骨架与观测 IP）。

## S0 — 开工前基线（**零改动**）

- [x] 记录基线：`ctest -L base` 总数 · 现有 `verify_rv32i_fw_dump.sh` 判据数 · 工具三种格式
      ⇒ ⇒ **2026-11-23 实测**：`ctest -LE perf` **262** · D0–D6 · `mem`/`vh`/`manifest`
- [x] 确认工作区干净（`git status --short`）⇒ ⇒ **禁** `git add -A`
      ⇒ ⇒ **2026-11-23**：仅 1 个**他人会话**的未跟踪报告（`.rddf/reviews/riscv-soc-skeleton-2026-11-verify.md`）
      ⇒ ⇒ 本批**不**纳入 `git add`（按路径显式 add）

```bash
cd «本机»
ctest --test-dir build -N -LE perf | tail -1
grep -cE '^echo "── D[0-9]+' scripts/verify_rv32i_fw_dump.sh
```

## S1 — 工具：修 4 个 MUST 违例

- [x] **`regions` 判据**改为「本段落在哪个区域 · 且该区域尚未登记」
      ⇒ ⇒ **2026-11-23 已实施**（`tools/rv32i-fw-dump/main.cpp` · `has_region`）
      ⇒ ⇒ 判据 = **D7**（双段 ELF ⇒ `regions[]` 含 0x80000000 + 0x80010000）
- [x] **入口 MUST == `kPcReset`**（否则 exit 3）⇒ ⇒ 判据在 `validate()`（ELF / JSON 共用）
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ 判据 = **D8**
- [x] **重定位检查**改为读 **section header**（非空 `SHT_RELA`/`SHT_REL` ⇒ exit 3）
      ⇒ ⇒ **2026-11-23 已实施**（此前是 `for (…) { (void)k; }` 空循环）⇒ ⇒ 判据 = **D9**
- [x] **`mem` / `vh` 多区域 ⇒ exit 3 + 诊断** ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ 判据 = **D10**
      + **D12**（反查：单区域 + `mem` 仍成功 ⇒ ⇒ 判据不是「一律拒绝」）

```bash
cd «本机»
cmake --build build --target rv32i-fw-dump -j"$(nproc)"
bash scripts/verify_rv32i_fw_dump.sh
```

## S2 — 工具：新增 `--format mem-split`

- [x] `mem-split` 产出 `<dir>/itcm.mem` + `<dir>/dtcm.mem` · **行号 = 区域字索引** · 空洞补零
      ⇒ ⇒ **2026-11-23 已实施**（`write_mem_split` · `(addr - base) >> 2` **唯一一处**）
- [x] 字节 → 字索引换算**只在一处**（禁各处自行解释）
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ 判据 = **D11**（I-TCM 4 字 · D-TCM 索引 64 = 0x5a · 前 64 行零）

```bash
cd «本机»
cmake --build build --target rv32i-fw-dump -j"$(nproc)"
bash scripts/verify_rv32i_fw_dump.sh
bash scripts/verify_rv32i_fw_dump.sh --self-test
```

## S3 — 核级 TB：`--firmware <dir>`

- [x] 读两个镜像 ⇒ 构造参数 ⇒ `init_data` ⇒ **零 `prog_*` 写**
      ⇒ ⇒ **2026-11-23 已实施**（`tb_rv32i_soc.cpp` · `read_mem_image` / `pad_itcm`）
- [x] ITCM 尾部填 NOP（G1 纪律：末尾 0 = 未知 opcode ⇒ 冻结）
      ⇒ ⇒ **2026-11-23 已实施**（`pad_itcm` 到 `kFillWords` = 64）
- [x] 打印**协议行** `FIRMWARE-TRACE:`（TB **不**硬编码固件期望值）
      ⇒ ⇒ **2026-11-23 已实施**（`print_firmware_trace`）
- [x] `--emit-rtl --firmware <dir>`（镜像进产物 `initial` 块）
      ⇒ ⇒ **2026-11-23 已实施**

```bash
cd «本机»
cmake --build build --target tb_rv32i_soc -j"$(nproc)"
build/examples/riscv-mini/tb_rv32i_soc            # 默认模式 24 条判据不回归
```

## S4 — SoC 级：init 透传 + `--firmware`

- [x] `Rv32iSocTop` 新增 `itcm_init` / `dtcm_init` **构造参数**（**零端口变更**）
      ⇒ ⇒ **2026-11-23 已实施**（`ch_layer<T, const vector&, const vector&>` 显式给 `Args…`）
      ⇒ ⇒ 判据 = E4a（产物顶层端口表与基线**逐字逐序**相同 · 34 个）
- [x] `tb_riscv_soc --firmware <dir>`（验证 + 发射）· 协议行 `SOC-FIRMWARE-TRACE:`
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ 判据 = **E3**（store_count=1 / writeback_count=2 手推）

```bash
cd «本机»
cmake --build build --target tb_riscv_soc -j"$(nproc)"
build/examples/riscv-soc/tb_riscv_soc            # 默认模式 A1–A6 不回归
```

## S5 — 端到端护栏 `verify_rv32i_elf_e2e.sh`

- [x] **E0** 二进制齐备（fail-closed）· **E1** mem-split 镜像（手推）
      ⇒ ⇒ **2026-11-23 已实施**
- [x] **E2** 核级：`.data` 经 `lw` 读回并**写回**（x1=0x5A / x2=0x5B / store=0x104:0x5B）
      ⇒ ⇒ **2026-11-23 已实施**
- [x] **E4** 产物级：(a) 顶层端口表逐字相同 · (b) `initial` 块携带 ELF 派生的字
      （4 程序字 + 60 NOP + D-TCM[64]=0x5a）
      ⇒ ⇒ **2026-11-23 已实施**
- [x] **E5** 投毒自证（改坏 `.text` / 改坏 `.data` / 去掉 `.data` ⇒ 判据各自转红）
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ `--self-test` 全绿
- [x] **E6** 真工具链 leg（`riscv64-unknown-elf-gcc`）· **env-gated**（无工具链 ⇒ SKIP + 说明）
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ 本机实测跑通（同组手推期望值）

```bash
cd «本机»
cmake --build build --target rv32i-fw-dump tb_rv32i_soc tb_riscv_soc -j"$(nproc)"
bash scripts/verify_rv32i_elf_e2e.sh
bash scripts/verify_rv32i_elf_e2e.sh --self-test
```

## S6 — CI 入口 + 可达性登记（**双向 1:1**）

- [x] `.github/workflows/ci.yml` 加 3 个 step（fw_dump self-test · e2e · e2e self-test）
      ⇒ ⇒ **2026-11-23 已实施**
- [x] `.rddf/checks/guard-reachability.md` 同步（新护栏行 + fw_dump 的第二调用点）
      ⇒ ⇒ **2026-11-23 已实施** ⇒ ⇒ `verify_guard_reachability.sh` **PASS**

```bash
cd «本机»
bash scripts/verify_guard_reachability.sh
```

## S7 — 收口

- [x] 全量重建 + 非 perf 全绿 ⇒ ⇒ **2026-11-23**：`ctest -LE perf` **262/262 PASS**
- [x] 关键护栏不回归 ⇒ ⇒ `verify_rv32i_fw_init_rtl.sh` · `verify_test_counts.sh` ·
      `verify_example_test_wiring.sh` · `verify_port_naming.sh` **全绿**
- [x] 文档同步：`docs/CHANGELOG.md` · `examples/riscv-mini/AGENTS.md` ·
      `examples/riscv-soc/AGENTS.md` · `docs/developer_guide/rv32i-rtl-delivery-and-sim.md`
      ⇒ ⇒ **2026-11-23 已实施**
- [x] `openspec validate` + `verify_openspec_delta.sh` + archive
      ⇒ ⇒ **2026-11-23 已实施**

```bash
cd «本机»
cmake --build build -j"$(nproc)"
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_openspec_delta.sh rv32i-elf-initdata-e2e-2026-11
openspec validate rv32i-elf-initdata-e2e-2026-11 --strict
```

## Out of Scope（**本 change 不做**）

- ❌ 删除 `prog_*` 根端口（ADR-077 D3 裁决**长期保留**）
- ❌ 运行期统一装载口 / boot ROM（登记为后续）
- ❌ 总线 / DMA 装载（ADR-077 D3 的 S3 **独立轨道**）
- ❌ 符号表解析
- ❌ `BootROM` stub 处置（`rv32i-dead-component` 类 backlog）
- ❌ Verilator 侧的行为判据（需 verilator ⇒ 既有 env-gated 轨道承担）
