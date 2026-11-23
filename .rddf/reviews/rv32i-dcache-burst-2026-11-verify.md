# 验收报告：`rv32i-dcache-burst-2026-11`（S3 · D-L1 的 AXI4 full 主口 + 真 line + 接入核）

- **日期**：2026-11-23
- **角色**：builder 自查（verifier 契约 1–5 步）· 本 change **未走** `rdd-verifier` skill
  （分流三条件第 2 项不成立：`.rddf/state/iteration.json` 里无该 change 的 P3 gate 记录）
- **判据来源（优先级序）**：
  1. `openspec/changes/rv32i-dcache-burst-2026-11/proposal.md` 的 `## 验收标准`（AC1–AC11）
  2. 同 change 的 `specs/rv32i-dcache-timing/spec.md`（4 个 Requirement / 12 个 Scenario）
  3. 母 improvement `.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 ACs3-1..4
- **已用轮次**：本轮第 **1** 轮（上限 3）

## 1. 编译 / 静态检查

| 项 | 命令 | 结果 |
|---|---|---|
| 全量构建 | `cmake --build build -j$(nproc)` | **0 error / 0 warning（新增文件）** |
| 头自包含 | `scripts/verify_header_self_contained.sh` | PASS |
| ODR | `scripts/verify_header_odr.sh` | PASS |
| 端口命名棘轮 | `scripts/verify_port_naming.sh`（+ `--self-test`） | PASS（`EXPECTED_EXEMPT_PORTS` 1311 → **1367**） |
| 必需端口登记表 | `scripts/verify_input_port_required_registry.sh` | PASS（新登记 `DCacheSync.strbe`） |
| 公开面登记 | `scripts/verify_public_api_visibility.sh` | PASS |

## 2. 单元 / 组件级

| 测试 | 结果 | 覆盖 |
|---|---|---|
| `test_cache_sync` | **24 用例 / 80 断言**（双后端） | AC1 · AC2 · AC3 · AC4 · AC5 · AC8 |
| `test_tcm_axi_slave`（新） | **2 用例 / 18 断言**（interpreter + verilator） | **AC6** + AC8 |
| `test_cache_complete` | 13 / 54 | 既有兼容（4-beat 激励已改） |
| `test_cache_pipeline_integration` | 3 用例（1 passed / 2 SKIP · 断言 22） | 既有兼容（DCache 用例已改新端口） |
| `test_cache_simple` / `test_cache_async_dual` | 2/4 · 6/146 | 未受影响 |
| `test_phase4_chlib` | 3 passed / 5 skipped | `strbe` 已显式驱动（护栏 R4） |

## 3. 集成 / 端到端

| 判据 | 结果 |
|---|---|
| `tb_rv32i_soc`（AC7） | **`RESULT: checks=24 fails=0`** |
| `run_all_ported_tests.sh` | **PASS 29 / FAIL 0 / MISSING 0** |
| **AC7 逐拍等价**（`test_rv32i_soc_dual_parity`） | **13 用例 / 23140 断言全过**（含 260 拍 × 27 观测的逐拍全等 · 24 条写回序列 · 字节/半字道 · 跳转冲刷） |
| **AC4 反压**（核级） | 由上项的 `StoreToLoadZeroIntervalReadNew` / `LoadByteLaneDiscriminating` / `StorePhaseCoherence` 覆盖（load miss ⇒ fill 期间整条流水线冻结 · 期望值手推） |
| `rv32i_rtl_sim_env.sh`（产物 + lint + 独立 TB） | **PASS** · `RESULT: checks=24 fails=0` |
| `verify_hierarchical_rtl.sh`（R1–R8 + R8′） | PASS（34 端口表 == 手推期望集） |
| `verify_flat_rtl_emit.sh` | PASS |
| `verify_examples_rtl_sim.sh`（⑧/⑧′） | PASS |
| `ctest --output-on-failure` | **275 / 275 passed** |

## 4. 边界与错误路径

| 边界 | 处置 / 证据 |
|---|---|
| **MRU/LRU way 选择**（两路都 valid） | `choose_way1` 用 `lru_mem.aread`；`test_cache_complete` 的 fill→hit 用例覆盖单 fill 路径；两路替换路径由既有 LRU 逻辑保持（本 change 未改判定） |
| **BEAT 4（`r_last`）的数据完整性** | 修掉「`beat3` 寄存器与 `fill_complete` 同沿」的 off-by-one（否则 **lane 3 恒 0**）⇒ AC1 用例覆盖 4 个 lane |
| **写命中 / 写 miss** | 写 miss 不 fill（`miss_read` 门 `!we`）⇒ 仍 write-through 直通 AXI；`test_cache_sync` 用例 9 覆盖 |
| **`strbe` 部分写** | `byte_lane_apply` 在目标 lane 内合并；`test_rv32i_soc_dual_parity` 的 `SB`/`SH` golden（`0x30A` / `0xD0000`）覆盖 |
| **空闲拍（无访存）** | `data_ready ← !data_req | cache.ready` ⇒ 否则**永久冻结**（实测踩中：23/24 红）⇒ 24 判据全绿即证据 |
| **连续 store 的 AW/W 分拍** | 已知代价（`w_hold` 锁存）⇒ 计入「观测面变化」，非缺陷 |
| **fill 期间 addr 变化（违反契约）** | AR 握手那拍锁存 index/tag/way ⇒ 已发事务不跟随新 `addr` |

## 5. 遗留清扫（本次范围内）

| 项 | 状态 |
|---|---|
| TODO / 半截实现 | 无（`design.md` 的 5 条实施期发现均已落地，无「待做」标记） |
| 注释掉的代码 | 无 |
| 临时调试日志 | 无（`printf` 类调试未进入库/测试） |
| 被 skip 的测试 | 2 处：`test_cache_pipeline_integration` 的 `SKIP("Rv32iPipelineWithCache not implemented")` 与 `test_phase4_integration` 的同款 —— **均为既有 SKIP**（非本次引入），其**组件级断言**已实跑 |
| 未替换的 mock | 无（`TcmAxiSlave` 是真实实现，不是 mock） |
| 临时 fallback | 无 |
| 本次范围外但发现的问题 | ① `verify_verif_migration_state.sh` **红**（`kind=soc` 有 2 行 ⇒ fail-closed）—— **pre-existing**（`2cab7fb` 引入 `tb_riscv_soc` 行时未同步该护栏）⇒ 记 backlog，**本 change 不动**；② `T0.1`（产物 sha256 基线）**显式不做**（见 `tasks.md`）⇒ 交用户裁决 |

## 6. 架构面评审（critic · `review_profile: build, plan`）

| 项 | 结论 |
|---|---|
| 总判定 | **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **BLOCKER 0** |
| focus 1（`ready`/`miss` 组合 + `rdata` 保持寄存器的**充分性**） | **无失效路径**（逐拍覆盖 hit→hit / hit→miss / miss→hit / fill 完成拍与下一请求同拍 / store 穿插 / 冻结期多次 fill） |
| focus 2（`data_ready ← !data_req \| cache.ready` 是否削弱已签批条款） | **不构成削弱**（条款约束 MEM 级响应，`mem_stall = !data_ready` 仍无条件；且新决策已落 spec delta 第 5 条 Requirement） |
| focus 3（判据放宽的判别力） | **可接受**；两处（wb 折一 / `perf_load_use_stalls`）已把残余盲区写进注释与退休总数判据 |
| SHOULD_FIX #1（固件缺 **hit 形态** load→use ⇒ 已签批「4 拍」形态的固件级载体消失） | ✅ **已修**：固件末尾补 `LW x29,[0x100]`（hit）+ `ADD x30,x29,x29` ⇒ `perf_load_use_stalls` **2 → 6** · `non_retired` **41 → 45** · 退休数 **`kCycles-46`** · 写回序列 **24 → 26** 项 · `hazards` **2 → 3** |
| SHOULD_FIX #2（`proposal.md` 的 `data_ready` 措辞与实现/AC11 矛盾） | ✅ **已修**（改为 `!data_req \| cache.ready`） |
| NIT（`d_cache_async.h` 文件头未标「同型缺陷」） | 📋 **记为 backlog**（不在本 change 范围；`d_cache_async` 未接入生产路径） |

## 结论

- **AC1–AC11**：全部满足（每条均有上表的工具证据）
- **ACs3-1..4**（母 improvement）：全部满足
- **判据来源冲突**：无（proposal AC 与 spec delta 口径一致；AC3 的订正已回写 proposal + spec）
- **失败项**：无
- **route**：本报告交 builder → **critic（架构面）** → 归档
