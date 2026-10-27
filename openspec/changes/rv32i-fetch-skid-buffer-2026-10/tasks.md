# Tasks: rv32i-fetch-skid-buffer-2026-10

> TDD 5 步（红 → 绿 → 精化 → 变异 → 回归）。**一个 change、两个 commit**（ADR-048 D13.1）：
> **C1** = 缺陷 A（skid）/ **C2** = 缺陷 B（`primed`）⇒ 各自可回退。
> **有 spec 变更** ⇒ 归档 **禁** `--skip-specs`。

## 0. 准备（读码 + 时序确认）

- [x] 0.1 逐拍 trace 确认 **ID = `pc_reg` 晚 2 拍**（design.md §1.1）
- [x] 0.2 确认**停顿首拍**到达的正是被丢弃的那条（`fetch_pc_reg` 与 `fetched_instr` 同拍成对）
- [x] 0.3 实测确认 **ADR 未记载的重复装载**（停顿后同一 word 连装两次，design.md §1.3）
      ⇒ 既有金标的「净退休数」不变（丢一条 + 重一条相抵）
- [x] 0.4 确认 `IfStage` 的 `rst` **高有效**、9 个输入端口清单（design.md §5.3）
- [x] 0.5 确认 `if_stage.h:105-112` 与 `:113-119` 是**逐字重复**注释（ZERO-DEBT 顺带删）
- [x] 0.6 手推 C1 的 6 拍时序表（design.md §2）与 C2 的 `primed` 语义（§4）

## 1. C1（commit 1）：skid buffer —— 修缺陷 A

- [x] 1.1 新增 `skid_firmware()` + 独立 `TEST_CASE`（§5.1 的 4 条断言 + 反向期望）
      ⇒ 「修前失败」的证据由 **M2/M5 变异**给出（M2 ⇒ `x19` 不出现；M5 ⇒ `x16 = 2`）
- [x] 1.2 加 3 个寄存器 `skid_instr` / `skid_pc` / `skid_valid`
- [x] 1.3 填充条件：`stall & instr_ready & !skid_valid & !(flush|flush_d1) & !rst`
      ⇒ `skid_pc ← fetch_pc_reg`（**不是** `pc_reg`）
- [x] 1.4 排空条件 = `if_id_update_en & skid_valid`（**同拍原子**，自动含 `!rst`）
- [x] 1.5 IF/ID 取数源加 mux：`skid_valid ? skid : 流上`
- [x] 1.6 `flush | flush_d1` ⇒ 清 `skid_valid`（防御性契约）
- [x] 1.7 删 `if_stage.h` 里**逐字重复**的第二段「flush 与 stall 不会同拍」注释
- [x] 1.8 **绿**：§5.1 断言 2/3/4 通过（`x19` 出现、`x20` 只写一次）
- [x] 1.9 组件级负向对照（`tests/test_rv32i_pipeline_behavior.cpp`）：
      `stall × flush` 同拍 / skid 已满时 flush ⇒ **3 条**用例（§5.3 + Batch 4-c 对齐）
- [x] 1.10 组件级用例的 **7 个**输入端口逐个显式驱动（`rst = 0` 高有效！）
      ⚠️ 实测 `IfStage` 是 7 个输入（原写「9」是笔误）
- [x] 1.11 实测主固件：`non_retired` **20 不变** / 退休数 **179 不变** / 写回序列 24 项**逐项不变**
      ⇒ 与 §1.3 的预测一致（救回 1 条 + 消掉 1 条重复，计数相抵）

## 2. C2（commit 2）：`primed` 门控 —— 修缺陷 B

- [x] 2.1 §5.1 断言 1（`x16 == 1` 且 `MUST NOT` 为 2）在 C1 之后**确实失败** ⇒ C2 修
- [x] 2.2 加 `primed` 寄存器（`select(rst, 0, 1)`）
- [x] 2.3 `if_id_valid_reg->next` 的装载分支加 `& primed`（**默认只门控 `valid`**）
- [x] 2.4 **绿**：§5.1 断言 1 通过；主固件退休数 **178**（−1）、`non_retired` **21**（+1）⇒ 已回填
- [x] 2.5 处置：删 `stall_drop_slots_are_nop` / `fwd_stall_drop_slots_are_nop` /
      `fwd_first_instruction_not_self_dependent` + **删去重掩码** `:941-945`
