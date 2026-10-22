## Tasks

> **状态：已实施并归档（2026-10-22）。**
> 立项自 2026-10-22 的全仓遗留审计：**40 个 sample 源文件零自动化覆盖** ⇒ 坏了没人知道
> （实测发现 2 个既有 segfault）。与 `bundle-ecosystem-hardening-2026-10` 分工：
> 本 change 只做**覆盖面**（samples 的回归护栏 + 修 `switch_demo`），
> bundle 框架的根因归那个 change。
>
> ⚠️ **本任务表经 2026-10-22 双路评审（+ critic）修订**：
> ① 新增「覆盖集合守恒」一组动作（注册一致性 + 数量守恒 + 缺失二进制判失败）——
>    防止「sample 编译失败就从护栏里消失」这一**与所治之病同型**的盲区；
> ② 豁免语义统一为「**豁免项仍然执行**，豁免只影响失败计数，转绿即报 stale 并失败」
>    （消除「豁免是否执行」的自相矛盾）；
> ③ 禁用清单落到**具体文件** `samples/disabled_samples.txt`；
> ④ 验证记录落到 `docs/reports/verification-samples-regression-guard-2026-10.md`；
> ⑤ CI job 明确 **Linux-only**（macOS 无 GNU `timeout`）；
> ⑥ `switch_demo` 根因位置更正（首个失败点 `:10`）。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4。
> ⚠️ **跨 change 顺序**：本 change **先**落地（护栏带 `nested_bundle_demo` 的
> **显式豁免**，失效条件 = `bundle-ecosystem-hardening-2026-10` archive）。

  - [x] ### Task 0: 全量基线 + 注册面清点（逐个定性既有失败）
  - **action**:
    - 跑一遍全部**已构建**的 sample，记录每个的退出码与耗时：
      ```bash
      for s in build/samples/*; do [ -x "$s" ] && [ -f "$s" ] || continue; \
          timeout 60 "$s" >/dev/null 2>&1 || echo "FAIL: $s (exit=$?)"; done
      ```
      ⚠️ 退出码 MUST 区分：`139` = SIGSEGV，`124` = timeout（`timeout` 的退出码）
    - **清点注册面**：`samples/*.cpp` = 40，`CH_SAMPLE_TARGETS` = 24 ⇒ **16 个未注册**
      ⇒ 逐个定性（**处置只有两种**，见下）：
      - **未经修改即可编译** ⇒ 直接补进 `CH_SAMPLE_TARGETS`
      - 否则 ⇒ 登记进 `samples/disabled_samples.txt`（带理由 + 归位条件）
      ⚠️ 本 change **不承担**「让未注册 sample 恢复可编译」的修复工作（Non-Goal 5）
      ⇒ 「能否编译」只做**判定**，不修
    - ⚠️ **MUST NOT** 假定「只有 2 个失败」⇒ 以本 Task 的实测为准
    - 产出两份清单，写进**验证记录**
      （`docs/reports/verification-samples-regression-guard-2026-10.md`）：
      ①「sample 名 → 退出码」；②「源码 → 注册状态（已注册 / 已禁用 + 理由）」
  - **verify**:
    ```bash
    ls samples/*.cpp | wc -l                                                  # 40
    sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' samples/CMakeLists.txt | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' | wc -l      # 24
    ```
    MUST 产出两份清单，且 16 个未注册 sample **逐个**有定性结论

  - [x] ### Task 1: 写 `scripts/run_all_samples.sh`（护栏）
  - **action**:
    - 新建 `scripts/run_all_samples.sh`：
      - 遍历 `build/samples/` 下的**可执行文件**（MUST 跳过目录与非可执行文件
        —— `build/samples/` 里有 `CMakeFiles/` / `Makefile` / `cmake_install.cmake`）
      - 每个用 `timeout 60` 执行（MUST NOT 让 CI 无限期挂起）
      - 收集退出码；**任一非零 ⇒ 脚本非零退出**
      - 输出 PASS / FAIL 汇总（FAIL 名单要能直接定位）
      - MUST 支持「先构建再跑」（或明确要求先 `cmake --build`）
    - ⚠️ **覆盖集合守恒**（本 change 的核心）：
      - **注册一致性**：每个 `samples/*.cpp` MUST 要么在 `CH_SAMPLE_TARGETS` 里，
        要么在 `samples/disabled_samples.txt`（**每行 `sample 名 <TAB> 理由 <TAB>
        归位条件`**）里 ⇒ 缺一 ⇒ **失败**
      - **数量守恒**：实际执行的 sample 数 MUST == 「**已注册数**」；
        不等 ⇒ **失败**（不是 SKIP）
        ⚠️ **豁免项也计入「已注册数」** —— 豁免项**仍然被执行**
      - **缺失二进制 = 失败**：`run_all_ported_tests.sh` 的 `SKIPPED (binary not found)`
        语义对回归护栏太弱 ⇒ 本脚本 MUST 判失败
    - ⚠️ **豁免清单**（数据驱动，语义 MUST 与 spec 一致）：
      - 显式列表：sample 名 + 根因 + 关联 change/缺陷单 + **失效条件**
      - **豁免项仍然被执行**；其失败 **MUST NOT 计入失败数**
      - 运行时 MUST **逐条打印**（对齐 `scripts/verify_rst_n_wiring.sh` 的
        `::notice` 先例 ⇒ **豁免不是静默的**）
      - ⚠️ **stale 检测**：豁免项若**已转绿** ⇒ MUST 报 stale 并**失败**
    - ⚠️ **可移植超时**：MUST 做 `command -v timeout || command -v gtimeout` 回退
      （GNU `timeout` 在 macOS 上默认不存在）
    - ⚠️ 脚本 MUST 与既有的 `run_all_ported_tests.sh` 风格一致（同样的
      输出格式 / 退出码约定）
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh; echo "exit=$?"
    ```
    MUST 列出全部 sample，且把 Task 0 发现的既有失败**报为失败**（或**显式豁免**，
    但 MUST 在输出里点名，并 MUST NOT 把它从执行集合里去掉）

  - [x] ### Task 2: 修既有失败（MUST 逐个，不得静默跳过）
  - **action**:
    - `samples/switch_demo.cpp`：**缺 `ctx_swap`** ⇒ `make_uint<4>(0)`
      （`:10`，首个失败点）在无 context 下创建节点失败（`node_builder` 报
      「No active context for literal creation」）⇒ 补 `ch::core::context` +
      `ch::core::ctx_swap`（与 `tests/test_fifo_example.cpp` 等的约定一致）
      ⚠️ 该文件共 **9 行 / 17 处** `make_uint` 调用、同一无 context 的 main 作用域 ⇒
      补 `ctx_swap` 是**全局**修复，MUST NOT 只补第一个点
    - `samples/nested_bundle_demo.cpp`：根因在 bundle 框架
      （`bundle_base(const std::string&)` 在基类构造期调虚函数）⇒ 归
      `bundle-ecosystem-hardening-2026-10`；本 change MUST 在**豁免清单**里
      **显式**记录它（附根因、关联 change、**失效条件 = 该 change archive**），
      MUST NOT 静默跳过
      ⚠️ 该豁免项**仍然会被执行**（只是失败不计入失败数）⇒ 它的崩溃 MUST 在
      输出里可见
    - ⚠️ 若 Task 0 发现其他失败 ⇒ 同样逐个定性（修 / 显式豁免），MUST NOT 跳过
  - **verify**:
    ```bash
    ./build/samples/switch_demo; echo "exit=$?"     # MUST exit=0
    bash scripts/run_all_samples.sh                 # 豁免项 MUST 被点名打印且仍被执行
    ```

  - [x] ### Task 3: 注册面收敛 + CI + 文档同步
  - **action**:
    - **注册一致性**（D2）：
      - 16 个未注册 sample 逐个处置：**未经修改即可编译** ⇒ 补进
        `CH_SAMPLE_TARGETS`；否则 ⇒ 登记进 `samples/disabled_samples.txt`
        （理由 MUST 写「为什么不能编 / 何时回来」）
      - ⚠️ `samples/CMakeLists.txt` 里现有的**注释掉的条目没有任何禁用理由**
        ⇒ 本次 MUST 把理由补上（理由写在 `samples/disabled_samples.txt`，
        CMakeLists 注明指向它）
      - ⚠️ `tlm_bundle_demo` 需特别注明：它是 `deserialize<>` 路径的 **sample 级
        消费方**（`samples/tlm_bundle_demo.cpp:121`）⇒ 该路径的 sample 级覆盖
        当前**无效**
    - **CI**：`.github/workflows/ci.yml` 新增一个 job 跑 `scripts/run_all_samples.sh`
      - MUST 挂在 **Linux** runner（`ubuntu-22.04`）⇒ macOS 无 GNU `timeout`
      - MUST 设 job 级 `timeout-minutes`（最坏 24 × 60s ≈ 24 min）
      - ⚠️ **MUST NOT** 写成「与既有的 `run_all_ported_tests.sh` job 并列」——
        CI 里**没有**那个 job（examples 在 CI 里只有编译覆盖）
    - **文档**：
      - 根 `AGENTS.md`：
        - COMMANDS 段加 `bash scripts/run_all_samples.sh`
        - 「已消除的过时信息」新增本 change 条目
        - ⚠️ **CTEST_TOTAL 不变**（samples 不是 ctest target）⇒
          `scripts/verify_test_counts.sh` 无需改，但 MUST 确认它仍通过
      - `samples/AGENTS.md`：加「每个 sample MUST 以 0 退出；由
        `scripts/run_all_samples.sh` 守护」+「新增 sample MUST 同时注册进
        `CH_SAMPLE_TARGETS`（否则护栏会因覆盖集合不一致而失败）」
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh     # MUST 仍通过（CTEST_TOTAL 不变）
    grep -n "run_all_samples" .github/workflows/ci.yml AGENTS.md samples/AGENTS.md
    # 每个 samples/*.cpp 要么已注册、要么在禁用清单里
    comm -23 <(ls samples/*.cpp | sed 's|samples/||;s|\.cpp||' | sort) \
             <( { sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' samples/CMakeLists.txt | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p'; \
                  cut -f1 samples/disabled_samples.txt | grep -v '^#'; } | sort) \
      | grep . && echo "FAIL: 有 sample 既未注册也未登记" || echo "OK"
    ```

  - [x] ### Task 4: 全量回归 + archive
  - **action**:
    - `bash scripts/run_all_samples.sh` ⇒ MUST 全绿（既有失败已修或显式豁免，
      且**无 stale 豁免**）
    - `ctest -LE perf`（期望数不变）
    - `./run_all_ported_tests.sh`（28/28，本地）
    - `openspec archive samples-regression-guard-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
    - ⚠️ **验证记录**（D13 对应）：
      `docs/reports/verification-samples-regression-guard-2026-10.md` MUST 落盘，
      含 Task 0 的两份清单 + 护栏基线
    - ⚠️ **archive 后补写新 capability 的 `## Purpose`**（D7）：
      ⚠️ **实测**：OpenSpec CLI 1.4.1 的 archive 对**新建** capability **一律生成**
      `## Purpose` = `TBD - created by archiving change <name>. Update Purpose after archive.`
      ⇒ delta 里的 `## Purpose` **不会被合并** ⇒ MUST 用 delta 的 `## Purpose`
      内容**替换**该骨架行（仓库先例：`axi4-full-slave-burst` 的 Purpose 即
      「archive 后已补写」）⇒ 替换前主 spec 会残留 change 名
    - ⚠️ 记录 follow-up：**examples 的运行覆盖不在 CI 里**（本 change 的 Non-Goal 4）
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    # ⚠️ 用**子 shell**：避免 ctest 失败后 cwd 停在 build/，导致后续 guard 假绿
    #    （`grep` 找不到目录时 exit=2、`openspec validate` 打印「No items found」且 exit=0）
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    openspec validate --specs --strict
    # ⚠️ **CLI 版本前置**：该断言依赖 openspec CLI **1.4.1**（仓库现有 spec 是按该
    #    版本的校验规则写就的）。CLI **1.14.x** 新增了两条 strict 规则
    #    （「Requirement 正文 > 500 字符」「Purpose 仍是占位符」），仓库现有 **15/17**
    #    个主 spec 会因此变红 ⇒ 与本 change 无关（纯净 HEAD 同样红）。
    #    ⚠️ CI **不跑** `openspec validate` ⇒ 该漂移**不阻断合并**。
    # 主 spec MUST NOT 残留本 change 名（Purpose 骨架已手工替换 ⇒ 0 行）
    grep -rn "samples-regression-guard-2026-10" openspec/specs/   # MUST 命中 0 行
    # Purpose MUST 是真实内容（MUST NOT 仍是 TBD 骨架）
    sed -n '3,5p' openspec/specs/sample-regression-guard/spec.md   # MUST NOT 含 TBD
    test -f docs/reports/verification-samples-regression-guard-2026-10.md
    ```
