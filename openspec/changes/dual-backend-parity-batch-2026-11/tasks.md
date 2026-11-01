# Tasks: dual-backend-parity-batch-2026-11

> 勾选规则：**commit 之后**按 `git show --name-only <sha>` 核对实际改动再勾
> （MUST NOT 先勾后做）。

## T1 量化盘点（T7 步骤 1 · **planner 决策门**）

- [x] **T1.1** 用 capability R6 的 **grep 模式**筛候选 ⇒ ⇒ **37**
- [x] **T1.2** 减契约锁定（7）+ 已是 dual（1）⇒ ⇒ **可转 dual = 29**
- [x] **T1.3** 逐个核对「无 runtime 语义」⇒ ⇒ **4 个**（旧盘点说 3 ⇒ **差 4**）
- [x] **T1.4** 核对 >64-bit 端口 ⇒ ⇒ **0 个**（grep 模式已排除）
- [x] **T1.5** ⚠️ 核对「Catch2 SECTION 重入」排除项 ⇒ ⇒ **措辞可能已陈旧**
      （宏是**两个独立 TEST_CASE**）⇒ ⇒ **登记待 planner 复核**
- [x] **T1.6** ⇒ ⇒ **可达上限（路线 A）= 58 < 60** ⇒ ⇒ **决策门触发**
      ⇒ ⇒ **交数字，不改判据**（判据属 planner）

## T2 成本模型（**实测**，不拍脑袋）

- [x] **T2.1** 冷缓存 · 小 DUT ⇒ ⇒ **13.5 s**（试点实测）
- [x] **T2.2** 冷缓存 · mem 类 DUT ⇒ ⇒ **72.0 s**（假 HOME 实测）
- [x] **T2.3** 热缓存档位 ⇒ ⇒ **0.6 – 7.7 s**
- [x] **T2.4** ⇒ ⇒ 结论：**冷编译是主成本** ⇒ ⇒ **按 DUT 聚合组织批次**

## T3 批次清单

- [x] **T3.1** 写 `.rddf/plans/dual-backend-full-coverage-2026-11.md`
      （盘点 + 成本模型 + **8 批** + 墙钟账本 + 风险对策）
- [x] **T3.2** ⚠️ 登记 **CI 45 min 上限** + `RESOURCE_LOCK` 串行 ⇒ ⇒
      累计预估 **≈ 56 min** ⇒ ⇒ **超标即停 + 报告**（不硬凑）

## T4 批次 A 实施（**试点**）

- [x] **T4.1** 新增 `tests/test_blt_debug_behavior.cpp`
      （**1 个 dual pair** · DUT = `riscv::BranchConditionUnit`）
- [x] **T4.2** **原 `tests/test_blt_debug.cpp` MUST NOT 修改** ⇒ ⇒ 零回归
- [x] **T4.3** 期望值**手推**（`10 < 20` ⇒ ⇒ `branch_condition == 1`）
- [x] **T4.4** `tests/CMakeLists.txt` 注册 `add_dual_backend_test`
- [x] **T4.5** ⚠️ **判别力**：断言数 = **2 × pair 数** ⇒ ⇒ **verilator 半边
      真跑**（实测 2/2）⇒ ⇒ ⇒ **该核对 MUST 进每批验收模板**

## T5 计数锚点同步（**每批必做**）

- [x] **T5.1** `CTEST_TOTAL` 205 ⇒ ⇒ **206**
- [x] **T5.2** `VERILATOR_E2E_TOTAL` 38 ⇒ ⇒ **39**
- [x] **T5.3** `DUAL_BACKEND_PARITY_TOTAL` 29 ⇒ ⇒ **30**
- [x] **T5.4** `DUAL_BACKEND_PARITY_PAIRS` 184 ⇒ ⇒ **185**
- [x] **T5.5** `tests/test_*.cpp` 文件数 118 ⇒ ⇒ **119**
- [x] **T5.6** ⚠️ 根 `AGENTS.md` 的**叙述值**同步（「206 CTest tests
      registered」·「195 non-perf」·「现值 30」）⇒ ⇒ 否则锚点与叙述漂移

## T6 验收

- [x] **T6.1** `ctest -LE perf` 全绿（**195/195**）
- [x] **T6.2** `scripts/verify_test_counts.sh` ⇒ ⇒ exit **0**
- [x] **T6.3** `scripts/verify_dual_backend_parity.sh` ⇒ ⇒ 双锚点匹配
- [x] **T6.4** 全部护栏 exit 0
- [x] **T6.5** `openspec validate --type change --strict` ⇒ ⇒ **valid**
- [x] **T6.6** 冷缓存墙钟实测并回填墙钟账本（**13.5 s**）

## T7 归档

- [x] **T7.1** ⚠️ **零 spec 变更声明**（proposal 已明文）⇒ ⇒ 归档**允许**
      `--skip-specs`
- [ ] **T7.2** critic 评审（本 change 独立预算）

## 验证命令

```bash
cmake --build build --target test_blt_debug_behavior -j"$(nproc)"
./build/tests/test_blt_debug_behavior          # ⇒ 断言数 MUST = 2 × pair 数
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_test_counts.sh
bash scripts/verify_dual_backend_parity.sh
bash scripts/verify_stream_ready_ownership.sh
openspec validate dual-backend-parity-batch-2026-11 --type change --strict
```
