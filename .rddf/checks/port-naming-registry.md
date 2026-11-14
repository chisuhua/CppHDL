# 端口命名棘轮登记表（port naming ratchet registry）

> **机器面**（逐端口条目）= `.rddf/checks/port-naming-exempt.tsv` ← **护栏读它**
> ⚠️ **本文件是人读面**（口径 / 统计 / 维护规则）⇒ ⇒ **不要手改 tsv**（护栏会红）。

---

## 1. 为什么要有这张表（`port-naming-ratchet-2026-11` **N2**）

现状产物里的端口名是 `top_unnamed_input_3` 这类**不可自解释**的名字
⇒ ⇒ 用户写 TB **必须查 port map**（planner 本轮就是靠 node-id 对照才查出来的）
⇒ ⇒ 而「**新增**端口 MUST 具名」这条义务此前**没有任何自动化护栏**。

⚠️ **为什么不能一次全改**：存量 **1452** 个端口声明改成具名 ⇒ ⇒ 会动**全部**产物
⇒ ⇒ 所有 golden / 快照 / 上下游 TB 全断 ⇒ ⇒ **不可接受**。

⇒ ⇒ 故用**棘轮**：**存量登记为豁免** ⇒ ⇒ **债只许减不许增** ⇒ ⇒
新增端口一旦具名 ⇒ ⇒ 登记表逐条缩小 ⇒ ⇒ 债自然归零。

---

## 2. 口径

| 项 | 值 | 理由 |
|---|---|---|
| **扫描面** | `include/**` + `examples/**` | **product 侧**（N2 ①）⇒ ⇒ `tests/**` **排除**（沿用既有裁决 `scripts/verify_input_port_additions.sh`：「测试 DUT 端口自包含 ⇒ 不属风险面」） |
| **扫描对象** | `__io(...)` 块内的端口声明 | 组件端口的**唯一**声明处 |
| **合规判据** | 具名 **且** `标识符 == 显式名字串` | ⚠️ **名字串是唯一真源**（框架**无反射** ⇒ 运行期只存活显式命名串 ⇒ ⇒ manifest 的 `cpp_member` 只能是它 ⇒ ⇒ 「端口名可解析到 C++ 成员名」**必须**由静态护栏兜住 —— oracle 根因 **F-C**） |
| **条目身份** | `<文件>\t<__io 块序号>\t<声明文本>` | ⚠️ **不是文件哈希** —— 哈希对**任何注释改动**都失效 ⇒ ⇒ 一次无关的注释编辑就会让整张表失效 ⇒ ⇒ 必须**逐端口**（N2 ③⑤） |
| **判定粒度** | 端口级（逐条集合比对） | N2 ② |

---

## 3. 判定（双向）

| 方向 | 含义 | 红？ |
|---|---|---|
| **正向** | 树里有、登记表里**没有** ⇒ ⇒ **债增长**（新增端口未具名 / 名字串与标识符不一致） | 🔴 **红** |
| **反向** | 登记表里有、树里**没有** ⇒ ⇒ **登记表烂掉**（债已修但表没同步 / 声明文本变了 ⇒ 身份变了） | 🔴 **红** |

⇒ ⇒ **棘轮的方向性**：新增端口 ⇒ **必须**具名且 `标识符 == 名字串`；
既有债修掉一条 ⇒ **必须**同步删表里那一行（否则反向红）⇒ ⇒ 表**只减不增**。

---

## 4. 当前状态（**2026-11-13 更新** · 基线 = 2026-11-11）

| 指标 | 值 | 变化 |
|---|---:|---|
| product 侧端口声明总数 | **1484** | — |
| 其中**未具名**（既有债） | **1452** | **−32** |
| 其中**具名且合规** | **32** | **+32** ✅ |
| 其中**具名但标识符 ≠ 名字串**（P1 违规） | **0** | — |
| 登记表条目数（= `EXPECTED_EXEMPT_PORTS`） | **1452** | **−32**（锚点 `scripts/verify_port_naming.sh:50`）|

✅ **2026-11-13 · R17.2 首批消化落地**（architect 裁量闭 R17.1 =「现在做」）：
对象 = `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 **32** 个端口**全部**带**显式名**
⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`（实测 `top_ext_rst` / `top_prog_we` / `top_obs_data_addr` / …）
⇒ ⇒ ⚠️ **2026-11-14**：`top_ext_clk` **已不存在**（R8 改判删掉 `ext_clk` ⇒ ⇒ 顶层端口 **32 → 31**）
⇒ ⇒ **棘轮首次真咬**：豁免 **1484 → 1452** ⇒ ⇒ 「递减」这一半从停滞变为**有推进**。

