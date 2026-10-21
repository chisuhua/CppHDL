## Tasks

> **状态：草案（尚未实施）。**
> 立项自 2026-10-22 的全仓遗留审计：**40 个 sample 零自动化覆盖** ⇒ 坏了没人知道
> （实测发现 2 个既有 segfault）。与 `bundle-ecosystem-hardening-2026-10` 分工：
> 本 change 只做**覆盖面**（samples 的回归护栏 + 修 `switch_demo`），
> bundle 框架的根因归那个 change。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4。

- [ ] ### Task 0: 全量基线（逐个定性既有失败）
  - **action**:
    - 跑一遍全部 40 个 sample，记录每个的退出码与耗时：
      ```bash
      for s in build/samples/*; do [ -x "$s" ] && [ -f "$s" ] || continue; \
          timeout 60 "$s" >/dev/null 2>&1 || echo "FAIL: $s (exit=$?)"; done
      ```
    - 逐个定性既有失败（当前已知：`nested_bundle_demo` segfault、
      `switch_demo` segfault）
    - ⚠️ **MUST NOT** 假定「只有这两个」⇒ 以本 Task 的实测为准
  - **verify**: 产出一份「sample 名 → 退出码」清单，写进本 change 的实施记录

- [ ] ### Task 1: 写 `scripts/run_all_samples.sh`（护栏）
  - **action**:
    - 新建 `scripts/run_all_samples.sh`：
      - 遍历 `build/samples/` 下的**可执行文件**（MUST 跳过目录与非可执行文件）
      - 每个用 `timeout 60` 执行（MUST NOT 让 CI 无限期挂起）
      - 收集退出码；**任一非零 ⇒ 脚本非零退出**
      - 输出 PASS / FAIL 汇总（FAIL 名单要能直接定位）
      - MUST 支持「先构建再跑」（或明确要求先 `cmake --build`）
    - ⚠️ 脚本 MUST 与既有的 `run_all_ported_tests.sh` 风格一致（同样的
      输出格式 / 退出码约定）
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh; echo "exit=$?"
    ```
    MUST 列出全部 sample，且把 Task 0 发现的既有失败**报为失败**

- [ ] ### Task 2: 修既有失败（MUST 逐个，不得静默跳过）
  - **action**:
    - `samples/switch_demo.cpp`：**缺 `ctx_swap`** ⇒ `make_uint<8>(0)` 在无 context
      下创建节点失败（`node_builder` 报「No active context for literal creation」）
      ⇒ 补 `ch::core::context` + `ch::core::ctx_swap`
      （与 `tests/test_fifo_example.cpp` 等的约定一致）
    - `samples/nested_bundle_demo.cpp`：根因在 bundle 框架
      （`bundle_base(const std::string&)` 在基类构造期调虚函数）⇒ 归
      `bundle-ecosystem-hardening-2026-10`；本 change MUST 在豁免清单里
      **显式**记录它（附根因与关联 change），MUST NOT 静默跳过
    - ⚠️ 若 Task 0 发现其他失败 ⇒ 同样逐个定性（修 / 显式豁免），MUST NOT 跳过
  - **verify**:
    ```bash
    ./build/samples/switch_demo; echo "exit=$?"     # MUST exit=0
    ```

- [ ] ### Task 3: 挂进 CI + 文档同步
  - **action**:
    - `.github/workflows/ci.yml`：新增一个 job 跑 `scripts/run_all_samples.sh`
      （与既有的 `run_all_ported_tests.sh` job 并列）
    - 根 `AGENTS.md`：
      - COMMANDS 段加 `bash scripts/run_all_samples.sh`
      - 「已消除的过时信息」新增本 change 条目
      - ⚠️ **CTEST_TOTAL 不变**（samples 不是 ctest target）⇒
        `scripts/verify_test_counts.sh` 无需改，但 MUST 确认它仍通过
    - `samples/AGENTS.md`：加「每个 sample MUST 以 0 退出；由
      `scripts/run_all_samples.sh` 守护」
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh     # MUST 仍通过（CTEST_TOTAL 不变）
    grep -n "run_all_samples" .github/workflows/ci.yml AGENTS.md samples/AGENTS.md
    ```

- [ ] ### Task 4: 全量回归 + archive
  - **action**:
    - `bash scripts/run_all_samples.sh` ⇒ MUST 全绿（既有失败已修或显式豁免）
    - `ctest -LE perf`（期望数不变）
    - `./run_all_ported_tests.sh`（28/28）
    - `openspec archive samples-regression-guard-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    cd build && ctest -LE perf -j4 --output-on-failure
    cd .. && ./run_all_ported_tests.sh
    openspec validate --specs --strict
    grep -rn "samples-regression-guard-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
