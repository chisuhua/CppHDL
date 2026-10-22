# Change: disabled-samples-remediation-2026-10 — 修复 14 个**禁用** sample 的编译

> **状态：已实施（2026-10-22）。**
> 立项自 `samples-regression-guard-2026-10` 的 Non-Goal 5。
> ⚠️ 本 change 的每个 Task 都以「**该组的 sample 已登记进
> `CH_SAMPLE_TARGETS` 且已从禁用清单移除**」为完成判据。
> ⚠️ **2026-10-22 实测**：14 个 sample 的**全部** error 数合计 **229**
> （不是「14 个问题」）⇒ 排期 MUST 参考 proposal 的 D1 分组顺序。
>
> **落地结果：9 个已修并注册（清单 14 → 5）；5 个逐个定性后留在清单**
> （各自被一个**具体的框架层缺口**挡住 ⇒ 归位条件已指向那个缺口）。

- [x] ### Task 0: 复核基线（14 个 sample 的**全部**编译错误）
  - **action**:
    - 逐个重跑编译探测，确认根因**未变**（自 2026-10-22 登记以来框架可能已变）：
      ```bash
      FLAGS=$(grep -m1 "CXX_FLAGS" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_FLAGS = //')
      INCS=$(grep -m1 "CXX_INCLUDES" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_INCLUDES = //')
      # 清单来自禁用清单（名字在第 1 列）
      for f in $(cut -f1 samples/disabled_samples.txt | grep -v '^#'); do
          n=$(g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" 2>&1 | grep -c "error:")
          first=$(g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" 2>&1 | grep -m1 "error:" | cut -c1-90)
          printf "%-42s errors=%-3s %s\n" "$f" "$n" "$first"
      done
      ```
    - ⚠️ MUST 数**全部** error（`grep -c "error:"`），**MUST NOT** 只看首个
      （`grep -m1`）—— 首个 error 一致 **≠** 根因分类一致
      （`spinalhdl_component_function_area_example` 首个是「基类名不认识」，
      实则 **98** 个 error ⇒ 那个分类几乎肯定不成立）
    - ⚠️ 若根因**已变**（框架已修）⇒ 该 sample MAY 直接编译通过 ⇒ 走 Task 2
      的「登记」路径（不需改代码）
  - **verify**:
    ```bash
    # ⚠️ 禁用清单的行数 MUST 排除注释行（实测：`grep -c .` = 35，含注释与分组行）
    grep -v '^#' samples/disabled_samples.txt | grep -c .   # 14（落地前）
    ```

- [x] ### Task 1: 组 A — 十六进制字面量后缀（5 个）✅ **5/5 已修**
  - **action**:
    - `stream_arbiter_example` / `stream_demux_example` / `stream_flow_basic_example` /
      `stream_fork_example` / `stream_join_example`：
      **十六进制**字面量的 `_d` 后缀 → `_h`（`0xAB_d` → `0xAB_h`）
    - ⚠️ **MUST NOT** 改成裸 `0xAB`（会退化成 `int`、宽度 32 ⇒ 信号宽度与
      打印输出可能改变 ⇒ 「绿 ≠ 对」）；`0xAB_h` 实测 value=171 / width=8 ✓
    - ⚠️ **MUST NOT** 改十进制字面量（`stream_demux_example:23,45` 的 `1_d` /
      `0_d` —— `_d` 本来就正确）
    - ⚠️ **优先「按 `examples/stream/*_demo.cpp` 改造」**（近乎同源 ⇒ 成本最低）
      ⇒ 每个 sample MUST 显式回答「修 / 删 / 留」三者之一（proposal D1）
    - ⚠️ 改完 MUST 逐个跑护栏（**运行**回归可能暴露新的失败）
  - **落地补充（实测发现的 4 个约束，已写进各 sample 的头注释）**：
    ① **所有节点 MUST 在 `ch::Simulator` 构造之前创建** —— Simulator 在构造时
       建立 data_map_，之后新建的节点不在其中 ⇒ `get_value()` 读到
       "Value not found" 警告；
    ② **TB 侧要驱动的信号用 `ch_uint<N>` 成员**（`set_input_value()` 只有
       `ch_in<T>` 与 `ch_uint<N>` 两个重载，**没有 `ch_bool`**）；
    ③ chlib 的组合子对 stream 字段做**值**运算 ⇒ 字段 MUST 用 `=`
       （节点重绑定）接到值节点上，**MUST NOT** 用 `<<=`（那是 proxy 节点 ⇒
       "Invalid operands for binary operation"）；
    ④ chlib 的结果对象里**有些字段根本没被驱动**（`stream_fork` 的输出 ready、
       `stream_join` 的输出 ready、`stream_demux` 的输出 ready）⇒ 读它们会
       CHABORT ⇒ sample 只读**被驱动**的字段。
  - **verify**:
    ```bash
    # ⚠️ verify 块 MUST 自带 FLAGS/INCS（Task 0 的定义在另一个代码块里 ⇒
    # 单独执行本块时两者为空 ⇒ 编译必然失败）
    FLAGS=$(grep -m1 "CXX_FLAGS" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_FLAGS = //')
    INCS=$(grep -m1 "CXX_INCLUDES" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_INCLUDES = //')
    for f in stream_arbiter_example stream_demux_example stream_flow_basic_example \
             stream_fork_example stream_join_example; do
        g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" -o /dev/null || { echo "FAIL $f"; exit 1; }
    done
    # ⚠️ 循环 MUST 以非零退出码收尾（原写法以 `echo` 收尾 ⇒ 恒 exit 0 ⇒
    # 这个 verify 永远不可能失败）
    ```