- [x] 2.6 主固件备注更新：word 22 / 45 的「停顿会丢指令」注释 ⇒ 改为「不再是约束」

## 3. 精化（判别力自检）

- [x] 3.1 每条断言确认**反向期望**（`X_old != X_new`）并留痕在注释里
- [x] 3.2 「未修」的红证据 = M2（丢指令）/ M5（双装）两条变异 ⇒ 见 §4.1/§4.5
- [x] 3.3 `skid_slot_is_discriminating()` 要求在 word 12 命中（`found`）⇒ 非恒真（word 8 MUST **不是** NOP 且写非 x0）

## 4. 变异验证（每条**先确认编译成功**）

- [x] 4.1 **M1** ⇒ SoC **1 failed** + 组件 **2 failed**
- [x] 4.2 **M2** ⇒ SoC **2 failed**（组件级抓不到，见 design §6.1）
- [x] 4.3 **M3** ⇒ SoC **2 failed**（同上）
- [x] 4.4 **M4**（删掉 flush 清空 skid）⇒ SoC **全绿**（不可达）+ 组件 **2 failed**
      ⇒ 印证 ADR-048 D5/D9.4：**组件级是唯一判别力来源**
- [x] 4.5 **M5** ⇒ SoC **2 failed**（`dbg_id_valid(0)` / `non_retired`；`x16 = 2` 亦被写回序列抓到）
- [x] 4.6 **M6** ⇒ SoC **1 failed**（`ta[0][kIdxIdPc] == 0x80000000`）
- [x] 4.7 每条变异**编译 0 error** 后跑；全部还原后重建 ⇒ SoC 10859 断言 / 组件 74 断言全绿

## 5. 回归与工程

- [x] 5.1 `ctest -LE perf` 零回归（**191/191**，2026-10-27 实测，344 s）
- [x] 5.2 `ctest -L perf`：**10/11 通过**；`perf_regression` 报 2 处回归
      ⇒ **判定为环境漂移，非本次改动**（证据如下，**如实记录**）：
      ① **依赖闭包**：perf 源文件**零** `#include` 任何 `cpu/` 头，编译产物的
         `.d` 也**零**命中 `include/cpu` ⇒ 本次唯一实现面
         `include/cpu/pipeline/stages/if_stage.h` **不在** perf 的编译闭包内；
      ② **分档形态**：回归只出现在 **regs=10（最小档）**（interpreter 1.86× /
         jit 1.89× / verilator 1.31×），而 **regs=100 / regs=1000 的比率是
         1.06–1.15**（逐拍吞吐几乎不变）⇒ 代码级回归会**每档同比劣化**；
      ③ **劣化集中在固定开销**：`build_us` 5198 → **16220 µs**（JIT 编译 / 进程启动）
         ⇒ 是**机器状态**而非代码路径；
      ④ **同二进制二次测量有抖动**：`perf_tests --tc=08 --direct` 重跑 ⇒ verilator
         行 3070 → **2734 µs**（**掉回 1.3× 阈值内**），interpreter 17537 → 18513 µs；
      ⑤ baseline 采集于 **2026-10-25 / git_sha 7c5038e**（另一台机器状态）——
         与根 `AGENTS.md §NOTES` 2026-10-25 条目登记的「共享主机 ⇒ 环境漂移」
         **同型**（该条目已给出「先做 pre-C2-vs-HEAD 对照」的排查指引；本次连对照
         都**不必做**，因为闭包里根本没有 `include/cpu/**`）。
      ⚠️ **遗留（backlog，不在本 change）**：perf baseline 与当前机器状态已脱节
      ⇒ 需要一次**专门的** baseline 重采集（带 `verify_perf_baseline_anchors.sh`
      的锚点恢复流程）⇒ 与本 change 无关。
