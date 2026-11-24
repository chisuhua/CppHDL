# RV32I 产品级 RTL 产物指纹基线（S3 之后 · **S4 的对比基线**）

> **记录时间**：2026-11-23 · **commit** `7d2a453`（S3 归档后）· **记录人**：build agent
> **性质**：**产品级 RTL 产物的字节指纹** ⇒ 用途 = **定位**「产物是否被改动」，
> **不是判据**（判据见 §3）。

## 1. 记的是什么

仓库里的**三份产品级 RTL 产物**（每一份 = 一个可交付的产品单元）：

| # | 产物 | 生成命令 | 形态 | module 数 |
|---|---|---|---|---|
| **P1** | **核 · 扁平** `top.v` | `build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --out <dir>` | 单 module（顶层内联） | **1** |
| **P2** | **核 · 层次化** `top.v` | `build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --out <dir>` | 核复合体成层 | **8** |
| **P3** | **SoC · 层次化** `top.v` | `build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out <dir>` | ADR-077 D1 两级顶层 | **10** |

## 2. 指纹（**S4a-0 之后** · S4 进行中）

| 产物 | sha256（**S4a-0 之后**） | `port_table[]` 条数 |
|---|---|---|
| P1 `flat/top.v` | `43fb0cb96afef77ff5613f709ba66a9c03571819879e5cb9daf9d7f2e273ec09` | — |
| P1 `flat/manifest.json` | `4a17a536b1d8122b8cb42623af2b17a20bebee998147e11ece67560ee79c906b` | **34** |
| P2 `hier/top.v` | `4fc0b31f85ce6fc277f2cda7eb624716319a385485c7f7fd6650115749e0727c` | — |
| P2 `hier/manifest.json` | `4a17a536b1d8122b8cb42623af2b17a20bebee998147e11ece67560ee79c906b` | **34** |
| P3 `soc-hier/top.v` | `36d3d4b7072d2c69d0951052d0cd2a478a0d8730db6a0dc10bf5122d991f80ba` | — |
| P3 `soc-hier/manifest.json` | `c8d9b5f7223490fa6ca4195efd7fd1a4d06986870e8d2257c341ecf9b5ea2647` | **8** |

⚠️ **P1 与 P2 的 `manifest.json` 指纹相同**（`4a17a53…`）⇒ ⇒ 这是**结构性的**：
两种形态的**顶层端口面逐字相同** ⇒ ⇒ **manifest 的指纹对「形态」不敏感**。
⚠️ **`top.v` 的指纹对「形态」敏感**（P1 ≠ P2 ≠ P3）⇒ ⇒ 形态/层次变化只改 `top.v`。

> ⚠️⚠️ **2026-11-24 订正（S4a-0 实测 · 本块优先于本节其余表述）** ——
> 本节原先写「`manifest.json` **只记**顶层端口表（`port_table[]`）」⇒ ⇒ **该表述对
> `rtl-emit-manifest-v2` 是错的**：manifest 有**两份**清单：
> - `port_table[]` = **顶层端口表**（P1/P2 = **34** · P3 = **8**）⇒ ⇒ 这是**产品端口面**；
> - `ports[]` = **节点清单**（P1/P2 = **284** · P3 = **294** 条，含 `driven` / `effective` /
>   `owner` / `is_top_io` / `semantic`）⇒ ⇒ **数据通路 / 观测面逻辑改动会增删节点** ⇒ ⇒ `ports[]` 变。
> ⇒ ⇒ ⇒ **后果**：`manifest.json` 的**整体 sha 变 ≠ 端口面变了**（S4a-0 实测：`port_table` 仍
> 34 / 34 / 8 **且** C6 快照 / R8′ / `verify_riscv_soc_core_ports` 三处**全绿**，但 manifest sha 变了）
> ⇒ ⇒ ⇒ **判端口面 MUST 看 `port_table[]` 条数 + 那三条结构化判据**，MUST NOT 看 manifest 的整体 sha。

### 历史指纹（**S3 之后 / S4 之前** · 仅供定位，已被上表取代）

| 产物 | sha256 |
|---|---|
| P1 `flat/top.v` | `26ad20fa14766691bfa96759db0cbeb0ec9924a7a75c1cd34a77717489f7da19` |
| P1 `flat/manifest.json` | `e4938b5ebda3b1a1654a6de58dcc0cc6a9b3bbcf53806f8de91fbc81ef9f578e` |
| P2 `hier/top.v` | `c7a6066220b581a106748e55670cc3cb2537d6bcd0ff11d7e2d731a91037984c` |
| P2 `hier/manifest.json` | `e4938b5ebda3b1a1654a6de58dcc0cc6a9b3bbcf53806f8de91fbc81ef9f578e` |
| P3 `soc-hier/top.v` | `b3e939fc253736108bc08f577c2036a8e5956bdf4cbf44fdfc612e19c3e7f56d` |
| P3 `soc-hier/manifest.json` | `f69a4882922d80349ac01d653f1e6488a148449ba60de1eea869064a8f27d6a3` |

