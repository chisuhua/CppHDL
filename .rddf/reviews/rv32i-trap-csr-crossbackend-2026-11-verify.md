# 验收报告 · `rv32i-trap-csr-crossbackend-2026-11`（步 3 · P0-2）

> **角色**：builder 自验（审查者 = `verifier` 角色内建）
> **判据来源**：① 用户 2026-11-26「按你建议的顺序执行」② `proposal.md` 的 AC1–AC6
> ③ change 自身声明 ④ 步 1 的 spec 契约
> **已用轮次**：**1** · **结论**：**PASS（6/6 AC）** · 遗留 = T4.3 归档 + P1（反压）未纳入

---

## 1. 逐条判定

### AC1 全量用例绿 —— ✅

```
$ ./build/examples/riscv-mini/test_rv32i_soc_dual_parity
All tests passed (60311 assertions in 18 test cases)      ← 原 16 / 44494
EXIT=0
```

### AC2 逐拍 diff 覆盖全端口面 —— ✅

两个新用例都走 `require_per_cycle_equal()`，其内层循环 = `k < kObsCount`（**39**），
且 `static_assert(kObsCount == 39)` 在编译期钉住（步 2 加的）⇒ 端口面缩水即编译失败。

### AC3 trap golden —— ✅

```
$ ./…/test_rv32i_soc_dual_parity "Rv32iSoc - TrapEntryCrossBackend"  ⇒ 全过
（15817 断言 / 2 用例 = 该用例 + CSR 用例一起跑的合并数字；单跑亦全过）
```

手推值命中：`{1, 0x8000_0000}` · `{2, 2}` · 触发拍 `retired == 0` · `trigger+1 / +2` 的 `id_valid == 0`。

### AC4 CSR golden —— ✅

9 条写回 == 手推表（含 `csrrc` 后 `mscratch = 0`）· 合法 CSR 全程 `reason == 0` ·
编码手算常量交叉校验（`0x341020F3` / `0x34202173` / `0x34009173` / `0x3407F3F3` —— 均用
`(imm<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x73` 独立算过）。

### AC5 锚点零变化 —— ✅

`bash scripts/verify_test_counts.sh` ⇒ exit 0（284 / 81 / 67 / 297 / 26 全部未动 ——
新用例在同一 binary 内，不新增 CTest 条目、不新增 `DUAL_BACKEND_TEST_PAIR` 调用）。

### AC6 护栏 —— ✅

```
✅ verify_dual_backend_parity（主路径 + 第二判据 39 + 0 == 39）
✅ verify_docs_links
✅ verify_guard_reachability（58 个被引用脚本）
✅ verify_improvement_index
```

---

## 2. 变异（判别力自证 · 两条均**编译通过**）

| 变异 | 做法 | 结果 |
|---|---|---|
| **M1** | trap 期望 `0x8000_0000` → `0x8000_0004`（模拟「`mepc = pc+4`」缺陷的期望） | `EXIT=1` · `CHECK( wbs[0].second == 0x8000'0004ull )` **FAILED**（实际 0x80000000） |
| **M2** | CSR 期望表 `{4, 0x1234'5678}` → `{4, 0x0}` | `EXIT=1` · `REQUIRE( wbs == expect )` **FAILED** |

⇒ ⇒ 两条 golden **不是恒真断言**。变异后已还原并**重建**，再跑全量 ⇒ 18 / 60311 全绿。

---

## 3. 边界与错误路径

| 路径 | 处置 |
|---|---|
| verilator 缺席 | `run_both` 返回 `ok = false` ⇒ 调用方 `return`（不用 SKIP） |
| `initialize()` false / `port_access_` 空 | 同上 |
| 镜像未覆盖 trap 落点（0x040） | 读到 0 = 又一条未知 opcode ⇒ 写回 > 2 条 ⇒ 红（自解释） |

---

## 4. 遗留清扫

| 项 | 状态 |
|---|---|
| TODO / 注释代码 / 临时调试 | 无 |
| 死代码 | 无（`run_both` / `require_per_cycle_equal` / `collect_writebacks` 均被两个新用例消费） |
| **范围外未做** | **P1（`test_rv32i_stall_integrity` 的跨后端化）** ⇒ 已登记在盘点 §5 / §9 的「3b」行 + improvement `进度:` |
| 已知局限（**如实登记**） | 本项的 `csrrc` 程序里 `old == x` ⇒ `& ~x` 与 `^ x` **都给 0** ⇒ ⇒ 「按位取反后 AND」的判别力由 `test_rv32i_csr_instructions` 的**第二个**用例提供（`old=0x0F` / `x=0xF0`）⇒ 已写进 `design.md` §3，不静默声称覆盖 |

---

## 5. 失败项与路由

**无失败项。**
