# 治理清账 + R8 改判落地（`ext_clk` 删除）— 验收报告

- 日期：2026-11-14
- 角色：builder（自验；R8 属「用户裁决 + builder 机械落地」，architect 追认可另行安排）
- 范围：
  1. **D-1** = 删 RV32I SoC 的 `ext_clk` 死端口（用户裁决 **A2**）
  2. **D-2** = 扁平顶层 80 个内部 `ch_out` 登记为已知残留（用户裁决 **A；复核发现 2026-11-13 已落地**）
  3. **治理清账**：9 张 `decision-request-*` 的状态字段据实回填 + 两个 change 的 `tasks.md` 回填

## 1. D-1 判据与证据

| # | 判据（来源） | 证据 | 结果 |
|---|---|---|---|
| AC-1 | **零行为影响**（删前，R8.2 要求的针对性实测） | `ext_clk` **0/1 交替翻转** ⇒ ⇒ **60 拍 × 27 观测逐拍全等**（**1351 断言**）⇒ ⇒ 临时探针，跑完即删 | ✅ |
| AC-2 | **删后等价** | `ctest -LE perf` **253/253**（含 SoC 双后端 golden · 手推期望值零漂移） | ✅ |
| AC-3 | **产物面** | 顶层端口 **114 → 113**（6 输入 + 107 输出）· `ext_clk` 在产物里**零命中** | ✅ |
| AC-4 | **端口快照/棘轮** | C6 PASS（31 个与快照一致）· 命名棘轮 PASS（豁免表 1450 → **1449** 双向一致） | ✅ |
| AC-5 | **独立 Verilog TB 仍严格通过** | `verify_examples_rtl_sim.sh` **10/10**（严格 flags ⇒ **无 UNDRIVEN / PINMISSING**） | ✅ |
| AC-6 | **公开面登记** | `public-api-surface.md` +2 行 · `public-api-removals.tsv` +2 探针（锚点 34 → **36** · 表行 20 → **22**）· `CHANGELOG.md` 1 条 | ✅ |

### 为什么连带删 `riscv::Rv32iPipeline::clk`

它**零消费者**（`include/cpu/pipeline/**` 内无任何 `io().clk` 读取，剥注释后 `clk` 在该文件 **0 命中**）。
只删 `ext_clk` 会让它变成**悬空输入** ⇒ ⇒ 产物里出现 undriven wire ⇒ ⇒ verilator 严格模式 **UNDRIVEN 致命**
⇒ ⇒ 且 ZERO-DEBT 明确要求「零消费者端口不得留存」。

## 2. D-2 复核结论

**2026-11-13 已落地**，无需新动作：

| 载体 | 位置 |
|---|---|
| 残差登记表 | `.rddf/checks/arch-residual-registry.md` **id = 23**（状态 = 已知 · 日期 2026-11-13） |
| 架构台账 | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 **行 23**（状态格 = 登记表生成的规范句） |

⇒ ⇒ 本次只做了两处**订正**（不改状态格、不改裁决内容）：
- 台账行 23 的**描述列**：`.v` 114 → **113** · 根组件自有 32 → **31**（**80 个内部 `ch_out` 不变**）
- 请求单 §1.1 加**时点标注**（2026-11-13 快照 vs 2026-11-14 数字）

## 3. 治理清账

### 3.1 9 张 `decision-request-*` 的状态回填