⚠️ **「具名且合规 = 0」的历史基线事实已改变**：具名构造的**调用点此前实测 = 0**
⇒ ⇒ 那条路径此前**从未被端到端跑过** ⇒ ⇒ **N1** 才第一次把它跑通
（`tests/test_port_name_named_ctor.cpp`）⇒ ⇒ **2026-11-13 是第一条「产品侧」具名端口**。

⚠️ **仍未具名的 80 个顶层端口 = 后代冒泡端口**（`top_pipeline_*` / `top_itcm_*` / `top_dtcm_*`）
⇒ ⇒ 它们**不是**根组件自有 IO ⇒ ⇒ 不在本批范围 ⇒ ⇒ 归属**后续批次**。

⚠️ **分布**（Top 文件）：`include/axi4/axi_interconnect_4x4.h` 241 ·
`include/axi4/axi_to_axilite.h` 95 · `include/axi4/axi4_lite_matrix.h` 77 ·
`include/cpu/pipeline/rv32i_pipeline_regs.h` 76 · … ⇒ ⇒ **AXI 侧是债的主体**
⇒ ⇒ ⇒ **首个消化对象** = `port-naming-ratchet` 的 N3 首批靶
（`examples/riscv-mini/src/rv32i_pipeline_top.h` · 32 端口 · 用户 2026-11-11 拍板）。

---

## 4.1 消化留痕（**每次消化 MUST 独立 commit + 独立回归 + 在此留痕**：文件 + 端口数 + 日期）

| 日期 | 文件 | 端口数 | 豁免数变化 | 独立回归 | 依据 |
|---|---|---:|---|---|---|
| **2026-11-13** | `examples/riscv-mini/src/rv32i_pipeline_top.h` | **32** | **1484 → 1452**（−32） | ✅ `verify_port_naming.sh` PASS（R2 = 1452）· ✅ `verify_examples_rtl_sim.sh` **10/10**（`riscv-mini-rv32i-soc` = `checks=24 fails=0`，与改名前**逐字相同**）· ✅ `ctest` **263/263** · ✅ 全量重建 RC=0 | `port-naming-ratchet` **N2 补遗「首批消化 32 端口」** · R17.2（architect 裁量闭 R17.1 =「现在做」）· 验收报告 `.rddf/reviews/r17-2-port-naming-first-batch-verify-2026-11.md` |

⇒ ⇒ ⚠️ **本表是「消化次数」的活账** ⇒ ⇒ 每次消化 MUST 追加一行
⇒ ⇒ **禁**改历史行（改了就篡改历史）⇒ ⇒ **禁**删行（删了就丢证据）。


---

## 5. 维护规则

| # | 规则 |
|---|---|
| 1 | **新增端口 MUST 具名**（`ch_in<ch_bool> prog_we{"prog_we"};`）⇒ ⇒ 且**标识符 == 名字串** ⇒ ⇒ 否则护栏红 |
| 2 | **既有债被修掉时 MUST 同步删表**（否则反向红）⇒ ⇒ 删表行 = 债减少 |
| 3 | **计数锚点 `EXPECTED_EXEMPT_PORTS` 钉在 `scripts/verify_port_naming.sh`**（不在本表、不在 tsv）⇒ ⇒ 增删 MUST 同步锚点 |
| 4 | tsv **不要手改**（用 `python3 scripts/lib/port_naming.py --root . --write-registry .rddf/checks/port-naming-exempt.tsv` 重生成） |
| 5 | 改了某个文件的 `__io` 块 ⇒ ⇒ **块序号会移位** ⇒ ⇒ 表里该文件的条目身份全变 ⇒ ⇒ 反向红 ⇒ ⇒ **必须**重生成 + 同步锚点（这是棘轮的**代价**，如实登记） |

---

## 6. 用法约定（在别处）

- **写法 + 已知限制 + 验证命令** = `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md`
- **端到端探针** = `tests/test_port_name_named_ctor.cpp`
- **契约**（命名规则的规范所有者）= capability `rtl-hierarchical-output`
- **manifest schema**（`cpp_member` 字段）= capability `rtl-emit-portability`

---

## 7. 护栏

```bash
bash scripts/verify_port_naming.sh            # 判定
bash scripts/verify_port_naming.sh --self-test # 4 类投毒（每条规则 MUST 有判别力）
```

⚠️ **已进 CI**（`build-and-test` 的 guard 段）⇒ ⇒ push / PR 都跑。