- [x] 5.3 `run_all_ported_tests.sh` **28/28** + `run_all_samples.sh` **35/35**
- [x] 5.4 双后端逐拍全等（**无掩码**，27 个顶层输出）—— 新增用例内部已含该断言
- [x] 5.5 8 个 `scripts/verify_*.sh` 全 exit 0
      ⚠️ 其中 `verify_no_duplicate_rv32i_headers.sh` 一度报 C5 违规（`include/cpu/AGENTS.md`
      自述行数 6278 ≠ 实测 6364）⇒ 已随本次改动同步为 **6364** ⇒ 现 exit 0
- [x] 5.6 计数**不变**（实测）：`ctest -N` = **202** / `ctest -N -L verilator_e2e` = **37** /
      `EXAMPLES_TEST_SOURCES` = **17**
- [x] 5.7 `openspec validate rv32i-fetch-skid-buffer-2026-10 --strict` = **valid**；
      `--specs --strict` 的 7 passed / 19 failed 是 **CLI 1.14.3 漂移**（仓库钉 1.4.1）
      ⇒ 与本 change 无关（纯净 HEAD 同样红）

## 6. 文档与簿记

- [x] 6.1 根 `AGENTS.md §KNOWN LIMITATIONS`：两条残留**标为已消除** + **订正**
      （PC 回退已否决 / 根因 = 加载拍预填）
- [x] 6.2 根 `AGENTS.md §NOTES`：本 change 记录（含 §1.3 的重复装载实测 + 组件级判别力边界）
- [x] 6.3 `include/cpu/AGENTS.md`：CONVENTIONS 增补取指侧契约（skid / `primed` 的前提）
      ⚠️ 顺带把自述行数 6278 → **6364**（护栏 C5 要求与磁盘一致）
- [x] 6.4 `docs/architecture/rv32i-microarchitecture.md`：「已知残留」两行改为 ✅ 已修
      + 决策依赖图 / 未决项（D7 不启动）同步
- [x] 6.5 `feat-rv32i-isa-completeness` ④ 勾选 + `.rddf/roadmap.md`
- [ ] 6.6 主 spec 的 `## Purpose`（`:33-36`）**归档后手工订正**

## 7. 审稿与归档

- [x] 7.1 对抗审稿（critic，`review_profile: plan, build`）⇒ **第 1 轮 REJECT**
      （1 BLOCKER + 2 SHOULD_FIX + 1 NIT）⇒ 处置见 7.2 ⇒ 第 2 轮复审（只提交
      BLOCKER 闭合证据 + 新增 diff）
- [ ] 7.2 审稿处置（BLOCKER ⇒ 返工；SF / NIT ⇒ 同批闭合留痕）
      ✅ **第 1 轮处置（2026-10-27）**：
      - **BLOCKER**（spec delta 的退休数 NOTE 仍是实施前基线 179/20 + 冻结的 TODO
        ⇒ 归档会冻错主 spec）⇒ **已修**：NOTE 回填为 **199 − 21 ⇒ 178**、
        行号改为 `:907-908`、删掉「MUST 替换」的过程性指令、MODIFIED 头的基线
        表述收口为「188 → 179（实现前）→ 178（实现后）」；
      - **SHOULD_FIX 1**（design.md §5.1 与实现漂移：写 32 字 / `SW x16` /
        `expect_written` helper， 实现是 24 字 / 无该 store / 走写回序列 + 计数断言）
        ⇒ **已修**：§5.1 按实现逐条重写（固件布局表 + 5 条断言 + 反向期望推导）；
      - **SHOULD_FIX 2**（design.md §5.3 写「9 个输入端口」，实为 **7**）
        ⇒ **已修**（与 tasks 1.10 一致）；
      - **NIT**（Batch 4-b 的 tick2 注释：`fetch_pc` 应为 **0x80000000**
        而非 0x80000004 —— `fetch_pc_reg` 无使能地跟 `pc_reg`，晚一拍）
        ⇒ **已修**（该 skid 随即被 flush 清空 ⇒ 值不被观测 ⇒ 断言判别力不受影响）；
      ⇒ 全部**同批**闭合并留痕（design.md §6.1 已记录「组件级抓不到」的三条变异）。
- [ ] 7.3 `openspec archive rv32i-fetch-skid-buffer-2026-10 --yes`（**禁** `--skip-specs`）
- [ ] 7.4 归档后核对：`openspec list` 无活跃 change + 主 spec 已合并两条 MODIFIED
