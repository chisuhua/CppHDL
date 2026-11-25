# PROVENANCE —— `riscv-arch-test`（ACT4）适配资产

> SPDX-License-Identifier: BSD-3-Clause
>
> 本目录 = **`riscv-arch-test`（ACT4）的 `rv32i` 非特权子集**适配资产。
> 判据载体 = `.rddf/improvements/rv32i-arch-test-certification-2026-11.md`（规划）
> ⇒ ⇒ **本文件是「资产 + 来源 + 实测结论」的单一真相**。

## 0. 状态

| 项 | 值 |
|---|---|
| 阶段 | **S-A.0 · 环境侦察**（**已完成一轮**）⇒ ⇒ **S-B / S-C / S-D 未开工** |
| 上游 | `riscv/riscv-arch-test` · 分支 **`act4`** · HEAD **`98a1ec7`**（2026-11-25）· 许可 **BSD-3-Clause / Apache-2.0**（代码）+ **CC-BY-4.0**（文档） |
| 生成侧环境 | ⬜ **未搭建**（本机 `sail_riscv_sim` / `mise` / `uv` / `docker` **全缺** ⇒ 见 §3） |
| 判定面 | **核级**（`examples/riscv-mini`，53 端口含 `obs_data_*`）⇒ ⇒ **不是** SoC 级 |

---

## 1. S-A.0 的**零安装容量侦察**（2026-11-25 · **实测**）

> ⚠️ **本轮走的是规划的 S-A.0 两条路径之外的第三条**（零安装）⇒ ⇒ **产出 = 容量判据 ①②** ⇒ ⇒
> **判据 ③（`rv32i-fw-dump` exit 3）/ ④（运行时）/ ⑤（期望值来源）仍需真实 ELF ⇒ ⇒ 仍阻塞于 Sail 环境**。

### 1.1 为什么能零安装

- `tests/env/signature.h` 是**纯宏模板**（不含 Sail 产出的期望值数值）⇒ ⇒ 期望签名**只影响 `.data` 的运行时内容**，**不影响 `.text`** ⇒ ⇒ **汇编不需要 Sail**。
- ⇒ ⇒ 用本机 `riscv64-unknown-elf-gcc`（16.1.0）+ 官方 `cvw-rv32i` 的 `rvmodel_macros.h` ⇒ ⇒ **`-c` 汇编（不链接）** ⇒ ⇒ 拿到 `.o` 的**精确 section 体积**。
- ⇒ ⇒ **39/39 个 `.S` 全部汇编成功**（`compiled_ok=39 failed=0`）。

### 1.2 实测数据

**上游默认切分下**（`tests/rv32i/I/` = **39** 个 `.S`，**每个只有 `-00` 单分片** ⇒ ⇒ 上游默认未分片）：

| 指标 | 实测最大 | 本核容量 | 判定 |
|---|---:|---:|---|
| `.text*` 合计（`.text.init` + `.text.rvtest` + `.text.rvmodel`） | **115,280 B（112.6 KB）** · `I-jal-00` | I-TCM **16 KB** | ❌ **超 7.0 倍** |
| `.data` | **41,888 B（40.9 KB）** | D-TCM **16 KB** | ❌ **超 2.6 倍** |

**逐文件**（前 6）：

| 文件 | `.text.rvtest` | `.text*` 合计 | `.data` | `SIGUPD_COUNT` |
|---|---:|---:|---:|---:|
| `I-jal-00` | 114,688 | **115,280** | 15,712 | 120 |
| `I-beq-00` | 49,152 | 49,744 | 35,248 | 420 |
| `I-bge-00` | 49,152 | 49,744 | 35,248 | 420 |
| `I-bgeu-00` | 49,152 | 49,744 | 35,504 | 420 |
| `I-blt-00` | 49,152 | 49,744 | 35,248 | 420 |
| `I-xor-00` | 28,672 | 29,264 | — | 402 |

### 1.3 线性关系（**用于外推**）

| 量 | 固定部分 | 每 testcase |
|---|---:|---:|
| `.text.rvtest` | ≈ 0 | **71.3 B**（单 `RVTEST_SIGUPD`）/ **956 B**（`jal` = **每 testcase 2 个** `RVTEST_SIGUPD` ⇒ 2×478） |
| `.data` | ≈ **4,840 B**（env 公共部分） | **≈ 90.6 B**（每个 testcase 一个 debug 串 `"test: N; cg: ...; cp: ...; bin: ..."` ≈ 48 B + 签名/指针表） |

