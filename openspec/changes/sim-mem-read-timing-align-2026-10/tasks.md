## Tasks

> **状态：草案（尚未实施）。**
> 立项自 2026-10-21 会话中的一次未提交实验（"EXPERIMENT B2"）——
> 该实验实测**不可直接提交**（5 个测试转红，根因：Verilog codegen 双驱动 +
> 未声明信号），完整 diff 见 `proposal.md` 附录 A。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7。
>
> ⚠️ **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-5 逐条转绿，
> Task 6-7 回归。每个 Task 的 verify 必须是**可执行命令**。
>
> ⚠️ **本 change 改的是 Simulator 的时序语义**（不是局部 bug 修复）——
> 回归面远大于 `ch_mem` 本身，Task 6 的回归清单 MUST 全跑，不允许抽样。

- [ ] ### Task 0: 决策点裁决 + RED 基线
  - **action**:
    - ⚠️ **先裁决**（MUST NOT 由实施者临场随机选）：
      codegen 的处理走**方案 A**（把 `type_mem_read_port` 加入 wire 声明 +
      `print_proxy` 跳过 mem read port 的代理）还是**方案 B**（只在 codegen
      侧排序，不改 AST 形状）？
      → **倾向方案 A**：interpreter 的求值顺序来自 `context::get_eval_list()`
      的拓扑排序，而该排序**只沿 `src` 边递归** → 不加 `src` 边就无法表达
      「端口先于代理」→ 方案 B 对 interpreter 无效。
      → 裁决结果 MUST 写进本 change 的 tasks
    - 记录 RED 基线：在 Task 3 之前，当前 HEAD（`7aefdfe` 之后）的
      `ctest -LE perf` 必须全绿 → 若已红，先修红再动本 change
  - **verify**:
    ```bash
    cd build && ctest -LE perf -j4 --output-on-failure
    ```

- [ ] ### Task 1: 建立隔离的实验分支 / worktree
  - **action**:
    - 本 change 触及 **Simulator 核心求值路径** → MUST 在独立分支上做
      （禁止直接在 main 上试错）
  - **verify**:
    ```bash
    git status --short   # MUST 为空
    git branch --show-current
    ```

- [ ] ### Task 2: 写会失败的用例（RED）
  - **action**:
    - 在 `tests/test_mem_comb_read.cpp` 追加 / 改写用例：
      1. **aread 0-cycle**：地址在某个 tick 改变后，该 tick 结束时
         `rdata_async` MUST 已是**新地址**的值（当前实现：旧值 → RED）
      2. **sread 保持 1-cycle**：`rdata_sync` MUST 仍是旧地址的值（防回归护栏）
      3. **竞态值反转**：同拍写+读 → `rdata_async` MUST 返回**写后**值
         （当前钉住的是写前值 → 该用例在改实现前 RED）
    - ⚠️ 用例 MUST 复用既有 `CombReadFixture`（MUST NOT 新建 fixture）
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_mem_comb_read
    ./build/tests/test_mem_comb_read "*aread*"
    ```
    MUST 观察到**红**（RED 基线）

- [ ] ### Task 3: Simulator 侧 —— async read port 归组合 + 数据流边
  - **action**:
    - `src/simulator_init.cpp`：`type_mem_read_port` 的 **async** 子类归入
      `combinational_instr_list_`（sync 保持 `sequential_instr_list_`）
    - `include/ast/mem_port_impl.h` + `src/ast/mem_port_impl.cpp`：
      建立 `read_port → data_proxy` 的 `src` 边；读端口指令写**自身**的
      `data_map` 槽位，由代理的 `instr_proxy` 搬运到代理槽位
    - ⚠️ **必须同时保证**：`data_map` 中读端口节点自身的槽位存在且宽度正确
      （`size_` = `DATA_WIDTH`）
  - **verify**:
    ```bash
    ./build/tests/test_mem_comb_read "*aread*"
    ```
    Task 2 的用例 1 MUST 转绿；用例 2 MUST 仍绿

- [ ] ### Task 4: codegen 侧 —— 让数据流边不破坏 Verilog 生成
  - **action**:
    - 按 Task 0 裁决的方案实现（方案 A）：
      - `print_header` 的 wires 收集加入 `type_mem_read_port`（否则
        `print_proxy` 发射的 `assign` 引用**未声明信号**）
      - `print_proxy` MUST 跳过「其 `src(0)` 是 mem read port」的代理
        （否则同一 wire **双驱动**）
      - `print_mem_read_port` 保持 `async_read` → `assign`、
        `sync_read` → `always_ff` 的分派不变
    - ⚠️ 这正是原实验「5 个测试转红」的根因 → 本 Task MUST 与 Task 3 同批验证
  - **verify**:
    ```bash
    ./build/tests/test_mem_comb_read "*codegen*"
    cd build && ctest -R "test_cache_sync|test_branch_predictor_v2|test_verilator_memory_e2e|test_verilator_complex_e2e|spinalhdl_branch_predictor_v2" --output-on-failure
    ```
    MUST 全绿（原实验下这 5 个转红）

- [ ] ### Task 5: 文档同步
  - **action**:
    - `docs/simulation/ARCHITECTURE.md`「Memory read port timing」小节：
      删掉「register-driven read address skew」，改写竞态语义
    - 根 `AGENTS.md` CONVENTIONS 的「Mem read port」条目：同步
    - `src/codegen_verilog.cpp::print_mem_read_port` 的大段注释：同步
  - **verify**:
    ```bash
    grep -n "Mem read port" AGENTS.md
    grep -n "Memory read port timing" docs/simulation/ARCHITECTURE.md
    ```
    MUST 命中且内容已更新

- [ ] ### Task 6: 全量回归（**不允许抽样**）
  - **action**:
    - 跑下列**全部**套件（本 change 改的是 Simulator 时序语义）：
      ```bash
      cd build && ctest -LE perf -j4 --output-on-failure
      cd build && ctest -L perf --output-on-failure      # ~15-20 min
      ./run_all_ported_tests.sh                          # 28 examples
      ```
    - 重点观察（时序敏感）：`test_mem_comb_read` / `test_cache_sync` /
      `test_cache_complete` / `test_branch_predictor_v2` /
      `test_rv32i_pipeline` / `test_fifo_example` /
      `test_verilator_memory_e2e` / `test_verilator_complex_e2e`
    - ⚠️ **双后端 parity 只会变好不会变坏**（Verilog 侧本就是 0-cycle）——
      若某个 dual-backend 用例转红，先怀疑 interpreter 侧的新语义
  - **verify**: 上述三条命令 MUST 全绿（0 failed）

- [ ] ### Task 7: spec 落地 + archive
  - **action**:
    - `openspec validate sim-mem-read-timing-align-2026-10 --strict` MUST 通过
    - `openspec archive sim-mem-read-timing-align-2026-10 -y`（**MUST NOT** 用
      `--skip-specs`）
    - archive 后 MUST 补写主 spec 的 `## Purpose`（CLI 会留下 TBD 骨架）
    - 主 spec 内的 change 名引用 MUST 全部改为指向 capability
  - **verify**:
    ```bash
    openspec validate --specs --strict
    grep -rn "sim-mem-read-timing-align-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
