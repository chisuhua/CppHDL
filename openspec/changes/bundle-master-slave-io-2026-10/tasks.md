## Tasks

> **状态：草案（尚未实施）。**
> 立项自 2026-10-21 对 `samples/fifo_bundle` 既有失败的根因审计。
> 审计把它拆成**一条缺陷链**（见 `proposal.md` 的实测表），**根因是 D0**
> （`as_master()`/`as_slave()` 把字段重绑定成零字面量总线的**位切片** ⇒
> 设计只读不写的字段**恒为 0**），D1-D6 都是它的下游症状。
> 该路径**全仓零回归防护**（`fifo_bundle` / `config_bundle` / `interrupt_bundle`
> 从未做过真实的 master/slave 连接）⇒ 本 change 同时补上护栏。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8。
> ⚠️ **Task 2（D0）MUST 先于 Task 4（D3/D4）** —— 否则 `operator<<=` 的
> `set_src(0, ...)` 会打在**切片节点**上，`offset > 0` 的字段语义错误
> （「取对方 1-bit 节点的第 2 位」），`offset == 0` 的单字段 bundle 会**假绿**
> ⇒ RED→GREEN 的分步验证被污染。
>
> ⚠️ **TDD 纪律**：Task 1 先写会失败的用例（RED），Task 2-5 逐条转绿，
> Task 6-8 回归 + archive。每个 Task 的 verify 必须是**可执行命令**。

- [ ] ### Task 0: 探针基线（把「哪一环坏了」量化）
  - **action**:
    - 建两个**独立**探针（避免把「slave 自身没驱动」与「连接没传过去」混在一起）：
      1. **独立 slave 探针**：`ch_device<FiFo>` + `sim.get_bundle_value(dev.instance().io)`
         → 只看 slave 自身的字段值
      2. **master/slave 探针**：顶层 `__io(ch_out<ch_bool> empty_probe; ...)` 把 slave 的
         bundle 字段**引到顶层端口**观测
    - **第三个探针（决定性）**：「只读不写字段」探针 —— 把 slave 的 `push`
      （设计只读）引到顶层端口，TB 驱动 master 的 `push=1`
      ⇒ **当前 slave 侧 MUST 读到 0**（这就是 D0 的证据）
    - ⚠️ **MUST NOT** 保存 `describe()` 里局部 `ch_module` 的指针（局部变量 → 悬垂
      → 读出垃圾 node id，2026-10-21 的探针即栽在这里）
    - 记录三支探针各自的基线读数
  - **verify**:
    ```bash
    g++ -std=c++23 -w -Iinclude -Iinclude/core -Iinclude/ast -Iinclude/abstract \
        -Iinclude/utils -Iexternal/inipp probe.cpp build/libcpphdl.a \
        -L/usr/lib/llvm-18/lib -lLLVM-18 -o probe && ./probe
    ```
    MUST 记录下三支探针的读数（当前预期：只读字段探针读到 **0**）

