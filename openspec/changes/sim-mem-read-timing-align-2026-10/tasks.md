## Tasks

> **状态：已实施（Task 0-6 完成；Task 7 archive 待执行）。**
> 实施分支：`feature/sim-mem-read-timing-align-2026-10`（基于 `bd56b4c`）。
>
> **执行结果（2026-10-21）**：
>
> | Task | 结果 |
> |---|---|
> | 0 | ✅ 决策点已在 `proposal.md` D1 定稿（A′ 四步）；基线实测 pristine HEAD `bd56b4c` **169/169 全绿**（`ctest -LE perf`），4 个 mem 套件逐个直跑全绿 → 「pre-existing 红」不成立，已改写该条 |
> | 1 | ✅ 分支 `feature/sim-mem-read-timing-align-2026-10` |
> | 2 | ✅ RED 达成：`test_mem_comb_read` **2 处红**（寄存器驱动 aread 拿到 `0xA1` 而非 `0xE5`；竞态拿到 `0xC3` 而非 `0xEE`）+ `test_fifo_example` **1 处红**（pop 后 `dout=0x11` 而非 `0x22`）；sread 护栏用例改前即绿 ✓ |
> | 3 | ✅ `test_mem_comb_read` 的 2 处红转绿；`test_mem` / `test_mem_timing` / `test_mem_connection` / `test_memory` 全绿（async+sync 同步改的回归证据） |
> | 4 | ✅ 生成 Verilog 实测合法（无未声明引用、无双驱动）；**新增自动化护栏**「生成 Verilog 不得引用未声明信号」—— 临时撤掉 A′.3 修复即抓到 **4 处**未声明引用（证明护栏非空转） |
> | 5 | ✅ `docs/simulation/ARCHITECTURE.md`「Memory read port timing」+ 根 `AGENTS.md`（CONVENTIONS + 「已消除的过时信息」新增条目）+ `print_mem_read_port` 注释 + 3 个 FIFO sample/example 注释同步 |
> | 6 | ✅ `ctest -LE perf` **169/169 全绿**（含 31 个 `verilator_e2e` 双后端 parity + 25 个 `spinalhdl-ported`）；`run_all_ported_tests.sh` **28/28 PASS**；`ctest -L perf` **11/11 全绿**（1311 s；`perf_tests` 617 s / `perf_three_way` 90 s；`perf_results.json` **18/18 PASS**）。⚠️ `samples/fifo_bundle` **既有失败**（bundle 方向定义问题，与本 change 无关，基线同样失败） |
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7。
>
> ⚠️ **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-5 逐条转绿，
> Task 6-7 回归。每个 Task 的 verify 必须是**可执行命令**。
>
> ⚠️ **本 change 改的是 Simulator 的时序语义**（不是局部 bug 修复）——
> 回归面远大于 `ch_mem` 本身，Task 6 的回归清单 MUST 全跑，不允许抽样。

- [x] ### Task 0: 决策点已定稿 + RED 基线
  - **action**:
    - ✅ **决策点已在 `proposal.md` D1 定稿（方案 A′ 四步），实施者 MUST NOT 重新裁决**：
      | 步骤 | 内容 | 位置 |
      |---|---|---|
      | A′.1 | 建立 `read_port → data_proxy` 的 `src` 边 | `include/ast/mem_port_impl.h` |
      | A′.2 | `print_proxy` 跳过「`src(0)` 是 mem read port」的代理 | `src/codegen_verilog.cpp:1019` |
      | A′.3 | `print_output` 的代理链遍历**不得越过 mem read port** | `src/codegen_verilog.cpp:572-579` |
      | A′.4 | `type_mem_read_port` **MUST NOT** 进 wire 声明列表 | `src/codegen_verilog.cpp:425-485` |
      > ⚠️ **A′.3 漏做 → 生成非法 Verilog**（`assign out = <read_port>` 引用未声明信号）。
      > 详见 `proposal.md` D1 的「A′.3 的坑」。
    - 记录 RED 基线：在 Task 3 之前，当前 HEAD 的 `ctest -LE perf` 必须全绿
      → 若已红，先定位根因再动本 change
      ⚠️ **2026-10-21 基线实测（pristine HEAD `bd56b4c`，本仓库当前构建配置）**：
      ```
      cd build && ctest -LE perf -j4
      → 100% tests passed, 0 tests failed out of 169
      ```
      - `test_mem_comb_read` / `test_mem_timing` / `test_mem_connection` /
        `test_memory` **逐个直跑全部全绿**（15 / 37 / 20 / 10 assertions，0 failed）；
      - 25 个 `spinalhdl-ported` 套件**全部实际运行并通过**（非 "Not Run"）；
      - 31 个 `verilator_e2e` 套件全绿；`ctest -N` = 180 / non-perf = 169。
      ⇒ **本 change 的 baseline 是全绿的**。MUST NOT 以「pre-existing 红」为由
      跳过任何回归。
      > ⚠️ 若在**其它构建配置**下看到 `spinalhdl_*` "Not Run" 或 mem 套件红，
      > 那是**构建配置问题**（如 SpinalHDL 工具链缺失 / Verilator 冷编译失败），
      > **不是**本 change 的 baseline —— MUST 先把构建配置修对再判定红绿。
  - **verify**:
    ```bash
    cd build && ctest -LE perf -j4 --output-on-failure
    # 期望：0 failed out of 169
    ```

