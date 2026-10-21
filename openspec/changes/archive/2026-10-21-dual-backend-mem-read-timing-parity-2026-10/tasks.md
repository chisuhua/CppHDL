## Tasks

> **状态：已实施并 archive（2026-10-21，`2026-10-21-dual-backend-mem-read-timing-parity-2026-10`）。**
> 起因：`sim-mem-read-timing-align-2026-10` 落地后的**测试完备性审计**发现 ——
> 该 change 的核心主张「interpreter == Verilog」**没有任何跨后端测试验证**：
> capability `dual-backend-test-coverage` 的 R6 排除清单第 3 条把 aread-based
> 设计排除在外（理由正是该 change 消除的 drift），31 个 `verilator_e2e` 套件
> **没有一个跑过 `aread`**。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4。

- [x] ### Task 0: 审计取证（确认缺口真实存在）
  - **action**:
    - 读 `openspec/specs/dual-backend-test-coverage/spec.md` 的 R6 排除清单
      → 确认第 3 条排除 aread-based 设计，理由是「verilator 0-cycle vs simulator
      posedge 的必然 parity 失败」
    - 读 `tests/dual_backend_fixture.h` 的「Out of scope」清单 → 同一条理由
    - 读 `tests/test_verilator_trace_parity.cpp` 的 fixture 约束 → 同一条理由
    - 全仓 grep：确认**没有任何**双后端测试使用 `aread`
  - **verify**:
    ```bash
    grep -rn "aread" tests/dual_backend_fixture.h tests/test_verilator_trace_parity.cpp
    grep -rln "DUAL_BACKEND_TEST_PAIR" tests/ | xargs grep -l "aread"
    ```
    → 第二条命令 MUST 无输出（无 aread 的双后端测试）

- [x] ### Task 1: 写会失败的测试（RED）—— 双后端 parity
  - **action**:
    - 新建 `tests/test_mem_read_timing_dual.cpp`：DUT 用**寄存器驱动**的地址
      （`ch_reg` → `mem.aread(reg)`），读数据接**顶层 `ch_out`**
      （VerilatorBackend 无法观测深层 mem / reg-file）
    - 一个 `DUAL_BACKEND_TEST_PAIR` = 2 个 TEST_CASE，断言：
      地址寄存器提交新地址的**同一 tick**，两后端的读数据都是**新地址**的值；
      同一条件下 `sread` 仍是**旧地址**的值
    - ⚠️ MUST 用寄存器驱动的地址（输入驱动的地址两后端修复前就同拍可见 →
      锁不住任何东西）
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_mem_read_timing_dual
    ./build/tests/test_mem_read_timing_dual
    ```
    → 在 `sim-mem-read-timing-align-2026-10` **未落地**时 MUST 转红
    （interpreter 滞后 1 拍 vs verilator 0-cycle）；已落地时 MUST 全绿
  - **note（实测）**: 在 `sim-mem-read-timing-align-2026-10` 已落地的当前树上，
    该测试**直接全绿**（12 assertions / 2 test cases）→ 说明 drift 确实已消除；
    非空转性通过「临时把 D2 回退为 posedge 求值」验证：interpreter 半边转红
    （`0xA1 != 0xE5`）而 verilator 半边仍绿。

- [x] ### Task 2: spec 解锁（R6 排除清单 + cache Sync/Async 分类）
  - **action**:
    - MODIFIED `dual-backend-test-coverage` 的「适用范围排除清单」：
      删去第 3 条（aread-based 设计），并删掉「排除清单由 spec 引用 grep 模式
      定义」里的 `xargs grep -L "aread"` 过滤环节
    - MODIFIED「cache 双后端 MUST 按 Sync/Async 分类」：解除 Async 强制
      golden-value 的禁令（理由已不成立），golden-value 降为可选
    - ADDED「mem 读端口时序 MUST 有双后端 parity 覆盖」
  - **verify**:
    ```bash
    openspec validate dual-backend-mem-read-timing-parity-2026-10 --strict
    ```

- [x] ### Task 3: 补 `sread` 的 enable 拉低路径覆盖
  - **action**:
    - 该路径此前**零覆盖**（`tests/test_mem_connection.cpp` 的 sread 用例只驱动
      `enable=1`）。读端口改写自身槽位后，这条路径承载了「sync 恒读 0」的语义
      → MUST 有用例钉住
    - 断言：enable 拉低 → 读端口**保持**上次读到的值（与 Verilog `always_ff`
      的 hold 语义一致）；enable 重新拉高 → 读到新地址的值
  - **verify**:
    ```bash
    ./build/tests/test_mem_timing
    ```

- [x] ### Task 4: 清理过时注释 + 行号引用漂移 + 计数同步
  - **action**:
    - `tests/dual_backend_fixture.h` 的「Out of scope」清单删去 aread 项
    - `tests/test_verilator_trace_parity.cpp` 的 fixture 约束注释更新
      （aread 现在可以进 dual fixture）
    - `openspec/specs/mem-read-timing/spec.md` 的两处**行号引用**改为
      名称/标签引用（行号已随本次改动漂移）
    - 根 `AGENTS.md`：`CTEST_TOTAL` 180 → **181**；`ctest -L verilator_e2e`
      31 → **32**；NOTES 的测试计数同步
    - 根 `AGENTS.md`「已消除的过时信息」新增本 change 条目
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh
    cd build && ctest -N | tail -1 && ctest -N -LE perf | tail -1
    ctest -N -L verilator_e2e | tail -1
    ```

- [x] ### Task 5: 全量回归 + archive ✅（`ctest -LE perf` **170/170 全绿**（含新增的 `test_mem_read_timing_dual`）；`run_all_ported_tests.sh` **28/28 PASS**；`openspec validate --specs --strict` **16/16 通过**；主 spec 无残留 change 名引用）
  - **action**:
    - `ctest -LE perf`（新增 1 个 target → 期望 170/170 全绿）
    - `./run_all_ported_tests.sh`
    - `openspec archive dual-backend-mem-read-timing-parity-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
  - **verify**:
    ```bash
    cd build && ctest -LE perf -j4 --output-on-failure
    ./run_all_ported_tests.sh
    openspec validate --specs --strict
    ```
