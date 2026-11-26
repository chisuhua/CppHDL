# 验收报告 · `rv32i-obs-vector-axil-2026-11`（步 2 · AXI-Lite 进跨后端向量）

> **角色**：builder 自验（本 change 的审查者 = `verifier` 角色内建）
> **判据来源**：① 用户 2026-11-26「按你建议的顺序执行」② `proposal.md` 的 `## 验收标准`（AC1–AC7）
> ③ change 自身声明 ④ 步 1 的 spec 契约（`verilator-rv32i-e2e`）
> **已用轮次**：**1**
> **结论**：**PASS（7/7 AC）** · 遗留 = T4.3 归档

---

## 1. 逐条判定

### AC1 向量 == 端口面（强形式）—— ✅

```
$ bash scripts/verify_dual_backend_parity.sh
=== 第二判据：跨后端观测向量 vs 核顶层 ch_out 端口面（rv32i-obs-vector）===
  ✅ C1 守恒：向量 39 + 登记 0 == 核顶层 ch_out 39
  ✅ C2 登记有效：0 条全部存在于核顶层 ch_out
  ✅ C3 登记不 stale：0 条均不在向量里
  ✅ C4 归属：向量 ∪ 登记 == 核顶层 ch_out（39 根）
  ✅ C2b 登记完整性：0 条的「为什么未纳入」/「归位条件」均非空
  ℹ️ 登记表 0 条（缺口已归零 ⇒ 向量覆盖全部端口）
✅ rv32i-obs-vector: PASS
EXIT=0
```

### AC2 全量用例绿 —— ✅

```
$ ./build/examples/riscv-mini/test_rv32i_soc_dual_parity
All tests passed (44494 assertions in 16 test cases)      ← 原 15 / 23213
EXIT=0
```

### AC3 AXI 语义 golden（B–F）—— ✅（**两条变异证明判据不是空转**）

| 变异 | 做法 | 结果 |
|---|---|---|
| **M1** | `kExpectWstrb` 0xF → 0x3 | `EXIT=1` · `CHECK( first_w_strb == kExpectWstrb )` **FAILED** ⇒ wstrb 车道判据**有判别力** |
| **M2** | `AxilSlave.awready` 1 → 0（从设备永不应答） | 5 条断言 **FAILED**（`aw_hi == 1` · `ar_hi == 1` · `stores == 1` · `wbs.size() == 3` …）⇒ 「VALID 恰 1 拍」与事件判据**有判别力** |

两条变异均**编译通过**（`[100%] Built target …`）⇒ 跑的是**新**二进制（非假绿）。
变异后已还原源码并**重建**，再跑全量 ⇒ `44494 / 16` 全绿（见 AC2 末次运行）。

### AC4 非 MMIO 程序「请求线全程 0」—— ✅

新用例 ② 段（`plain_fw` 7 字 = ALU + 一条**非 MMIO** store + 自环）逐拍断言
`axil_awvalid == 0 && axil_wvalid == 0 && axil_arvalid == 0` ⇒ 全过。
（= 把 `test_rv32i_core_mmio` 的 AC5「pre-cache 分流方向」搬进跨后端证据。）

### AC5 护栏自检 —— ✅

```
$ bash scripts/verify_dual_backend_parity.sh --self-test
self-test: 12/12 fixture 全绿
  ✅ 基线 fixture 全绿（P=4 · V=2 · R=2）
  ✅ P0 空登记表 + 向量全覆盖 ⇒ 绿（缺口归零是合法状态）     ← 本 change 新增
  ✅ P1 向量删一项 ⇒ 红（命中 C1、C4）
  ✅ P2 登记编造端口 ⇒ 红（命中 C2）
  ✅ P3 登记删一行 ⇒ 红（命中 C1、C4）
  ✅ P4 新增端口未登记 ⇒ 红（命中 C1、C4）
  ✅ P5 补齐未清账 ⇒ 红（命中 C3）
✅ rv32i-obs-vector --self-test: PASS（基线 + P0 + 5/5 投毒各自必红）
```

### AC6 锚点零变化 —— ✅

`bash scripts/verify_test_counts.sh` ⇒ exit 0（`CTEST_TOTAL` 284 · `VERILATOR_E2E_TOTAL` 81 ·
`DUAL_BACKEND_PARITY_TOTAL` 67 · `DUAL_BACKEND_PARITY_PAIRS` 297 · `EXAMPLES_TEST_SOURCES` 26）·
护栏主路径仍打印 `PARITY_COUNT=67 / PARITY_PAIRS=297`。

### AC7 其它护栏 —— ✅

```
✅ docs-links: PASS（L1 48 篇 · 链接 199 条 · 引用 178 处）
✅ verify_guard_reachability: PASS（58 个被引用脚本 · 登记表双向 1:1）
✅ 索引状态列与 frontmatter `状态:` 逐行一致（20 行）
✅ verify_openspec_delta: R1–R4 全绿
```

---

## 2. 边界与错误路径

| 路径 | 处置 | 探测 |
|---|---|---|
| verilator 缺席 | `WARN` + return（不用 SKIP） | 代码路径（`tool_available`） |
| `initialize()` false / `port_access_` 空 | 同上 | 代码路径 |
| **空登记表** | 原 analyzer 会 **fail-closed** ⇒ 本 change 改为**合法**（缺口归零） | 自检 **P0** 钉住 |
| 未来用例意外碰 MMIO | 默认从设备**立即应答** ⇒ 不挂死（原默认「全 0」会永久冻结） | 设计选择（design §1） |

---

## 3. 遗留清扫

| 项 | 状态 |
|---|---|
| TODO / 半截实现 | 无（新代码 grep `TODO/FIXME` = 0） |
| 注释掉的代码 | 无 |
| 临时调试输出 | 无（`INFO(...)` 是**失败诊断**，与文件既有风格一致；`RV32I_SOC_DEBUG` dump 是既有工具） |
| 死代码 | 无新增；`AxilSlave` 的 8 个字段全部被 `drive_axil` 消费 |
| 被 skip 的测试 | 无 |
| 范围外遗留 | P0-2 / P1（步 3）已在 improvement `进度:` 与盘点 §0/§9 登记 |

---

## 4. 失败项与路由

**无失败项。**