⇒ ⇒ **两区都随 testcase 数线性增长** ⇒ ⇒ ⇒ **`TESTCASES_PER_FILE` 是有效杠杆**。

### 1.4 外推（⚠️ **外推 · 非实测**）

| 约束 | 预算 | 结果 |
|---|---|---|
| I-TCM | 16 KB − `.text.init`(12) − `.text.rvmodel`(580) − 余量(1 KB) ≈ **14,768 B** | 瓶颈 = `I-jal-00`（956 B/tc）⇒ ⇒ **每分片 ≈ 15 testcase** |
| D-TCM | 16 KB − 余量(1 KB) ≈ **15,360 B** | ≈ **169 testcase**（**不是**瓶颈） |
| **⇒ 需要的 `TESTCASES_PER_FILE`** | — | **≈ 15~20**（取 16 保守） |
| **⇒ 所需 ELF 总数** | — | **≈ 109**（vs 上游默认 **39** ⇒ ⇒ **×2.8**） |
| **⇒ 运行时** | @10 s/ELF（现有 40 镜像实测 405.6 s / 40） | ≈ **18 min** |
| | @100 s/ELF（草案初版估） | ≈ **182 min** ⇒ ⇒ **超日常回归预算** |

⇒ ⇒ ⇒ **判据 ④（运行时）的实际值取决于真实单 ELF 耗时** ⇒ ⇒ **必须实测** ⇒ ⇒ arch-test 的 testcase 数约为现有 `rv32ui-p-*` 的 **10 倍**（402 vs ~10）⇒ ⇒ ⇒ **大概率落在 30~100 s/ELF ⇒ ⇒ 55~180 min** ⇒ ⇒ ⇒ **这是「装得下但跑不完」的具体形态**。

### 1.5 探针的**已知局限**（⚠️ MUST 登记 · 防止结论被误用）

| # | 局限 | 影响 | 处置 |
|---|---|---|---|
| L1 | `rvtest_config.h` 是**手造的最小探针版**（非 UDB 生成物）⇒ ⇒ env 公共部分的代码量可能与真实生成有偏差 | `.text` 体积有**未知偏差** | ⚠️ **结论方向不受影响**（超一个数量级 ⇒ ⇒ 任何合理偏差都救不了）⇒ ⇒ 但**精确值待真实生成后重测** |
| L2 | `signature_stub.h` = 1024 × `.word 0`（4,096 B）⇒ ⇒ **实测 `.data` 含这 4,096 B** | `.data` 实测值**偏高** | 真实 `.data` = 实测 − 4,096 + (`SIGUPD_COUNT` × `SIG_STRIDE`) ⇒ ⇒ **方向不变** |
| L3 | 期望签名的**真实体积未实测**（需 Sail） | 只影响 `.data` | 已在 §1.4 计入 `SIGUPD_COUNT` 量级 |
| L4 | 探针产物（`.o` / stub / 上游 clone）全在 `/tmp/opencode/` | **不进仓** | ✅ 符合「零产品代码改动 · 产物不进仓」 |

### 1.6 复现命令（**本地 · 零安装**）

```bash
# ① 上游 clone（不进仓）
git clone -b act4 https://github.com/riscv/riscv-arch-test /tmp/opencode/rat-act4

# ② 探针头（最小 rvtest_config.h + 空签名 stub）
#    → /tmp/opencode/arch-probe/{rvtest_config.h, signature_stub.h}

# ③ 39 个 .S 汇编（不链接 ⇒ 不需要 Sail）
cd /tmp/opencode/arch-probe
for f in /tmp/opencode/rat-act4/tests/rv32i/I/*.S; do
  b=$(basename "$f" .S)
  riscv64-unknown-elf-gcc -march=rv32i_zicsr_zifencei -mabi=ilp32 -c \
    -DRVTEST_SELFCHECK -DSIGNATURE_FILE='"signature_stub.h"' \
    -DTEST_FLEN=32 -DTEST_FILE="\"$b\"" \
    -I. -I/tmp/opencode/rat-act4/tests/env \
    -I/tmp/opencode/rat-act4/config/cores/cvw/cvw-rv32i \
    "$f" -o "$b.o"
done

# ④ 量体积
riscv64-unknown-elf-size -A I-add-00.o
```

---

## 2. **S-A.0 的判定结论**（**部分可判**）