## 3. 它**不是**判据（判据在别处）

字节指纹只能回答「变了没有」，回答不了「变得对不对」⇒ ⇒ **行为正确性由结构化判据负责**：

| 判据 | 管的什么 |
|---|---|
| `scripts/verify_hierarchical_rtl.sh` **R8′** | 核的**顶层端口表 == 手推期望集**（34 根）⇒ ⇒ 端口面不许漂 |
| `scripts/verify_flat_rtl_emit.sh` | 扁平产物的发射口径（含 `--emit-hygiene`） |
| `scripts/rv32i_rtl_sim_env.sh` | 产物 + lint + **独立 Verilog TB** ⇒ `RESULT: checks=24 fails=0` |
| `scripts/verify_examples_rtl_sim.sh` | SoC 级产物 + 登记守恒（⑧/⑧′） |

⇒ ⇒ **本文件的价值 = 「改了没有」的第一手定位**（比翻 git diff 快），
**不是**「改得对不对」的证据（那是上表的事）。

## 4. 复算命令（S4 结束时用）

```bash
rm -rf /tmp/opencode/base && mkdir -p /tmp/opencode/base/{flat,hier,soc-hier}
./build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --out /tmp/opencode/base/flat
./build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --out /tmp/opencode/base/hier
./build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out /tmp/opencode/base/soc-hier
cd /tmp/opencode/base && sha256sum flat/top.v flat/manifest.json \
                              hier/top.v hier/manifest.json \
                              soc-hier/top.v soc-hier/manifest.json
```

## 5. 变更规则（S4 用）

| S4 的改动类型 | `top.v` 指纹 | `manifest.json` 指纹 | 谁来判对错 |
|---|---|---|---|
| 数据通路改（外设 / DMA / 取指侧总线化 / I-L1 / **观测面逻辑**） | **变**（预期） | **整体 sha 变（预期）** ⇒ ⇒ `ports[]` 节点清单动了 ⇒ ⇒ ⚠️ **不是**端口面判据 ⇒ ⇒ 判端口面 MUST 看 `port_table[]` 条数 + C6 / R8′ / `verify_riscv_soc_core_ports` | R8′ + `rv32i_rtl_sim_env` |
| 顶层端口面改（加口 / 删口） | **变**（预期） | **变**（预期） | ⚠️ 端口面变更 = **产品契约变更** ⇒ MUST 走 **G-5 / ADR-077** 的裁决流程，**不是**本文件能判的 |
| 发射器/命名改动（`codegen_verilog` 等） | **变**（预期） | 不变 | 既有 golden + lint |
| **意外改动**（谁也不知道为什么变了） | **变** | 视情况（`ports[]` 动 = 有节点增删；`port_table[]` 动 = **端口面变了**） | ⚠️ **本文件的用途就在这里** ⇒ 定位到「哪个产物的哪一段变了」⇒ 再看 diff |

⇒ ⇒ **本文件不设 CI 门**（指纹变化本身**不是**违规 —— 批次本来就会改产物）。
⇒ ⇒ 若将来要把它变成**护栏**（例如「端口面 manifest 的指纹 MUST 不变」），
那是一次**判据新增**（需要锚点 + 投毒自证 + CI 挂载）⇒ ⇒ 属独立立项，不在本文件内。

## 6. 历史

| 时间 | commit | 事件 |
|---|---|---|
| 2026-11-23 | `7d2a453` | **本基线建立**（S3 归档后 · 作为 S4 的对比基线） |
| 2026-11-24 | （S4a-0 同批提交） | **S4a-0 更新基线**：三份 `top.v` 全变（**观测面逻辑**：`dbg_wb_write_en & !mem_stall` · 核顶层 `obs_data_write_en & data_ready_calc`）· `manifest.json` 整体 sha 也变（`ports[]` 节点清单动了）⇒ ⇒ **但 `port_table[]` 仍 34 / 34 / 8** ⇒ ⇒ 端口面**未变**（C6 / R8′ / `verify_riscv_soc_core_ports` 三处全绿佐证） |

> ⚠️ **S3 的 `T0.1`（「记录**改动前**的产物 sha256」）已按 2026-11-23 的用户裁决**不做**：
> 取历史值需要 `checkout 46810a5` + **两次全量重建**（「还原源码 ≠ 复原 · 二进制也是状态」），
> 而它想守的东西（产物面）**已由 §3 的结构化判据覆盖** ⇒ ⇒ 且它对 S3 **不是**判据
> （S3 是**有意**改产物的）⇒ ⇒ 改为「记**当前**值作 S4 的基线」（本文件）。