- [ ] ### Task 1: 写会失败的用例（RED）
  - **action**:
    - 新建 `tests/test_bundle_master_slave.cpp`：
      - DUT：一个 bundle↔bundle 的 FIFO（master bundle ↔ slave bundle）
      - 用例 1：TB 驱动 master 的 `push` + 写数据 → **slave 侧**的 FIFO MUST 真的写入
        （用 slave 的 `empty` 翻转来观察）
      - 用例 2：slave 产生的 `empty` / `full` → **master 侧** MUST 观测到（初始 `empty=1`）
      - 用例 3：**只读不写字段**（slave 的 `push`）MUST 读到 TB 驱动的值
        ⇒ 专门暴露 D0
    - ⚠️ **DUT MUST 用多字段 bundle**（`fifo_bundle` 即可），
      **MUST 含至少一个 `offset > 0` 的字段**（`push` 在 offset 2），
      **MUST 含至少一个「设计只读不写」的字段**
      ⇒ 否则 D0 与「切片覆写」缺陷都测不出来（`SimpleBundle` 单字段 offset=0 会**假绿**）
    - ⚠️ **MUST 覆盖两个传播方向**（只测一个方向会漏掉一半的缺陷链）
    - 该用例在实现修复前 MUST **全红**
    - ⚠️ **非空转 MUST 用变异验证证明**（见 `proposal.md` D8）：
      逐项临时改坏 D0/D1/D2/D3/D4/D5，每次确认对应用例转红，然后**逐一还原**
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_master_slave
    ./build/tests/test_bundle_master_slave
    ```
    MUST 观察到**红**（且 3 个用例都红）

- [ ] ### Task 2: 字段节点身份稳定（D0 / 决策 D7）—— **MUST 最先**
  - **action**:
    - `include/core/bundle/bundle_base.h`：
      - `set_role()`（`:282-297`）MUST NOT 调用 `create_field_slices_from_node()`
        去**重绑定**字段节点
      - `create_field_slices_from_node()`（`:301-345`）要么删除，要么改成
        **由字段派生总线**（方向：字段 → 总线），MUST NOT 由总线切片得到字段
      - `as_master()` / `as_slave()` 的职责 MUST 限定为**设置方向**（`dir_`）
    - ⚠️ 总线视图（`bundle_base::operator=` 拷贝的那个 `node_impl_`）MUST 仍有定义：
      若无人使用则可留空，MUST NOT 让字段依赖它
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ```
    用例 3（只读字段）MUST 转绿；用例 1/2 仍红

- [ ] ### Task 3: 修 `Simulator` 的 bundle 字段读写（D1 / D2）
  - **action**:
    - `include/simulator.h`：
      - `get_bundle_field_value()`：非端口字段 MUST 读该字段节点的 `data_map` 槽位
        （MUST NOT 用 `static_cast<uint64_t>(field)` 的常量转换）
      - `set_bundle_field_value()`：非端口字段 MUST **写**该字段节点的 `data_map` 槽位
        （MUST NOT 用 `field = ch_bool(v)` 的节点重绑定）
      - 读写 MUST 共用同一套槽位寻址逻辑
    - ℹ️ 端口类型字段的分支 MUST 保持不变（`get_port_value` / `set_port_value`）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ```
    用例 1（TB → slave）MUST 转绿；用例 2 仍红

- [ ] ### Task 4: 修 bundle 字段连接 + `connect()`（D3 / D4）
  - **action**:
    - `include/core/bundle/bundle_base.h`：
      - `connect_field_based_on_direction_and_source()` 的**两个方向分支** MUST 用
        `operator<<=`（数据流边）替代 `operator=`（节点重绑定）
      - **第三分支**（`:163-164`，方向相同 / 无方向 → `left = right` 同样重绑定）
        MUST 一并改为 `left <<= right`
      - `connect()`（`:360-367`）MUST 逐字段复用该方向感知逻辑，
        MUST NOT 无条件 `dst.field = src.field`
    - ⚠️ `tests/test_bundle_connection.cpp` 的 **6 处**断言编码了旧的
      「重绑定 / 节点共享」语义，MUST 一并改写：
      `:375`、`:448-449`、`:485-487`、`:544-546`（断言节点**共享**）、
      `:452-454`、`:490-491`（断言「重绑定后跟随源」）
      ⇒ 改成表达其**注释里声明的意图**（dst 节点独立、由 src 驱动），
      且**用「值能传播」而不是「指针相等」来断言**
    - ℹ️ **其它 bundle 套件 MUST 零行为变化**；⚠️ 注意「端口型 bundle」这一类别
      **并不存在**（全仓 bundle 的字段全是 `ch_bool`/`ch_uint<N>` 成员）——
      爆炸半径是全部普通信号字段 bundle；只是它们的使用点极少
    - ℹ️ `ch_stream` 走**自由** `operator<<=`（`include/chlib/stream_operators.h:25-33`）
      ⇒ 天然不经 bundle_base 的连接路径 ⇒ D3/D4 对它 no-op
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ./build/tests/test_bundle_connection
    ./build/tests/test_bundle
    ./build/tests/test_bundle_advanced
    ./build/tests/test_axi_lite_bundle
    ./build/tests/test_ch_stream          # 证明 ch_stream 走自由运算符、不受影响
    ./build/tests/test_bundle_bitstream   # 嵌套 bundle（ch_fragment / ch_flow）覆盖
    ```
    Task 1 的用例 2 MUST 转绿；其余 bundle / stream 套件 MUST 保持全绿

