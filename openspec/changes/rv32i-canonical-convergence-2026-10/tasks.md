## Tasks

> 每个任务对应一个**可验证**的结果；「RED → GREEN → REFACTOR」按 tasks 顺序执行。
> ⚠️ 本 change 的核心约束：**契约先行**（ADR-046 D12）—— delta 与代码改动**同批落地**。

### Task 1: 建立单源化护栏（先立尺子，再改代码）

- [x] T1.1 新建 `scripts/verify_no_duplicate_rv32i_headers.sh`，含 **6** 条检查：
      C1 同 basename ≤ 1 / C2 根 tests 无反向 `-I`（**跨行续行也 MUST 抓到**）/
      C3 根 tests 无 example include / C4 **硬**死头 ≤ 允许清单（**带期望条数**）/
      C5 `include/cpu/AGENTS.md` 与磁盘一致（ADR-046 D9）/
      C6 SoC 顶层端口集合与登记快照一致（ADR-046 D10）
- [x] T1.2 允许清单初始内容 = `examples/riscv-mini/src/rv32i_pipeline_top.h`（1 条，
      理由：C3 的预定 DUT；失效条件：C3 落地）
- [x] T1.3 **RED 验证**：故意造一个同名副本 ⇒ 护栏 MUST exit 1；
      故意加一处反向 `-I` ⇒ MUST exit 1；
      故意删掉允许清单里那条的「零消费者」状态 ⇒ MUST exit 1（stale 检查）
- [x] T1.5 护栏自带 **`--self-test`**（**44 个 fixture**；构造合成树 ⇒ 断言 6 条判据
      **各自有判别力**：干净树 MUST 通过；每个注入的变异 MUST 被抓到；
      分析器崩溃 / 扫描面为空 MUST fail-closed）
      —— 护栏自身的回归测试（第一轮对抗审稿要求：否则同类缺陷会重演）
      ⇒ 挂进 CI（与护栏本体同一 job 的独立 step）
      ⚠️ 第六轮审稿后 C4 源① 由「~420 行手写 include 解析器」改为**按构造 sound 的
      basename 原文提及搜索**（5 轮审稿每轮都在解析器上找到新洞 ⇒ 停止打补丁）：
      不再有「解析失败」状态 ⇒ 只漏报。对应 fixture 已重写（⑩b/⑰/⑰c/⑰d/⑳/㉖ 等）；
      ⑱ / ㉓ 因缺陷类别被结构性消除而删除。
- [x] T1.4 护栏挂进 CI（与既有 5 个护栏同一 job）

### Task 2: 删死头（17 个）

- [x] T2.1 `git rm` 17 个零消费者头（清单见 proposal §「删死头」）
- [x] T2.2 验证：全仓 `#include` 反查 + 编译 ⇒ 无任何 TU 引用它们
- [x] T2.3 保留 `rv32i_pipeline_top.h`（C3 的 DUT，零消费者已登记在 root `AGENTS.md`）

### Task 3: 提升 10 个头到 `include/cpu/`

- [x] T3.1 `git mv` 9 个到 `include/cpu/pipeline/`（`rv32i_alu` / `rv32i_regfile` /
      `rv32i_pc` / `rv32i_forwarding` / `rv32i_branch_predict` /
      `rv32i_hazard_complete` / `branch_predictor_v2`）
- [x] T3.2 `git mv` `rv32i_core.h` → `include/cpu/pipeline/hazard_detection_unit.h`
      （**改名**，ADR-046 D8；类名 `HazardDetectionUnit` 与文件名对齐）
- [x] T3.3 `git mv` `i_cache_prefetch.h` / `d_cache_write_buffer.h` → `include/cpu/cache/`
- [x] T3.4 **namespace 保持 `riscv`**（ADR-046 D8）⇒ 迁移后逐文件核对
- [x] T3.5 更新所有消费者的 `#include`（example 侧 3 个 unit test + 12 个 Catch2 test；
      根 `tests/` 侧 4 个 TU）—— 统一改为规范形式 `"cpu/pipeline/xxx.h"` /
      `"cpu/cache/xxx.h"`（顺带消除了 `../../include/...` 这种**路径深度巧合**
      形式：它此前只因 `-I <root>/include/core` 恰好是 2 层深才解析成功）