- [x] ### Task 1: 建立隔离的实验分支 / worktree
  - **action**:
    - 本 change 触及 **Simulator 核心求值路径** → MUST 在独立分支上做
      （禁止直接在 main 上试错）
  - **verify**:
    ```bash
    git status --short   # MUST 为空
    git branch --show-current
    ```

- [x] ### Task 2: 写会失败的用例（RED）
  - **action**:
    - ⚠️ **用例 MUST 用「寄存器驱动地址」**（`ch_reg` → `mem.aread(reg.impl())`），
      **不能**用顶层输入端口驱动地址。
      **理由（2026-10-21 实测）**：drift **只在地址经过寄存器时出现** —— 地址直接
      来自输入端口时，修复前后**都是同拍可见**（既有测试
      `tests/test_mem_comb_read.cpp:135-177`「aread observable same-tick」正是断言
      这一点，且**今天就绿**）→ 用它写用例**拿不到 RED**。
    - ⇒ **允许扩展 `CombReadFixture`**（加一个 `ch_reg` 驱动的地址路径），
      或**新建**一个专用 fixture。**「MUST NOT 新建 fixture」这条约束作废** ——
      它与「MUST 观察到红」互斥。
    - 在 `tests/test_mem_comb_read.cpp` 追加 / 改写用例：
      1. **aread 0-cycle（寄存器驱动）**：把地址寄存器从 A 改到 B 后，**该 tick 结束
         时** `rdata_async` MUST 已是 `mem[B]`（当前实现：`mem[A]` → **RED**）。
         ⚠️ 这条是本 change 的**核心行为锁**，它区分得开修复前后。
      2. **sread 保持 1-cycle（寄存器驱动）**：同一地址寄存器下，
         `rdata_sync` MUST 仍是**旧地址**的值（改前改后逐位一致 → 该用例
         改前就绿，是**防回归护栏**，不是 RED 用例）。
      3. **竞态值反转**（**保持输入驱动**）：同拍写+读 → `rdata_async` MUST 返回
         **写后**值（当前钉住的是写前值 → **RED**）。
         ⚠️ MUST 保持输入驱动：竞态的现有钉住用例
         （`tests/test_mem_comb_read.cpp:340-371`，tag `[mem][aread][race]`）
         用的就是输入驱动地址；改成寄存器驱动会**丢掉**对它的覆盖。
      4. **FIFO 读数据行为断言（新增，防「静默移位」）**：
         FIFO 的 `dout` 是**寄存器驱动的 `aread`** → skew 消除后 **pop 拍上的读数据
         移位一拍**（实测：改前 Cycle3=`0x1`/Cycle4=`0x2`；改后 Cycle3=`0x2`/Cycle4=`0x1`）。
         **这不是设计回归**（改后才与 Verilog 的 `assign dout = mem[rd_ptr]` 一致），
         但**当前全仓没有任何断言能发现它**：
         - `tests/test_fifo_example.cpp` 全是 `REQUIRE(x.impl() != nullptr)` 结构检查；
         - `samples/fifo_example.cpp` / `samples/fifo_bundle.cpp` 只打印不校验，
           且**不在 ctest、不在 CI**；
         - `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`（ctest `spinalhdl_fifo`）
           也只查 `empty`。
         ⇒ MUST 在 `tests/test_fifo_example.cpp`（或新文件）补一条**行为**断言：
         写入 2 个元素 → 依次 pop → 断言**在 pop 沿之前**采样到的 `dout` 依次为
         第一个、第二个元素（采样时机 MUST 与 FIFO 协议一致：**在 pop 生效前**取值）。
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_mem_comb_read test_fifo_example
    ./build/tests/test_mem_comb_read "*aread*"
    ./build/tests/test_fifo_example
    ```
    MUST 观察到**红**（RED 基线）：
    - 用例 1（寄存器驱动 0-cycle）→ **RED**（拿到旧地址的值）
    - 用例 3（竞态）→ **RED**（返回写前值）
    - 用例 2（sread 护栏）→ 绿（改前就绿，Task 3 后 MUST 仍绿）
    - 用例 4（FIFO 行为）→ **RED**（当前无此断言 / 或按旧时序断言）

- [x] ### Task 3: Simulator 侧 —— async read port 归组合 + 数据流边
  - **action**:
    - `src/simulator_init.cpp`：`type_mem_read_port` 的 **async** 子类归入
      `combinational_instr_list_`（sync 保持 `sequential_instr_list_`）
    - `include/ast/mem_port_impl.h` + `src/ast/mem_port_impl.cpp`：
      建立 `read_port → data_proxy` 的 `src` 边（A′.1）；读端口指令写**自身**的
      `data_map` 槽位，由代理的 `instr_proxy` 搬运到代理槽位
    - ⚠️ **sync 路径也必须改为写自身槽位**（2026-10-21 增补，实测决定）：
      若只改 async 而不改 sync，sync 端口仍写代理槽位、而代理的
      `instr_proxy` 已改为从 sync 端口自身槽位取源 ⇒ **sync 读到 0**
      （实测 `test_mem_timing` / `test_mem_connection` / `test_memory` 共 3 个
      套件转红）。async + sync **两处都要改**写自身槽位。
      ⚠️ **对 `sread` 的观测语义无影响**：sync 端口仍在时序表（沿上求值），
      代理在**沿后**的组合遍搬运 → tick 结束时的观测值与改前**逐位一致**
      （探针实测：`sread` 改前改后均为 0xA1 / 0xE5 / 0xD4）→ Non-Goal #1 成立。
    - ℹ️ `data_map` 中读端口节点自身的槽位**已存在且宽度正确**，**无需新增工作**
      （`src/simulator_init.cpp:41-75` 给每个节点分配槽位；`ch_mem::aread/sread`
      传的 size 就是 `data_width`）→ 实施时只需**确认**。
  - **verify**:
    ```bash
    ./build/tests/test_mem_comb_read "*aread*"
    ./build/tests/test_mem_timing
    ./build/tests/test_mem_connection
    ./build/tests/test_memory
    ```
    Task 2 的用例 1 MUST 转绿；用例 2（sread 护栏）MUST 仍绿；
    且上述 3 个内存语义套件 MUST 保持全绿（**async+sync 同步改**的回归证据）

- [x] ### Task 4: codegen 侧 —— 让数据流边不破坏 Verilog 生成
  - **action**:
    - 按 `proposal.md` D1 的 **A′** 实现（决策已定稿，MUST NOT 临场改方案）：
      - **A′.2** `print_proxy` MUST 跳过「其 `src(0)` 是 mem read port」的代理
        （否则 `assign proxy = <read_port>` 与 `print_mem_read_port` 的
        `assign proxy = mem[addr]` **双驱动**）
      - **A′.3** `print_output` 的代理链遍历 MUST **停在 mem read port 之前**
        （`src/codegen_verilog.cpp:572-579`）—— 把 data proxy 本身当作 Verilog 侧的
        driver。⚠️ **漏做这一步会生成非法 Verilog**：
        `assign top_unnamed_output_1 = top_async_port;` 引用了**未声明信号**
        （读端口在 Verilog 里是虚构节点）→ Verilator 编译失败且报错与真因相距甚远。
        实测（只做 A′.1+A′.2）的产物见 `proposal.md` D1「A′.3 的坑」。
      - **A′.4** `type_mem_read_port` **MUST NOT** 加入 `print_header` 的 wire
        声明列表 —— A′.3 修好后读端口不会被任何 `assign` 引用 → 声明它只会
        产生**声明但无驱动**的 `logic`（违反 ZERO-DEBT POLICY 的 "No dead code"）。
        ⚠️ 原任务描述里的「`print_header` 的 wires 收集加入 `type_mem_read_port`」
        **是错的**，MUST NOT 照做。
      - `print_mem_read_port` 保持 `async_read` → `assign`、
        `sync_read` → `always_ff` 的分派不变
      - `print_input`（`:620-626`）走的是「只穿未命名代理」的分支，mem 的 data
        proxy **恒为具名** → 当前不受影响；MUST 仍加同样的护栏以防未来出现
        未命名的情形。
    - ⚠️ 这正是原实验「5 个测试转红」的根因 → 本 Task MUST 与 Task 3 同批验证
  - **verify**:
    ```bash
    ./build/tests/test_mem_comb_read "*codegen*"
    cd build && ctest -R "test_cache_sync|test_branch_predictor_v2|test_verilator_memory_e2e|test_verilator_complex_e2e|spinalhdl_branch_predictor_v2" --output-on-failure
    ```
    MUST 全绿（原实验下这 5 个转红）
    ⚠️ **额外 MUST 人工检查生成的 Verilog**（断言「无未声明信号引用」不是
    现有测试能覆盖的）—— 例如生成 `tests/test_mem_comb_read.cpp` 用的 fixture 的
    Verilog，grep `assign` 行，确认**每个被赋值的信号都在 wire/logic 声明列表里**：
    ```bash
    # 期望：assign 右侧的每个标识符都能在声明列表中找到
    grep -E "^\s+(logic|output|input)" gen.v | grep -oE "[A-Za-z_][A-Za-z0-9_]*" | sort -u > /tmp/decl.txt
    grep -E "^\s*assign" gen.v | grep -oE "=\s*[A-Za-z_][A-Za-z0-9_]*" | awk '{print $2}' | sort -u > /tmp/refs.txt
    comm -13 /tmp/decl.txt /tmp/refs.txt   # MUST 为空
    ```

- [x] ### Task 5: 文档同步
  - **action**:
    - `docs/simulation/ARCHITECTURE.md`「Memory read port timing」小节：
      删掉「register-driven read address skew」，改写竞态语义；
      **行号引用改为函数名**（改完行号会漂）
    - 根 `AGENTS.md` CONVENTIONS 的「Mem read port」条目：同步
    - 根 `AGENTS.md`「已消除的过时信息」段中 2026-10-21「撤回 mem 读端口时序实验」
      那条：追加「2026-10-21 以本 change 正式立项并实施」
    - `src/codegen_verilog.cpp::print_mem_read_port` 的大段注释：同步
    - `openspec/specs/mem-read-timing/spec.md` 的 `## Purpose`：
      现文仍写 "the ChipForge dmem design tolerates via a downstream register" → 改写
  - **verify**:
    ```bash
    grep -n "Mem read port" AGENTS.md
    grep -n "Memory read port timing" docs/simulation/ARCHITECTURE.md
    grep -n "## Purpose" openspec/specs/mem-read-timing/spec.md
    ```
    MUST 命中且内容已更新

