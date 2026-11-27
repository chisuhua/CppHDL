# Proposal: S4c-0 —— 双产品线骨架（per-line marker + 元护栏 + full 线空壳）

> **来源**：`.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 **S4c** 批次 +
> **ADR-078**（`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md` · **Accepted** 2026-11-27
> · oracle 1 轮 + critic 2 轮全闭 · 审稿记录 `.rddf/reviews/ADR-078-dual-product-lines-and-full-line-scope.md`）。
> **用户 2026-11-27 裁决**：平台形态 = **双产品线**（mini 冻结保留 + full 新建）·
> 命名 = `examples/riscv-full/`（IP 级）+ `examples/riscv-full-soc/`（SoC 级）· **旧名不改**。
> **前置**：S4b（`2026-11-27-rv32i-soc-peripherals-interconnect-2026-11`）**已归档**。
> **spec 变更**：新 capability **`rv32i-product-lines`**（ADDED ×5）。
> **范围**：**只做 ADR-078 的 D8 的 S4c-0**（产品线抽象 + 护栏参数化 + full 线空壳）⇒
> S4c-1（取指 fork + I-L1）/ S4c-2（DMA boot）/ S4c-3（全互联 + L2）**各自另立**。

---

## Why

1. **加第二条产品线的边际成本目前是「手改 7+ 处 + 重算 5 个全局锚点」** —— 仓内 14 个护栏 /
   工具写死了 `examples/riscv-mini` / `examples/riscv-soc` 路径（实测 12 sh + 2 py），
   且根 `AGENTS.md` 的 5 个 CI 整数锚点（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
   `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`）是**全局单值**
   ⇒ ⇒ 若不先解决，**每加一条线都要手改这些地方** ⇒ ⇒ 成本随线数线性增长，且**漏一处 = 静默不覆盖新线**
   （本仓最恐惧的失败族）。
2. **「静默不覆盖新线」已是既有失败模式** —— 仓内用 `VERILATOR_E2E_TOTAL` 等锚点的**唯一目的**
   就是防「双后端覆盖静默缩面」⇒ ⇒ 产品线维度 MUST 有**等价的防缩面机制**。
3. **登记面必须能被独立核对** —— 手写中央登记表在多会话并行下会冲突（仓内已有 `git add -A`
   跨会话事故先例）⇒ ⇒ 登记面 MUST 是**派生物**（从每条线自带的 marker 生成）⇒ ⇒ 单点真相在各自目录内。
4. **治理红线当前是「线无关」写法** —— 根 `AGENTS.md` 的「imem MUST `sread`（1 拍）」与 capability
   `rv32i-data-forwarding` 的 load-use 停顿矩阵**实为 mini 线专属**（full 线按设计违反它们）
   ⇒ ⇒ 不逐线 scope，未来每个会话都会读到自相矛盾的「红线」。
5. **散文表已在漂移** —— `examples/riscv-soc/AGENTS.md` 的 module 表仍写「`AxiLiteMatrix2x2` ·
   `AxiLiteGpio` = 12」，而磁盘已是 `AxiLiteCrossbar` + 6 外设 ⇒ ⇒ 同周内漂移且**无护栏捕获**。

## What Changes

### A. **per-line marker**（每条产品线自带真相）

每条产品线目录放一个 `.rddf-line.yaml`：字段 = `line`（标识）· `layer`（`ip` | `soc`）·
`adrs`（关联 ADR 列表）· `specs`（关联 capability 列表）。
⇒ 4 条线（`riscv-mini` / `riscv-soc` / `riscv-full` / `riscv-full-soc`）各一个 ⇒ **单点真相在各自目录内**。

### B. **生成器 + 登记面**（派生物 ⇒ 无手写中央表）

`scripts/gen_product_lines.py` 从 marker 派生 `.rddf/checks/product-lines.md`（登记面）：
产品线表 · 锚点归组表 · 脚本处置表指针。`--check` 模式 = 「生成物 == 发现集」判定。

### C. **元护栏**（`scripts/verify_product_lines.sh`）

判据：**C1** 发现集 == 生成集（marker 缺失 / 生成物陈旧 ⇒ 红）· **C2** marker 必填字段完备
（`layer` 合法 · `adrs` 指向的 ADR 文件存在）· **C3** 每条线 MUST 被**至少一个**护栏登记面引用
（防「登记了但不覆盖」）· **C4** `--self-test` 投毒自证（改 marker ⇒ 必红）。

### D. **14 个写死路径脚本的逐个处置登记**

处置分两类：**A 类 = mini-only by design**（full 线不复制该语料 ⇒ 不参数化，只登记）
· **B 类 = MUST 参数化**（按产品线分组 / 按线发现）。

| 脚本 | 写死内容 | 类 | 处置 |
|---|---|---|---|
| `verify_no_duplicate_rv32i_headers.sh` | **34 处**（C1/C4/C5/C6 扫描面写死 `examples/riscv-mini/src/**`） | **B** | 扫描面按产品线分组；C1 因 full 线 basename 独立而天然绿 |
| `verify_riscv_soc_core_ports.sh` | 10 处（`AGENTS_MD="examples/riscv-mini/AGENTS.md"` + 53 行表 + V4 basename 规则） | **B** | 端口表快照按产品线分组 |
| `rv32i_obs_vector.py` | 4 处（`CORE_HEADER` / `VECTOR_SRC`） | **B** | 按产品线取核头 + 观测向量源 |
| `dual_backend_parity.py` | 2 处（测试源清单）+ 全局锚点 | **B** | 清单按产品线；锚点改分组 |
| `verify_example_assertion_quality.sh` | **24 处**（测试源清单 + 期望描述） | **B** | 新线测试源 MUST 登记（漏登记 ⇒ 红） |
| `verify_example_test_wiring.sh` | 归属守恒（按 `examples/**` 扫） | **B** | 新线纳入归属守恒（守恒面按产品线） |
| `verify_examples_rtl_sim.sh` | tsv 登记 + self-test 临时清单 | **B** | 新线增 `kind=soc` 行 |
| `verify_hierarchical_rtl.sh` | 5 处（`RV32I_HIER_EMITTER` **已可覆盖**） | **B** | 新线语料登记 + 期望集分组 |
| `verify_rv32i_official_rtl.sh` | 5 处（riscv-tests 语料在 mini 线） | **A** | 登记为 mini-only by design |
| `verify_dtcm_read_timing.sh` | 2 处（mini 线专有发射器） | **A** | 登记为 mini-only by design |
| `verify_rv32i_fw_init_rtl.sh` | 3 处（mini 线专有发射器） | **A** | 登记为 mini-only by design |
| `verify_rv32i_elf_e2e.sh` | 2 处（`MINI_BIN` / `SOC_BIN` 具体 binary） | **A** | 登记为 mini-only by design（full 线有对应物时再加行） |
| `rv32i_rtl_sim_env.sh` | 1 处（mini 线专有 TB） | **A** | 登记为 mini-only by design |
| `probe_r21_hier_granularity.sh` | 1 处（`-I examples/riscv-mini/src`） | **A** | 一次性探针 ⇒ 不参数化 |

⇒ **B 类 8 个 = 本 change 的主体工作量**；A 类 6 个**只登记不改**。

### E. **5 个全局整数锚点归组**

根 `AGENTS.md` 的 CI anchor 段从**单值**改为**按产品线分组**（形如 `CTEST_TOTAL: mini=272, full=0`）
⇒ ⇒ 加一条线只改自己那组 ⇒ 且 `verify_test_counts.sh` 的比对改为**分组比对**（分组错 ⇒ 红）。

### F. **逐线 scope 标注**

根 `AGENTS.md` + `include/cpu/AGENTS.md` 的「imem MUST `sread`（1 拍）」类红线、capability
`rv32i-data-forwarding` 的 load-use 停顿矩阵 ⇒ 标注「**mini 线专属**」；full 线对应物随 S4c-1 建立。

### G. **散文表治理**

`examples/riscv-soc/AGENTS.md` 的 module 集表 ⇒ 替换为**指针**（指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`
的 module 集）⇒ 单一真相源唯一 ⇒ 散文漂移面消除。

### H. **full 线空壳**（**不含任何功能**）

`examples/riscv-full/`（IP 级）与 `examples/riscv-full-soc/`（SoC 级）：
marker + 构建目标 + **最小 SoC 顶层**（框架 2 + 极少端口 · 无核、无外设、无互连）
⇒ 目的 = 让新线**立即进入所有护栏的扫描面** ⇒ ⇒ 漏覆盖会**当场红**而不是静默。

## 验收标准

- [ ] **AC1 per-line marker**：4 条线各有 `.rddf-line.yaml` 且必填字段完备（`layer` ∈ {`ip`,`soc`} ·
      `adrs` 指向的 ADR 文件**存在**）⇒ 元护栏 C2 绿
- [ ] **AC2 登记面是派生物**：`.rddf/checks/product-lines.md` **由 marker 生成** ⇒
      `scripts/gen_product_lines.py --check` 绿 ⇒ ⇒ 手工改生成物 ⇒ 红
- [ ] **AC3 元护栏存在且有判别力**：`scripts/verify_product_lines.sh` 的 C1–C4 全绿，
      且 `--self-test` 投毒（改 marker / 删 marker / 改 `layer`）⇒ **必红**
- [ ] **AC4 脚本处置登记完备**：处置表 **14 行**（12 sh + 2 py）逐行有「写死内容 / 类（A|B）/ 处置」
      ⇒ 漏一行 ⇒ 元护栏 C2 红
- [ ] **AC5 B 类脚本已参数化**：8 个 B 类脚本**对 full 线不误红**且**对 mini 线行为不变**
      ⇒ 逐个用「full 线存在 / 不存在」两种状态各跑一次，退出码 MUST 相同
- [ ] **AC6 锚点分组**：根 `AGENTS.md` 的 5 个 CI 锚点按产品线分组，且 `verify_test_counts.sh`
      按**分组**比对 ⇒ 分组值与 `ctest -N` 实际一致 ⇒ 改错分组 ⇒ 红
- [ ] **AC7 逐线 scope 标注**：imem-`sread` 红线与 `rv32i-data-forwarding` 停顿矩阵标注「mini 线专属」
      ⇒ grep 可判
- [ ] **AC8 散文表治理**：`examples/riscv-soc/AGENTS.md` **不含**手写的 module 集表（只留指针）
      ⇒ 且该指针指向的期望源里的 module 集 == 磁盘实际（17）
- [ ] **AC9 full 线空壳可用**：`examples/riscv-full/` 与 `examples/riscv-full-soc/` 存在 ·
      CMake 目标可构建 · `--check-products` **PASS** · 产物 module 集 / 顶层端口表 == 登记值
- [ ] **AC10 mini 线零行为变化**：`ctest` **全绿**（285/285）· 5 个既有护栏全绿 ·
      mini 线产物端口表**逐字不变**
- [ ] **AC11 新线覆盖不缩面**：full 线的 example MUST 被 tsv / test-wiring / assertion-quality
      **三处登记** ⇒ 任一处漏登记 ⇒ 对应护栏红 ⇒ ⇒ 「加线漏登记 = 当场红」
- [ ] **AC12 AC 映射回填**：ADR-078 元数据的「关联 change」行指向本 change 并列出 AC 映射

### 与 ADR-078 的 AC 映射

| ADR-078 的 AC | 本 change 的 AC |
|---|---|
| AC2（目录存在） | **AC9**（目录 + 可构建 + 产物面登记） |
| AC3（加线成本 = 自有文件 + 登记条目 + **全局锚点重算**） | **AC1** · **AC2** · **AC6** |
| AC4（两线快照各自比对、互不串扰） | **AC5**（B 类按线分组）· **AC9** |
| AC13（full 线 basename 独立 ⇒ C1 天然绿） | **AC4**（处置表登记 C1 不需改造） |
| AC17（逐线 scope 标注） | **AC7** |
| AC18（登记表自身的元护栏） | **AC3** |
| AC19（散文表纳入期望源） | **AC8** |

⇒ ADR-078 的 AC1 / AC5 / AC6 / AC7 / AC8 / AC9 / AC10 / AC11 / AC12 / AC14 / AC15 / AC16
**分属 S4c-1..3**（或本 change 的 AC10 / AC12）⇒ 不在本 change 的判据面。

## Out of Scope（明确不做）

1. **取指侧总线化 / 取指接口 fork / I-L1 两处升级** ⇒ S4c-1（ADR-078 D7 / D14）
2. **`AxiDmaLiteRegs` / DMA boot / `AxiDma` 行为测试** ⇒ S4c-2（ADR-078 D6 / D11 / D12）
3. **full 互连 / L2 / DRAM / `AxiInterconnect4x4`** ⇒ S4c-3（ADR-078 D5）
4. **扩 `AxiLiteCrossbar` 到 2×7** ⇒ 随 S4c-2（D10 · 唯一改动 mini 回归面的决策）
5. **旧线改名**（`riscv-mini` → `riscv-mini-ip` 等）⇒ ADR-078 D9 = **不做**（登记 backlog）
6. **mini 线的任何行为 / 产物 / 端口面改动** ⇒ 本 change 的 AC10 是**零变化**判据