### Task 4: 收敛 4 组同名双副本

- [x] T4.1 `rv32i_decoder.h` / `rv32i_pipeline_regs.h` / `rv32i_tcm.h`：
      example 副本删，消费者重指 `include/cpu/pipeline/`
- [x] T4.2 ⚠️ 这 3 组的差异**仅**是 `using namespace ch::core;` 与 `namespace riscv {`
      的位置 ⇒ lib 版**收窄**了名字可见性 ⇒ 逐 target 编译验证；
      若某个 TU 依赖了全局可见性 ⇒ 在该 TU 内**显式**补 `using namespace ch::core;`
      （MUST NOT 回退到「保留 example 副本」）
- [x] T4.3 `hazard_unit.h`：example 副本删；3 个消费者改装配为
      「`HazardUnit`（检测）+ `chlib::ForwardingMux`（数据 mux）」
- [x] T4.4 ⚠️ mux 拆解 MUST **保持断言语义**：原 `REQUIRE(rs1_data == 42)`（前推后的数据）
      ⇒ 改为读 `ForwardingMux` 的 `alu_input_a`，**期望值不变**
- [x] T4.5 验证：`chlib::ForwardingMux` 的档位语义与原 mux 逐档等价
      （0→REG / 1→EX / 2→MEM）

### Task 5: 消除反向依赖

- [x] T5.1 删根 `tests/CMakeLists.txt` 的 6 处 `target_include_directories(... examples/riscv-mini/src)`
- [x] T5.2 改写 6 个根测试 TU 的 `#include`（`test_rv32i_pipeline` /
      `test_rv32i_pipeline_behavior` / `test_blt_debug` /
      `tests/chlib/test_branch_predictor_v2` / `tests/chlib/test_hazard_complete` /
      `tests/chlib/test_phase4_integration`）
- [x] T5.3 `tests/test_rv32i_pipeline_behavior.cpp` 的 `hazard_unit.h` include 是**多余**的
      （该文件测的是 `ForwardingUnit` / `ControlHazardUnit` / `HazardDetectionUnit`）⇒ 直接删
- [x] T5.4 example 侧 `CMakeLists.txt:19-23` 的「include 顺序敏感」注释**作废** ⇒ 删

### Task 6: 全量回归

- [x] T6.1 `cmake --build build -j$(nproc)` 0 error 0 warning
- [x] T6.2 `ctest -LE perf` **188/188** 全绿（2026-10-24 基线）
- [x] T6.3 `ctest -L verilator_e2e` 全绿（基线 34）
- [x] T6.4 `ctest -L perf` 全绿（11/11）
      ⚠️ **本次附带重生成 `tests/benchmark/perf_baseline.json`**（旧 baseline 2026-10-16
      采的，已对当前机器过时 ⇒ `perf_regression` 曾报 9–11 处「回归」）。
      **非 C2 性能变化** ⇒ 由 pre-C2（`5191c47`）vs HEAD 的**背靠背对照实验**证明：
      编译选项一致 + 依赖闭包不含 `include/cpu/` ⇒ **HEAD/pre-C2 = 0.89–1.21（均值 ~1.03）**
      ⇒ C2 零影响；而 pre-C2/baseline 同样是 1.01–2.36× ⇒ 漂移是**环境性的**。
      ⚠️ **TC-09 的 6 个锚点已逐行恢复**（本次采集 TC-09 全 SKIPPED；`perf_regression`
      的 `base<=0 ⇒ continue` 会让 `median_us=0` 的行**永久退出比较集合** ⇒
      未来的 TC-09 真回归会不可见）。
      详见 proposal §「附带改动」+ 新 baseline 的 `note` 字段。