- [ ] ### Task 5: `fifo_bundle` 方向镜像（D5）
  - **action**:
    - `include/bundle/common_bundles.h`：`as_master_direction()` / `as_slave_direction()`
      MUST 与 `config_bundle` / `interrupt_bundle` 同构（严格镜像）：
      `data`/`push`/`pop` = master OUT / slave IN；`full`/`empty` = master IN / slave OUT
    - ⚠️ 该方向变更对下游是**行为变更** —— 但方向此前**根本没生效**
      （普通信号字段的方向虽被记录，但连接路径从未用过它）⇒ 下游不可能依赖它
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    grep -n "make_output\|make_input" include/bundle/common_bundles.h
    ```
    三个 bundle 的方向 MUST 呈镜像关系

- [ ] ### Task 6: 顶层改用普通端口（D6，裁决方案 c）
  - **action**:
    - `samples/fifo_bundle.cpp` / `samples/minimal_fifo_bundle.cpp`：
      顶层 MUST 改用普通 `ch_in`/`ch_out` 端口承载 TB 接口，
      bundle 只用于**模块间**连接
    - 理由见 `proposal.md` D6：根 `AGENTS.md` CONVENTIONS 已明文裁决
      「Bundle as IO: Use individual `ch_bool`/`ch_uint` ports, NOT bundle struct」；
      且 TB 驱动语义需要 `inputimpl`/`outputimpl`（不被求值覆写）
    - ⚠️ **D0 MUST 已修**（否则顶层端口驱动 bundle 字段依然无效 —— 字段仍是切片）
    - ⚠️ `data` 回读的语义 MUST 明确标注：当前 `data` 是**写**数据路径，
      **读数据无通路**（Non-Goal #1）⇒ sample 打印的 `data=` 是 TB 自己的写值，
      **不是** FIFO 的读数据 ⇒ MUST NOT 继续误导
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ./build/samples/fifo_bundle        # 初始状态检查 MUST 通过（当前 exit=1）
    ./build/samples/minimal_fifo_bundle
    ```
    ⚠️ 两个 sample **不在 ctest / CI** ⇒ MUST 手工跑，且 MUST 有 Task 1 的
    Catch2 用例兜底

- [ ] ### Task 7: 文档同步 + 计数同步
  - **action**:
    - 根 `AGENTS.md`：`CTEST_TOTAL` 181 → **182**；`tests/test_*.cpp` 111 → **112**；
      「已消除的过时信息」新增本 change 条目
    - `include/bundle/common_bundles.h`：三个 bundle 的方向语义加注释
      （说明「master/slave 是同一条总线两端，方向必须镜像」）
    - `include/core/bundle/bundle_base.h`：`set_role` / 字段连接的注释同步
      （说明「字段是一等节点；`operator=` 是软件拷贝语义，不是连接语义」）
    - 根 `AGENTS.md` CONVENTIONS 的「Bundle as IO」条目：确认与本 change 的 D6 裁决一致
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh
    cd build && ctest -N | tail -1 && ctest -N -LE perf | tail -1
    # Requirement ↔ 测试 映射检查：每条 Requirement MUST 至少有一个断言命中
    grep -c "TEST_CASE" tests/test_bundle_master_slave.cpp
    ```
    ⚠️ 实施者 MUST 逐条核对 `specs/bundle-master-slave-io/spec.md` 的每条
    Requirement / Scenario 都有对应的 Catch2 断言（无对应断言的条款 = 空转条款）

- [ ] ### Task 8: 全量回归 + archive
  - **action**:
    - `ctest -LE perf`（新增 1 个 target → 期望 171/171 全绿）
    - `./run_all_ported_tests.sh`
    - `./build/samples/fifo_bundle`（MUST 退出 0）
    - `./build/samples/minimal_fifo_bundle`
    - `openspec archive bundle-master-slave-io-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
  - **verify**:
    ```bash
    cd build && ctest -LE perf -j4 --output-on-failure
    ./run_all_ported_tests.sh
    openspec validate --specs --strict
    grep -rn "bundle-master-slave-io-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