- [x] ### Task 6: 全量回归（**不允许抽样**）
  - **action**:
    - 跑下列**全部**套件（本 change 改的是 Simulator 时序语义）：
      ```bash
      cd build && ctest -LE perf -j4 --output-on-failure
      cd build && ctest -L perf --output-on-failure      # ~15-20 min
      ./run_all_ported_tests.sh                          # 28 examples
      ./build/samples/fifo_example                       # ⚠️ 不在 ctest，必须手工跑
      ./build/samples/fifo_bundle                        # ⚠️ 同上
      ```
    - 重点观察（时序敏感）：`test_mem_comb_read` / `test_cache_sync` /
      `test_cache_complete` / `test_branch_predictor_v2` /
      `test_rv32i_pipeline` / `test_verilator_memory_e2e` / `test_verilator_complex_e2e`
    - ⚠️ **`test_fifo_example` 不是有效护栏**：该文件**全是结构检查**
      （`REQUIRE(x.impl() != nullptr)`），**无任何行为断言** → FIFO 读数据移位
      它发现不了。真正的 FIFO 行为护栏是 Task 2 新增的用例 4。
    - ⚠️ **FIFO sample 的打印值会变**（`dout` 在 pop 拍移位一拍：
      改前 Cycle3=`0x1`/Cycle4=`0x2` → 改后 Cycle3=`0x2`/Cycle4=`0x1`）。
      这是**向 Verilog 对齐**的结果（Verilog 的 `assign dout = mem[rd_ptr]` 中
      `rd_ptr` 在 pop 沿前进），**不是回归** → 需人工确认并在 Task 5 的文档里
      说明新的采样时机。
    - ⚠️ **双后端 parity 只会变好不会变坏**（Verilog 侧本就是 0-cycle）——
      若某个 dual-backend 用例转红，先怀疑 interpreter 侧的新语义
  - **verify**: 上述命令 MUST 全绿（0 failed）

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