- [x] T6.5 5 个既有护栏脚本 + `run_all_ported_tests.sh`（28/28）+ `run_all_samples.sh`（35/35）
- [x] T6.6 新护栏 `scripts/verify_no_duplicate_rv32i_headers.sh` 绿
- [x] T6.7 新护栏 `scripts/verify_perf_baseline_anchors.sh` 绿（3 条守恒检查）
      ⇒ 守住「重生成 baseline 时行/锚点静默消失」与「变松未披露」两类**纯人工**风险
      （`perf_regression.cpp:179` 的 `if (base <= 0.0) continue;` ⇒ 锚点降级为 0
      = 未来真回归永久不可见）
      ⇒ 护栏自带 `--self-test`（**15 项断言**；它在开发中**实测暴露过 3 个真缺陷**：
      heredoc 后残留的 `RC=$?` 覆盖退出码 ⇒ 违规只打印 CI 却见 PASS；
      披露判定曾用「三个子串各自出现」⇒ 删掉披露表的行不会被发现）
      ⇒ 变异验证：退回任一缺陷 ⇒ 对应 fixture 转红
      ⇒ 已挂进 CI 两个 job（`build-and-test` / `pr-verilator-fast`）的既有 step 内，
      且 **`--self-test` 先跑**（未测试的闸门 = 形同虚设）

### Task 7: 文档同步

- [x] T7.1 `include/cpu/AGENTS.md`：STRUCTURE / KEY FILES / NAMESPACE MAP 与磁盘一致；
      删幽灵条目；修正自述计数
- [x] T7.2 root `AGENTS.md`：STRUCTURE / NOTES 的 `cpu/` 条目同步
- [x] T7.3 `examples/riscv-mini/AGENTS.md`：同步 example 侧收敛后的事实
- [x] T7.4 ADR-046 §5 回填 `executed`；D3 #14 补「mux 拆到 `chlib::ForwardingMux`」的落地形态
- [x] T7.5 root `AGENTS.md` 的 KNOWN LIMITATIONS：`rv32i_pipeline_top.h` 零消费者的
      条目 MUST 保留并指向 C3

### Task 8: 归档前 gate