- [x] ### Task 2: 登记机制（每修好一个就跑）
  - **action**:
    - ① 补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`
    - ② 从 `samples/disabled_samples.txt` 移除该行
    - ③ 跑护栏
    - ④ ⚠️ **同步两处嵌了计数的头注释**（每移出一行就漂移）：
      - `samples/CMakeLists.txt:6`（「未注册的 sample 见 samples/disabled_samples.txt（**N 个**，2026-10-22 实测）」）
      - `samples/disabled_samples.txt:15`（「已注册 **N** / 源文件 40 / 本清单 **M**」）
    - ⚠️ 若采纳了「删源文件」的归宿 ⇒ 源文件总数也变（40 → 39）⇒ 根 `AGENTS.md`
      的「40 sample programs」条目同样要同步
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    # C1/C2 守恒 MUST 仍绿
    # 头注释里的计数 MUST 与实际一致
    grep -o "本清单 [0-9]*" samples/disabled_samples.txt
    grep -o "（[0-9]* 个" samples/CMakeLists.txt
    ```

- [x] ### Task 3: 组 C / D / E（5 个）✅ **5/5 已修**
  - **action**:
    - **组 C**：`stream_fifo_example` —— 改到当前 `stream_fifo<T,D>(in)` 签名；
      组件端口按仓库约定改用**独立端口**（bundle 成员不会成为组件 IO）；
      ⚠️ 头部如实记录「`stream_fifo` 的读使能硬编码 ⇒ `full` 不可达 ⇒
      反压场景当前无法演示」
    - **组 D1**：`spinalhdl_stream_example` —— `chlib/fragment.h` → `bundle/fragment.h`；
      `as_master() override` → `as_master_direction()`；
      `Stream`/`Flow`/`Fragment` → `ch_stream`/`ch_flow`/`ch_fragment`；
      ⚠️ 并把旧版「只构造对象、**从不调用 `process()`**」的空壳改成**真调用**
      + 行为断言
    - **组 E**：`advanced_pod_bundle_serialization` / `large_pod_bundle_example`
      —— `CH_BUNDLE_FIELDS(Self, …)` → `CH_BUNDLE_FIELDS_T(…)`（ADR-043）；
      `set_bundle_field_value()` 是 **private** ⇒ 改用 `ch_uint` 驱动成员 +
      `set_input_value()`；读回用 `get_bundle_field_value()`；
      ⚠️ 并修正了 POD 的**位偏移**：POD 是按**字节 memcpy** 进 uint64 数组的
      （不是位紧凑打包）⇒ 旧 sample 用的 0/64/96/112/120 连续位偏移是错的 ⇒
      part2 起全部读回 0
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    ```

- [x] ### Task 4: 组 B（`stream_handshake_example`）⛔ **留在清单**
  - **action**:
    - ⚠️ 它依赖 `chlib::stream_fifo` 的**背压语义**（FIFO 满 ⇒ 上游暂停），
      而当前 `stream_fifo` 的**读使能硬编码为 `pop_ready = true`** ⇒
      消费端恒就绪 ⇒ FIFO 永远在排空 ⇒ **`full` 不可达** ⇒ 该 sample 的
      全部价值**无法演示** ⇒ 改完会变成「跑得起来但什么都不演示」的 sample
      （「绿 ≠ 对」）
    - ⇒ 留在禁用清单，归位条件写成「依赖『`chlib::stream_fifo` 读使能
      外部化』这一**公开 API 变更**（需 ADR）」
  - **verify**:
    ```bash
    grep -c "stream_handshake_example" samples/disabled_samples.txt   # MUST == 1
    ```

- [x] ### Task 5: 组 D2（`conditional_demo`）⛔ **留在清单**
  - **action**:
    - ⚠️ 当前语句块 DSL 是 `include/chlib/if_stmt.h` 的 `_if(cond).then(…).elif(…).else_().end()`，
      而其中 `conditional_reg_assignment::next_proxy::operator=` **在条件块内是
      空实现**（该头文件 :160-166 的注释：「实际实现需要更复杂的机制」）
      ⇒ 把 `reg_count->next = ...` 写进分支里会**静默不生效** ⇒ 改完能编译
      但计数器**永远不计数**
    - ⇒ 留在禁用清单，归位条件写成「依赖『`_if` 条件块内的寄存器赋值捕获』实现」
  - **verify**:
    ```bash
    grep -c "conditional_demo" samples/disabled_samples.txt   # MUST == 1
    ```

- [x] ### Task 6: 组 F（`spinalhdl_component_function_area_example`，**98 error**）⛔ **留在清单**
  - **action**:
    - ⚠️ 它**不是**「组件 API 改名」⇒ **接近重写**（98 个 error，占全组 43%）
      ⇒ 独立立项
    - ⇒ 留在禁用清单，归位条件写成「依赖『spinalhdl_component_function_area_example
      重写』这一**独立立项**」
  - **verify**:
    ```bash
    grep -c "spinalhdl_component_function_area_example" samples/disabled_samples.txt   # MUST == 1
    ```

- [x] ### Task 7: 组 G（`if_demo`）⛔ **留在清单**
  - **action**:
    - 需要**框架层新能力**（`ch_in<ch_bool>` 绑到 `const ch_bool&`）
      ⇒ 留在禁用清单，把归位条件写成「依赖 <具体框架变更>」（D3）
    - ⚠️ **期望值 MUST 钉死为 1**（D3 明确「需要框架能力 ⇒ 留在清单」⇒ 归档时
      它**必然**还在清单里）
  - **verify**:
    ```bash
    # ⚠️ 期望值 MUST 是 1 —— 原写法「0 或 1」= 两种结果都算通过 = 零判别力
    grep -c "^if_demo" samples/disabled_samples.txt   # MUST == 1
    ```

- [x] ### Task 8: chlib 缺陷修复 + 行为级护栏（proposal D5）
  - **action**:
    - 修 `include/chlib/stream.h` 的 3 处「先读后写」空操作数缺陷
      （`stream_fifo` / `stream_fork` / `stream_arbiter_round_robin` +
      `stream_arbiter_priority`）—— 详见 proposal D5 的表
    - `tests/chlib/test_stream.cpp` 新增**行为级**用例
      （「Stream: chlib stream 组合子必须**行为正确**（不只是结构正确）」）
      ⇒ 把 3 处缺陷钉住
  - **verify**:
    ```bash
    ./build/tests/test_stream
    # 变异验证：把 stream_fifo 的读使能改回 `pop_ready & result.pop_stream.valid`
    #           ⇒ 该用例 MUST 转红
    ```

- [x] ### Task 9: 全量回归 + 文档 + archive
  - **action**:
    - `bash scripts/run_all_samples.sh` ⇒ MUST 全绿（**无 stale 豁免**）
    - `bash scripts/verify_test_counts.sh`（**CTEST_TOTAL 不变**）
    - 根 `AGENTS.md` 的「40 sample programs」条目同步（26 已注册 → **35 已注册**，
      14 已禁用 → **5 已禁用**）
    - ⚠️ **禁用清单最终长度 MUST 作为验收**（**5**，且每项的归位条件指向一个
      **具体**的框架缺口）
    - `openspec archive disabled-samples-remediation-2026-10 -y`
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    # ⚠️ 不得用 `openspec validate --specs --strict` 当验收 ——
    # 它在 pristine HEAD 上就是红的（CLI 1.14.3 实测：3 passed, 15 failed）⇒ 不具判别力
    openspec validate disabled-samples-remediation-2026-10 --strict
    openspec validate sample-regression-guard --type spec --strict
    grep -v '^#' samples/disabled_samples.txt | grep -c .   # MUST == 5
    grep -rn "disabled-samples-remediation-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