| 单 | 回填状态 | 依据 |
|---|---|---|
| `B1-builder-backpressure` | ✅ 已裁决并落地（2026-11-01） | 文内裁决段 + ADR-055 + capability |
| `fifo-full-threshold` | （原已正确） | — |
| `hierarchical-ledger-residuals` | ✅ 已闭（建议 **B** 已落地） | 登记表 + 护栏 + CI 均已存在；3 行陈旧已变「已收口/已裁决」 |
| `hierarchical-sibling-name-collision` | （原已正确） | — |
| `input-port-guards` | ✅ 已裁决并落地（2026-11-03 · D1-A + D2-A） | 两个护栏 + 登记表均在 |
| `port-face` | ✅ 已闭（R1 已登记 / R2 已闭 / R3 已闭 / R4 = 指针） | 登记表 id=23 + 台账行 23 |
| `port-naming-parity` | ✅ 已闭（R1-a/R1-b spec 追加已落地 + 残差两侧已落地） | `rtl-hierarchical-output/spec.md` · `component-ownership-and-naming/spec.md` |
| `rv32i-ext-clock-dead-port` | ✅ **已裁决并落地（2026-11-14 · 改判 = A2 删端口）** | 本报告 §1 |
| `uart-rx-defects` | （原已正确） | — |

### 3.2 `tasks.md` 回填（两 change · 精确到「已做 / 前提未触发 / 真未做」）

**`firmware-load-chain`**

| 项 | 结论 |
|---|---|
| 工作区干净 / 16 KB 对齐 / 编码器交叉校验保留 / 头有消费者 / 全护栏复跑 | ✅ **已做**（逐条附证据） |
| **受影响地址字面量清单** | ⚠️ **前提未触发**（L1 未改绝对地址 ⇒ 零条目）⇒ ⇒ 若将来改绝对地址 ⇒ **重新生效** |
| **越界投毒判据** | 🔴 **真未做** ⇒ ⇒ 现有用例只测 `0x12345678`（旧 64 KB 声明下也红 ⇒ 零判别力）⇒ ⇒ 有判别力形态 = `0x8000_4000` ⇒ `error_cs=1 ∧ i_tcm_cs=0` |
| **非零重定位 ⇒ 报错** | ⚠️ **改判为「已知边界 · 不做」**（`.rela` 在 section 里 ⇒ program header 看不到 ⇒ 静态检查**做不到**，不是漏做） |

**`memory-timing-contract`**

| 项 | 结论 |
|---|---|
| 产物级读口判据 / 判别性用例 / V14 拆分 / **V14c** | ✅ **已做**（V14c = 既有「⑥ 双生产者同 rd ⇒ 取较新」） |
| T6 紧邻同址 + 不冒充 | ✅ **已做** |
| 车道覆盖 | ⚠️ **部分**：`sb` **4/4** ✅ · `sh` **1/2**（只覆盖 `addr[1]=1`，偶半字道未覆盖）⇒ ⇒ 归 **T4** |
| **写使能同拍** | 🔴 **真未做**（接线看起来同拍，但**无产物级判据**）⇒ ⇒ 归 **T4** |
| **相位错位探针** | 🔴 **真未做**（依赖上一条）⇒ ⇒ 归 **T4** |

## 4. 遗留（**本次不做**，交用户决定）

| 项 | 归属 |
|---|---|
| **T4 批次**：写使能同拍判据（新护栏 `verify_dtcm_read_timing.sh`）+ `sh` 偶半字道 + 相位错位探针 + 越界投毒判据 | B3 最后一个技术洞 |
| 「真让 `ext_clk` 生效」（改 codegen 时钟选择 ⇒ 全仓影响） | 独立立项 |
| 非零重定位的**完整**静态检查（解析 section header） | 独立立项 |

## 5. 复跑命令

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -LE perf                  # 253/253
bash scripts/verify_no_duplicate_rv32i_headers.sh   # C6 = 31 端口
bash scripts/verify_port_naming.sh                  # 豁免 1449 双向一致
bash scripts/verify_public_api_visibility.sh        # 探针 36 · 表行 22
bash scripts/verify_examples_rtl_sim.sh             # 10/10（严格 verilator）
for s in scripts/verify_*.sh; do bash "$s" >/dev/null || echo "RED $s"; done
```

实测：`ctest -LE perf` **253/253** · 全部 `scripts/verify_*.sh` **FAIL=0**。