| 判据 | 状态 | 结论 |
|---|---|---|
| ① 容量（I-TCM） | ✅ **已判** | ❌ **上游默认切分下 NO-GO**（112.6 KB vs 16 KB）⇒ ⇒ **但可由 `TESTCASES_PER_FILE ≈ 15~20` 救** |
| ② 容量（D-TCM） | ✅ **已判** | ❌ **上游默认切分下 NO-GO**（40.9 KB vs 16 KB）⇒ ⇒ **但可由同一杠杆救** |
| ③ `rv32i-fw-dump` exit 3 数 == 0 | ⛔ **未判** | **需真实 ELF**（需 Sail 环境） |
| ④ 运行时上限 + CI 落点 | ⛔ **未判** | **需真实 ELF + 实测速率** ⇒ ⇒ 外推 18~182 min（**跨一个数量级** ⇒ ⇒ 必须实测） |
| ⑤ 期望值来源 = Sail | ⛔ **未判** | **需 Sail 环境** ⇒ ⇒ 「无 Sail ⇒ 连 ELF 都产不出」 |

⇒ ⇒ ⇒ **综合判定 = CONDITIONAL GO（未完成）**
⇒ ⇒ **容量这条最关键的判据已不再是未知**，且**找到了明确杠杆** ⇒ ⇒ **值得投入 Sail 环境**（判据 ③④⑤）
⇒ ⇒ **但** ⇒ ⇒ 判据 ④ 一旦实测落在 100 s/ELF 量级 ⇒ ⇒ ⇒ **≈ 182 min** ⇒ ⇒ ⇒ **那将是第二种 no-go** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 所以判据 ④ 是投入前的最后一道闸门** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 建议：投入 Sail 环境的**第一个**产出就同时量 ③④**（生成即量 ⇒ 不必等实现）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 决策点 = 「投入 0.5 天环境 spike，换 ③④ 两条实测判据」是否值得** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **该决策属用户 / architect（planner 域）** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **builder 不自决**。

---

## 3. 生成侧环境（**独立 work item · 本项不实施**）

| 工具 | 要求 | 本机 | 备注 |
|---|---|---|---|
| Sail | **0.16**（可执行名 `sail_riscv_sim`） | ❌ | **硬依赖** ⇒ ⇒ 无它 ⇒ ⇒ 连 ELF 都产不出（生成是「两遍」） |
| `mise` / `uv` | 框架是 **uv workspace** | ❌ | |
| Ruby + `riscv-unified-db` | UDB gem | ❌ | |
| `riscv64-unknown-elf-gcc` | 官方要求 **GCC 15 / Binutils 2.44**（或 Clang 22） | ✅ **16.1.0**（**高于要求 · 兼容性未验证**） | 探针已验证 16.1.0 可汇编 39/39 |
| 容器 | 官方预构建镜像 `ghcr.io/riscv/act4-build:latest` | ❌ 本机无 docker/podman | ⚠️ **CI runner（`ubuntu-22.04`）自带 docker** ⇒ ⇒ 「CI 里跑官方镜像」是可行路径（⚠️ **本仓 CI 无 docker 先例** ⇒ ⇒ 属 CI 改动） |

---

## 4. 许可

上游 `riscv-arch-test`：**BSD-3-Clause / Apache-2.0**（代码）+ **CC-BY-4.0**（文档）⇒ ⇒ **将来镜像入库时 MUST 随附许可全文**（先例 = `tests/riscv-tests/LICENSE.riscv-tests`）。
⚠️ 本文件当前**不含**上游代码副本（只记录元数据与实测数字）⇒ ⇒ 许可全文的随附义务在**镜像入库时**触发。

---

## 5. 未闭 / 阻塞

| # | 项 | 阻塞于 | 归属 |
|---|---|---|---|
| 1 | 判据 ③ ④ ⑤ | **Sail 环境** | 用户 / architect（投入决策）· builder（搭建） |
| 2 | `TESTCASES_PER_FILE ≈ 15~20` 的**实测验证**（外推未证） | 同上 | builder（S-B 阶段） |
| 3 | `.text.rvmodel` 落 I-TCM 的**偏离登记** | S-B 阶段 | builder（写 `link.ld` + 本文件） |
| 4 | 生成侧环境的搭建 | — | **独立 work item**（本项只声明依赖） |
| 5 | **特权面**（`Zicsr` / Exceptions） | `rv32i-privileged-csr-2026-11` 的 **P6-2** | 独立 change |
