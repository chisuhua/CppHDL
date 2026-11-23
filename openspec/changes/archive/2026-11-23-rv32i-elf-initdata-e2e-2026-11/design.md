# Design: ELF → init_data 端到端装载链路（方案 a）

## 1. 为什么是「路线 A 的接缝」而不是新装载路径

`firmware-load-chain`（2026-11-14）已裁决 **路线 A**：
「初值经 `ch_mem::init_data` ⇒ 产物自带 `initial` ⇒ TB 零装载动作 ⇒ 零端口变更」。

⇒ ⇒ 路线的**方向**成立；缺的是**上游那一端**：
ELF 怎么变成 init_data 的**初值镜像**。本设计只补这一端，不改路线。

**被否掉的替代方案**（与本项独有取舍，非泛泛列举）：

| 方案 | 否决理由 |
|---|---|
| 让工具**直接链 `examples/**`** 或内嵌一个固件工厂 | 违反 `rtl-emit-product-tooling` C1（工具 MUST 只链库 ⇒ 才能当自测载体）。⇒ ⇒ 接缝改走**文件契约** |
| 由**发射器读固件文件**填充初值 | 发射是**建图期**行为；让库层 CLI 读外部文件会把「固件内容」耦合进发射器，并破坏「同一设计 ⇒ 产物逐字节确定性」。⇒ ⇒ 初值 MUST 在**构造参数**层面进入 |
| 升级 `mem` 格式为带 `@地址` 注解的 readmemh 形态 | 可行但改动现有 D1 判据（手推 3 字逐行比对）与「纯 hex 字镜像」语义；**按区域拆分 + 文件名承载归属**对消费者更简单 ⇒ ⇒ 取 `mem-split` |
| 运行期统一装载口 / boot ROM | 需动被 C6 快照冻结的 34 端口面 + 新 change；路线 A 已覆盖 capability 本意 ⇒ ⇒ 登记为后续 |
| 总线 / DMA 装载 | ADR-077 D3 已登记的 S3 **独立轨道** |
| 把 capability 的「非零重定位 MUST 报错」**降级**为「已知边界」 | 成本高于实现 SHT_RELA 子集，且削弱对「看起来装好了、实际地址全错」这一 failure mode 的防护 |

## 2. 契约形状（数据流）

```
ELF(.text/.data/.bss)
  │  rv32i-fw-dump --format mem-split --out <dir>
  ▼
<dir>/itcm.mem   ← 行号 = I-TCM 内字索引（空洞补零）
<dir>/dtcm.mem   ← 行号 = D-TCM 内字索引（空洞补零）
  │  tb_* --firmware <dir>
  ▼
Rv32iPipelineTop / Rv32iSocTop(itcm_init, dtcm_init)   ← 构造参数，零端口变更
  ▼
ch_mem<…>(init_words)  →  codegen print_mem_init  →  product 的 initial begin … end
  ▼
核取指 / lw / sw        ← 判据：真实读通路读回（寄存器写回），不是「loader 返回成功」
```

### 2.1 为什么 `mem-split` 必须补零空洞

`ch_mem::init_data` 的语义是**下标 = 字索引**。若 `.data` 落在 D-TCM 偏移 `0x100`
（字索引 64）而镜像只输出 1 行，则装载后它会被装到**索引 0** ⇒ ⇒ 程序 `lw 0x100`
读到 0 ⇒ ⇒ **静默错值**。⇒ ⇒ 必须输出 64 行零 + 第 65 行的值。

⚠️ 换算 `(byte_addr - region.base) >> 2` **只允许在一处**（工具）。
理由：三条路径（`init_data` 下标 / `prog_*` 字地址 / 核的字节地址）对「地址单位」的
解释各不相同，各写一份必然漂移。

### 2.2 为什么 TB 要补齐 ITCM 尾部（NOP）

I-TCM 在程序末尾之后读到的是 `ch_mem` 初值 **0** ⇒ ⇒ opcode `0x00` = **未知 opcode**
⇒ ⇒ 流水线在此**冻结**（G1 纪律）。⇒ ⇒ 镜像 MUST 补 NOP。
生产与仿真两侧都补（否则产物与仿真的行为分叉）。

### 2.3 为什么 TB 不硬编码固件期望值

`tb_rv32i_soc --firmware` 只断言**固件无关**的不变量
（首字 == 镜像首字 / 未取指到 `0x00000000` / 退休数 > 0 / 有写回），
并打印协议行 `FIRMWARE-TRACE: …`。**固件专属**的期望值（`x1=0x5A` 等）
只存在于**护栏**（`verify_rv32i_elf_e2e.sh`）⇒ ⇒ 换固件不必改 TB。

### 2.4 为什么 SoC 级只判「计数」

ADR-072 D1：SoC 根端口面 = 框架 2 + 根自有 6 ⇒ ⇒ 核的 `dbg_*` / `obs_*` **不是**
顶层端口 ⇒ ⇒ SoC 级**看不到** `.data` 读回。
⇒ ⇒ SoC 级判据只能是「固件真的在跑」（`store_count` / `writeback_count`）；
`.data` 的承重判据在**核级**。

## 3. 失败模式与处置

| 失败模式 | 处置 | 判据 |
|---|---|---|
| manifest 丢区域 | `regions` 判据改为「本段所属区域 · 且未登记」 | D7 |
| 入口错 ⇒ 装出砖 | `entry == kPcReset`，否则 exit 3 | D8 |
| 静默忽略重定位 | 读 section header，非空 REL/RELA ⇒ exit 3 | D9 |
| `.data` 静默装进 I-TCM | 多区域 + `mem`/`vh` ⇒ exit 3 | D10 |
| 索引错位（空洞未补零） | `mem-split` 补零到最高索引 | D11 |
| 判据恒绿（无判别力） | 每条判据配**投毒**（正反互换 ⇒ 必红） | `--self-test` / E5 |
| 仓外工具链阻塞 CI | 真工具链 leg **env-gated**（无工具链 ⇒ SKIP + 说明） | E6 |

## 4. 已知边界（如实登记）

- **`e_shnum == 0`（节头被 strip）** ⇒ ⇒ 重定位无从查起 ⇒ ⇒ 接受。属**已知边界**，
  不是「漏做」：ELF32 的 program header 不携带重定位表信息。
- **`mem-split` 的镜像长度 = 最高字索引 + 1** ⇒ ⇒ 稀疏 `.data`（如偏移 `0x1000`）会
  产出较长的前导零。这是 `ch_mem::init_data` 语义的**直接代价**，不是缺陷。
- **SoC 级观测面只有两个计数**（ADR-072 D1）⇒ ⇒ SoC 级无法判 `.data` 读回（见 §2.4）。
- **Verilator 侧行为判据**不在本项：本项只判「产物端口面 + `initial` 块内容」。
  产物的行为等价由既有的 `verify_examples_rtl_sim.sh` / `verify_rv32i_fw_init_rtl.sh`
  在 env-gated 轨道上承担。