- [x] T8.1 `openspec validate rv32i-canonical-convergence-2026-10` 绿
- [x] T8.2 `bash scripts/verify_openspec_delta.sh rv32i-canonical-convergence-2026-10` 绿
- [x] T8.3 对抗式审稿（critic）通过 ⇒ **第 13 轮 APPROVE**（裸通过；此前第 6/7/8/9 轮 REJECT、第 10/11 轮 APPROVE_WITH_CONDITIONS、第 12 轮 REJECT 的整改均已落地并逐项变异自证）
      ⚠️ **第 6 轮审稿（2026-10-25）结论 = REJECT：3 个 BLOCKER + 7 个 SHOULD_FIX
         + 若干 NIT** ⇒ 全部整改完毕（见下），待第 7 轮复审：
      ① **B1** fixture ⑰ 的「MUST NOT 跨文件拼接」属性**空转**（悬空续行文件
         排序上**不与**目标头相邻 ⇒ 该场景从未被构造；变异
         `self[last]`→`self[FILENAME]` 仍 33/33 全绿）⇒ fixture 改为把悬空
         续行文件放在 `include/cpu/aaa_dangling.cpp`（排序紧邻 alpha.h 之前）
         ⇒ 变异被抓住（实测）。
      ② **B2** 豁免 stale 检查**方向倒置**（用「原文提及」证明「已有消费者」）
         ⇒ 纯注释提及即产生假违规，且错误信息指示「删豁免行」⇒ 链条终点是
         删掉 D10 冻结的 C3 DUT ⇒ 改为**只认 `.d` 真 include 证据**；源① 提及
         只**警告**且措辞不得指示删除 ⇒ 新增 fixture ⑨b / ⑨c。
      ③ **B3** perf 护栏的 `note_restored` 是**降级逃逸口**（加一个 JSON 字段
         就能把 PASS 锚点永久摘掉）且提示语与事实相反 ⇒ 删掉逃逸分支，改为
         「带该标记的行 MUST 断言取值与基准**逐字相等**」⇒ 新增 fixture ⑤⑥。
      ④ **S1/S2** 扫描面 prune 从 `build*` glob 改为**显式白名单**（`build*`
         会连带 prune 掉 `buildtools/`、`builder/` 等**源码目录** ⇒ 活头可被
         误判成死头）；探针的扩展名清单扩为**超集**（`.h++`/`.cu`/`.tcc`/
         `.mm`… 此前落在扫描面外却不报违规）。
      ⑤ **S3/S4** 缺 python3 / ref 解析不到 ⇒ 改 `::warning`（不静默）；
         CI 的 `git fetch` 失败 ⇒ **硬失败**（去掉 `|| true`），否则闸门在 PR
         上静默失效而 CI 仍绿。
      ⑥ **S5** TC-09 的 6 个恢复锚点记的是 2026-10-16 机器状态，而**本机已
         慢 1.3–2.1×** ⇒ 「偏严」不是假想而是**必然误报** ⇒ 已写进 baseline
         的 `note`（修 180 s 超时的人 MUST 一并重采）。
      ⑦ **S6/S7** perf 护栏的阈值改为从 `perf_regression.cpp` 读出（消除
         硬编码副本的漂移风险）；披露表的 `old=/new=/ratio=` **数值**现在与
         实际比对（写错也算违规）⇒ 新增 fixture ⑦。
      ⑧ **NIT** 修了：fixture ⑰c 的断言串无判别力、`basename` 外部命令、
         未加引号的 word splitting、`ARGC` 对 `-v` 实参计数的实现差异依赖
         （改用 `ENVIRON` + `ARGV` 位置）、baseline 里的机器专属绝对路径。
      ⇒ 护栏 fixture 数 **33 → 44**；perf 护栏 self-test **5 → 15** 项断言。
      ⚠️ **第 7 轮审稿（2026-10-25）结论 = REJECT：3 个 BLOCKER + 7 个 SHOULD_FIX
         + 若干 NIT** ⇒ 全部整改完毕（见下），待第 8 轮复审：
      ① **B-1** stale 豁免检查用「`.d` 里出现过」当「已有消费者」⇒ `.d` 是
         **累积**产物（TU 被删后残留）⇒ **假违规** + 破坏性指令（链条终点
         仍是删掉 D10 冻结的 C3 DUT）⇒ 改为要求「当前证据」：源文件**仍存在**
         + **仍提及**该 basename + `.d` **比源文件新**；否则**只警告**。
         ⇒ 新增 fixture ⑨c（当前的真 include ⇒ 报违规）/ ⑨d（陈旧 `.d` ⇒ 不报）。
      ② **B-2** prune 白名单**仍是前缀 glob**（`build-*`）⇒ 本仓真实存在的
         `openspec/specs/build-cpp-standard/` 被 prune ⇒ 里面的消费者对源①
         与探针**同时隐形** ⇒ 活头可被判死 ⇒ 改为**内容判定**（只有含
         `CMakeCache.txt` 的目录才算构建输出目录）⇒ 探针与扫描共用 prune
         列表的前提因此成立（prune 掉的必然是**生成物**）。
      ③ **B-3** perf 护栏的阈值回落是 **fail-open**（权威源不可解析时静默用
         内置副本）且该路径**零覆盖**（self-test 的 fixture 全部没有
         `perf_regression.cpp`）⇒ 删掉内置副本，权威源不可解析 ⇒
         **fail-closed**（报违规）；fixture 改为**自带**权威阈值源，并新增
         3 条断言（权威源 9.9 ⇒ 3× 不算慢 / 权威源 1.1 ⇒ 未披露必报 /
         权威源不可解析 ⇒ fail-closed）⇒ 另加「阈值表 MUST 覆盖 baseline
         里的所有 backend」。
      ④ **SHOULD_FIX** ⑨b 断言串改成具体文本（原 `::warning` 无判别力）；
         源②「当前证据」与主判定统一用 `deps_stale_files` 判据；ci.yml 的
         C2 注释改成与实现一致（**逐行**判定，不是计数比较）；文档里
         「5/8 项断言」的自相矛盾全部同步为 **11**；`cp "$BASELINE"` 缺文件
         时给友好信息。
      ⑤ **NIT** 允许清单加**格式校验**（3 个 TAB 分隔字段）；未知 backend
         不再静默跳过（报违规）。
      ⚠️ **第 8 轮审稿（2026-10-25）结论 = REJECT：2 个 BLOCKER + 2 个 SHOULD_FIX
         + 若干 NIT** ⇒ 全部整改完毕（见下），待第 9 轮复审：
      ① **B-1** 「当前消费者」检测在**真实** `.d` 上恒失效：真实 CMake/GCC 的
         `.d` 是**多行续行**格式（源文件路径在续行里）⇒ 旧提取
         (`sed -n 's/^[^:]*: //p'`) 只处理第一行 ⇒ 提取到 **0** 个源文件 ⇒
         检测恒为「无当前证据」⇒ 整条「豁免 stale ⇒ MUST 移除该行」的检测能力
         在生产环境**恒为 0**（而 fixture ⑨c 断言的是**单行** `.d` —— 真实
         构建里从不出现的形状 ⇒ 零真实覆盖）⇒ 提取改为按 token 流解析
         （`tr ' \\' '\n\n'` + 去冒号 + 扩展名过滤）⇒ 新增 fixture ⑩g
         （**真实形状的多行 `.d`** ⇒ MUST 判 stale）。
      ② **B-2** 当前性判据的 (b)(c) 两条**零自测覆盖**；且 fixture ⑨c 因
         「写 TU」与「写 .d」落在**同一 fs tick** ⇒ 时间差判据为假 ⇒ fixture
         自己转红（flaky）⇒ ① (b) 改为「源文件**仍 `#include` 它**」且
         **只认 `#include` 指令行**（否则注释里的提及会被当成真 include 证据
         ⇒ 第 6 轮 B2 的方向倒置在 (b) 上重演）⇒ 新增 fixture ⑨e
         （源文件不再 include ⇒ 只警告）；② (c) 改为**显式时间差**（第 9 轮修正：阈值 = **0** 而非 1 秒 —— 实测
      `g++ -MD` 产出与源文件**同秒**的 `.d` ⇒ 1 秒硬阈值必然漏判）
         （不用裸 `-nt`：macOS bash 3.2 只比秒级 st_mtime）⇒ 新增 fixture ⑨f
         （`.d` 比源文件旧 ⇒ 只警告）；③ fixture 用 `touch -t` 显式钉住
         mtime 关系 ⇒ 消除 tick 偶然性。
      ③ **SHOULD_FIX** prune 改为**纯内容判定**（任何含 `CMakeCache.txt` 的
         目录都是构建输出目录，**与名字无关** ⇒ `cmake -B out` 这类合理布局
         不再触发「扫描面之外」假违规）⇒ 新增 fixture ⑩h（无 CMakeCache.txt
         的目录里的 TU MUST 被扫到 —— 断言用**扫描面 TU 计数**，因为源① 的
         「文档提及即判活」会让「判活」断言空转）/ ⑩i（名字非 `build*` 的
         构建输出目录里的生成 `.c` MUST NOT 报违规）；允许清单格式校验补
         fixture ⑩j。
      ④ **NIT** `REPO_ROOT` 改 `pwd -P`（symlink 场景下每个 `.d` 都会被判
         「非本树构建」⇒ 源② 全面停摆）；perf 护栏的 self-test 缺依赖 ⇒
         **exit 1**（静默 exit 0 ⇒ 「自测已通过」是假的）；JSON 损坏 /
         `runs` 缺失 ⇒ 人类可读信息 + fail-closed。
      ⚠️ **第 11 轮审稿（2026-10-25）结论 = APPROVE_WITH_CONDITIONS（无 BLOCKER；
         放行条件 C1/C2）** ⇒ 已全部收口：
      ① **C1（`#if 0` 非激活区形态）**，按 critic 的 (a) 方案处置 →
         修掉脚本注释里**与实现相反**的论断（「`#if 0` ⇒ 漏判 ⇒ 安全」），
         改为**如实记录**：「`#if 0` / `#ifdef` 非激活区里的字面 include
         仍被计入证据（已知误报向量；可达性受限 —— 需 `.d` 提及 + `.d`
         不早于 TU + TU 只在非激活区提及三者同时成立 ⇒ 当前仓库不可达）」
         ⇒ spec / ADR / AGENTS / proposal 四处同步，**MUST NOT 记成「漏判」**；
         并新增 fixture ⑨h 钉住该实际行为（将来真修跳过非激活区时 ⇒ ⑨h 转红
         ⇒ MUST 同步契约；真修需嵌套 `#if`/`#else` 语义，当时被否决）。
      ② **C2** `proposal.md` 的「只认 `#include` 指令行」→「比对 include 的
         操作数」（一行）。
      ③ **NIT** 上轮残留的 2 个：`runs` 基准侧校验补 fixture ⑮（构造过程
         自查又抓到 1 个 fixture 构造错误：第二次 commit 后未恢复工作副本
         ⇒ 两侧都缺 ⇒ 空转 —— 已修）；`st_out` 失败行带 fixture 名。
      ⚠️ **第 12 轮审稿（2026-10-25）结论 = REJECT（无生产缺陷；三项收尾）**⇒ 已收口：
      ① **BLOCKER** ⑮ **空转（第二次）** —— 第 11 轮的修复**确实写进了文件
         并实测有判别力**（变异下独自转红），但随后一条命令用**旧备份**
         `pba6.bak` 恢复脚本 ⇒ 修复被覆盖回「两侧都缺」的空转版 ⇒
         「已修」声明与实物不符。**教训**：变异操作的备份恢复 MUST 用
         **当轮**快照（带时间戳命名），且修后 MUST 立即 `grep` 自证。
         ⇒ 本轮重修 + `grep new_with_runs` 自证（=2）+ 变异自证
         （基准侧移出校验 ⇒ **⑮ 独自转红** ✔）。
      ② **SHOULD_FIX** proposal.md 的 ② 段补「已知误报向量」段
         （="$(grep 误报向量 proposal)=1"，四处同步声明由此与实物相符）。
      ③ **NITS** 脚本 :1321 的交叉引用错号 `feature ⑭` → `fixture ⑨h`
         （⑭ 是 C6 端口快照 fixture；本轮新引入的笔误——跟随错误会把
         后人引到无从定位契约同步点的地方）。
      ④ critic 另**独立确认** 了重点 1 的立场：⑨g（钉「操作数收窄」）与
         ⑨h（钉「预处理跳过」）**正交**、各自判别力可实变验证，立场成立。
      ⚠️ **第 10 轮审稿（2026-10-25）结论 = APPROVE_WITH_CONDITIONS（无 BLOCKER；
         归档前收口 2 个 SHOULD_FIX + 4 NIT）** ⇒ 已收口（见下）：
      ① **SF-1** 判据 (b) 收窄到 `#include` **操作数**（新辅助
         `tu_includes_basename`：取引号 / 尖括号内的文本、取**最后一段**
         == basename）—— 若只比对「行首是 `#include` 的整行」，行尾注释形态
         （`#include "other.h" // rv32i_pipeline_top.h`）与 `#if 0` 非激活区
         的提及仍会产生**破坏性**假违规（reviewer 实测复现）⇒ 新增 fixture ⑨g
         （变异「退回整行匹配」⇒ **只有 ⑨g 红** ⇒ 专属判别力 ✔）。
      ② **SF-2** ⑪/⑫ 的断言串改成**判据专属**（generic `VIOLATION` 被
         C1 / runs 结构校验兜住 ⇒ 无法隔离判别自己命名的判据）+ 补 fixture ⑬
         （**两侧都缺 `runs`** ⇒ 该形态下 C1 也兜不住 ⇒ 只有 runs 校验自己
         能测）⇒ 变异验证：JSON except fail-open ⇒ ⑪ 红；runs 校验 fail-open
         ⇒ ⑫⑬ 红 ✔。
      ③ **NIT** ci.yml 两处计数 2→3；`.d` 证据搜索根（`find build`）的注释
         写明（`build-jit/` / `build-test/` 不在语料 ⇒ 证据更少 ⇒ 方向保守）。
      ⑤ **过程中自查抓到 2 个真 bug**：`${bn//./\\.}` 的 glob 语义（`.` 匹配
         **任意字符** ⇒ 把每个字符都替换 ⇒ 正则永不匹配）与
         `dep_mentions_basename` 的**参数顺序颠倒**（把 `.d` 路径当成
         basename）⇒ 两者都由新 fixture 暴露（⑨c / ⑩g 转红）。
- [ ] T8.4 `openspec archive` 合并主 spec + 手写补写 `## Purpose`（归档不会改它）
