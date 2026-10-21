## Tasks

> **状态：草案（尚未实施）。**
> 立项自 2026-10-21 对 `samples/fifo_bundle` 既有失败的根因审计。
> 审计把它拆成**一条缺陷链**（见 `proposal.md` 的实测表），**根因是 D0**
> （`as_master()`/`as_slave()` 把字段重绑定成零字面量总线的**位切片** ⇒
> 设计只读不写的字段**恒为 0**），D1-D6 都是它的下游症状。
> 该路径**全仓零回归防护**（`fifo_bundle` / `config_bundle` / `interrupt_bundle`
> 从未做过真实的 master/slave 连接）⇒ 本 change 同时补上护栏。
>
> ⚠️ **2026-10-21 二次复审（oracle + metis）后的关键增补**：
> ① **D0 反转了 ADR-014 的既定决策**（切片模型在该 ADR 的「Q1 修复的前置条件」
>    第 4 条被列为**确认项**）⇒ MUST 新增/更新 ADR（Task 7）；
> ② **D0 缺「字段节点从哪来」的决策** —— 字段默认构造即 `nullptr`
>    （`include/lnode/logic_buffer.tpp:13`），今天能拿到节点**全靠**切片机制
>    ⇒ **Task 2 MUST 先做一个 spike** 验证「独立未驱动节点」的可行性；
> ③ `fifo_bundle` 的真实缺陷是 **`push` 的方向指认错误**（其余 4 个字段已镜像）；
> ④ D6 之后 `set_bundle_value()` 只对**未连接**字段有效 ⇒ 护栏需要**第二个 DUT**；
> ⑤ `deserialize_bits_to_fields()` 也在切片重绑定 ⇒ R1 的覆盖面 MUST 与修法一致。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9。
> ⚠️ **Task 2（D0）MUST 先于 Task 4（D3/D4）** —— 否则 `operator<<=` 的
> `set_src(0, ...)` 会打在**切片节点**上，`offset > 0` 的字段语义错误
> （「取对方 1-bit 节点的第 2 位」），`offset == 0` 的单字段 bundle 会**假绿**
> ⇒ RED→GREEN 的分步验证被污染。

- [x] ### Task 0: 探针基线（**可选**，结论已折进 `proposal.md`）
  - **action**:
    - 三支探针的结论**已在 `proposal.md` 的 D0 取证行里**（决定性探针：
      TB 驱动 master `push=1` 而 slave `io.push` 读到 0）
    - ⇒ 本 Task **降级为可选**：实施者若要用探针继续定位，MUST 复用下述配方
    - 探针配方（避免重复 2026-10-21 走过的弯路）：
      - 顶层用 `__io(ch_out<ch_bool> empty_probe; ...)` 把 slave 的 bundle 字段
        **引到顶层端口**观测
        —— ⚠️ **MUST NOT** 保存 `describe()` 里局部 `ch_module` 的指针
        （局部变量 → 悬垂 → 读出垃圾 node id）
      - 独立 slave 探针（`ch_device<FiFo>` + `sim.get_bundle_value(dev.instance().io)`）
        与 master/slave 探针**分开**跑，用于区分「slave 自身没驱动」与「连接没传过去」
  - **verify**: 无（可选任务）

- [x] ### Task 1: 写会失败的用例（RED）
  - **action**:
    - 新建 `tests/test_bundle_master_slave.cpp`，**两个 DUT**：
      1. **已连接的 master↔slave FIFO**（`connect(master, slave)` 或
         `master <<= slave` —— ⚠️ **MUST 用 bundle 级接线**，MUST NOT 逐字段手工 `<<=`
         ⇒ 否则 D4/D5 的变异不会让测试转红，D8 的「非空转」就是空话）：
         - 用例 1：TB 驱动 master 的 `push` + 写数据 → **slave 侧**的 FIFO MUST 真的写入
           （用 slave 的 `empty` 翻转来观察）
         - 用例 2：slave 产生的 `empty` / `full` → **master 侧** MUST 观测到（初始 `empty=1`）
         - 用例 3：**只读不写字段**（slave 的 `push`）MUST 读到 TB 驱动的值 ⇒ 专门暴露 D0
      2. **未连接的 standalone bundle**（覆盖 D1/D2 与 spec 的槽位读写契约）：
         - 用例 4：`set_bundle_value()` 写入 → `get_bundle_value()` MUST 读回同一值
    - ⚠️ DUT MUST 用多字段 bundle（`fifo_bundle` 即可），
      **MUST 含至少一个 `offset > 0` 的字段**（`push` 在 offset 2），
      **MUST 含至少一个「设计只读不写」**的字段
      ⇒ 否则 D0 与「切片覆写」缺陷都测不出来（`SimpleBundle` 单字段 offset=0 会**假绿**）
    - ⚠️ **MUST 覆盖两个传播方向**（只测一个方向会漏掉一半的缺陷链）
    - 该用例在实现修复前 MUST **全红**
    - ⚠️ **非空转 MUST 用变异验证证明**（见 `proposal.md` D8）：逐项临时改坏
      D0/D1/D2/D3/D4/D5，每次确认对应用例**转红**，然后**逐一还原**；
      **变异结果 MUST 记录在 commit message 或归档日志里**（否则不可复核）
    - ⚠️ **变异卫生**：每项变异**前** MUST 先 `git diff` 确认工作区干净（或先 `git stash`），
      否则任一次还原失败都会把无关改动带进最终 diff
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_master_slave
    ./build/tests/test_bundle_master_slave
    ```
    MUST 观察到**红**（且 4 个用例都红）

- [x] ### Task 2: 字段节点起源 + 节点身份稳定（D0 / 决策 D7）—— **MUST 最先**
  - **action**:
    - ⚠️ **本 Task MUST 先做一个 spike**（阻塞项）：验证「每个字段拥有一个
      **独立的、无 src 的占位节点**」是否可行：
      1. 该节点 MUST 能进 `context::get_eval_list()` 从而拿到 `data_map` 槽位
      2. 该节点 MUST 能被 `set_src(0, ...)` 后正常求值
      3. 未驱动时读出 MUST 是确定值
      ⇒ ⚠️ **MUST NOT** 用字面量节点（`litimpl` 的 src 不被求值，`set_src` 打上去无效）
      ⇒ spike 结论（**节点种类 + 创建时机**）MUST 写进本 change 的实施记录
    - `include/core/bundle/bundle_base.h`：
      - `set_role()`（`:282-297`）MUST NOT 调用 `create_field_slices_from_node()`
        去**重绑定**字段节点
      - 字段 MUST 在**创建时机**（spike 结论）获得自己的节点
      - `as_master()` / `as_slave()` 的职责 MUST 限定为**设置方向**（`dir_`）
    - ⚠️ **总线节点的处置 MUST 二选一并写明**（否则两种合理解读导致不兼容结果）：
      - **(推荐) 保留** `set_role()` 里的零字面量总线创建（满足既有
        `impl() != nullptr` 断言：`tests/test_bundle_connection.cpp:250-251, 441-442,
        475-476, 509-510`），但**字段 MUST NOT 依赖它**
      - 或**删除**总线视图（则上述 4+ 处断言 MUST 一并迁移）
    - ⚠️ **D0 的静默受害者 MUST 一并处理或显式排除**：
      `bundle_concat` / `bundle_slice_view`（`include/core/bundle/bundle_operations.h:14,58`，
      它们靠「拷贝共享总线 + 切片重派字段」工作）、
      `deserialize_bits_to_fields()`（`include/core/bundle/bundle_serialization.h:85-97`，
      同样切片重绑定）、
      `master()` / `slave()` 工厂（`include/core/bundle/bundle_utils.h:10-17`，
      按值拷贝穿过 `bundle_base::operator=`）、
      `flip()`（`bundle_base.h:181-188`，硬编码 `as_slave()` 且返回全新未连接节点）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ./build/tests/test_bundle_connection      # 断言迁移前 MUST 仍是绿的
    ```
    用例 3（只读字段）MUST 转绿；用例 1/2/4 仍红

- [x] ### Task 3: 修 `Simulator` 的 bundle 字段读写（D1 / D2）
  - **action**:
    - `include/simulator.h`：
      - `get_bundle_field_value()`：非端口字段 MUST 读该字段节点的 `data_map` 槽位
        （MUST NOT 用 `static_cast<uint64_t>(field)` 的常量转换）
      - `set_bundle_field_value()`：非端口字段 MUST **写**该字段节点的 `data_map` 槽位
        （MUST NOT 用 `field = ch_bool(v)` 的节点重绑定）
      - 读写 MUST 共用同一套槽位寻址逻辑
      - ⚠️ **位段提取的 UB 修复**：现 `(1ULL << width) - 1`（`simulator.h:531`）在
        `width == 64` 时是 `1ULL << 64` = **未定义行为** ⇒ MUST 改用
        `width >= 64 ? ~0ULL : ((1ULL << width) - 1)` 之类的无 UB 形式
      - ⚠️ **宽度 enforcement**：`get_bundle_width() > 64` 时 MUST **显式拒绝或告警**
        （MUST NOT 静默截断；实测 `axi_lite_bundle<32,32>` 宽 142 位）；
        单字段宽度 > 64 位时 `get_bundle_field_value` 的 `uint64_t` 返回同样 MUST
        显式拒绝或告警
    - ℹ️ 端口类型字段的分支 MUST 保持不变（`get_port_value` / `set_port_value`）
    - ℹ️ **适用面**：已连接的字段其值由设计每拍求值产生，TB 写入会被下一拍覆写
      ⇒ `set_bundle_value()` 的合法用途是**未连接**字段 ⇒ 由 Task 1 的第二个 DUT 覆盖
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave    # 用例 4 MUST 转绿
    ```
    用例 1（TB → slave）MUST 转绿；用例 2 仍红

- [x] ### Task 4: 修 bundle 字段连接 + `connect()`（D3 / D4）
  - **action**:
    - `include/core/bundle/bundle_base.h`：
      - `connect_field_based_on_direction_and_source()` 的**两个方向分支** MUST 用
        `operator<<=`（数据流边）替代 `operator=`（节点重绑定）
      - **第三分支**（`:163-164`，方向相同 / 无方向 → `left = right` 同样重绑定）
        MUST 一并改为 `left <<= right`
      - `connect()`（`:360-367`）MUST 逐字段复用该方向感知逻辑，
        MUST NOT 无条件 `dst.field = src.field`
      - ⚠️ **参数顺序 MUST 明确**：`connect(src, dst)` ≡
        `dst.connect_field_based_on_direction_and_source(dst.field, src.field)`
        （两种顺序在第三分支下结果相反）
    - ⚠️ **设计侧驱动字段 MUST 用 `<<=`**：`field = expr` 是重绑定 ⇒ 若它在
      connect **之后**执行，会把字段指向另一个节点 ⇒ **已建立的边仍指向被孤儿化的
      旧节点** ⇒ 读到 0。现状之所以能工作，只是因为 `ch_module` 的 `describe()`
      恰好先于 `connect()` 执行 —— **顺序巧合，不是语义保证**
      ⇒ `samples/fifo_bundle.cpp:81,84`（`io.empty = ...` / `io.full = ...`）
      与 `samples/minimal_fifo_bundle.cpp` 的同类赋值 MUST 改为 `<<=`
    - ⚠️ `tests/test_bundle_connection.cpp` 的断言迁移清单（**10 处**）：
      编码「节点共享 / 重绑定跟随」：`:375`、`:448-449`、`:485-487`、`:544-546`、
      `:452-454`、`:490-491`
      编码「节点非空」（D0 改变节点起源）：`:394-396`、`:419-420`、`:513-519`、`:573-582`
      ⇒ 改成表达其**注释里声明的意图**（dst 节点独立、由 src 驱动），
      且**用「值能传播」而不是「指针相等」来断言**
    - ℹ️ `ch_stream` 走**自由** `operator<<=`（`include/chlib/stream_operators.h:25-33`）
      ⇒ 天然不经 bundle_base 的连接路径 ⇒ D3/D4 对它 no-op
      （⚠️ 但它内部用的正是重绑定语义 ⇒ 见「遗留债务」）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ./build/tests/test_bundle_connection
    ./build/tests/test_bundle
    ./build/tests/test_bundle_advanced
    ./build/tests/test_bundle_serialization     # deserialize 走切片重绑定
    ./build/tests/test_tlm_bundle_phase1
    ./build/tests/test_axi_lite_bundle
    ./build/tests/test_ch_stream                # 证明 ch_stream 走自由运算符、不受影响
    ./build/tests/test_bundle_bitstream         # 嵌套 bundle（ch_fragment / ch_flow）
    ./build/tests/test_verilator_bundle_e2e     # 双后端：同方向 <<= 第三分支 + 字段重绑定赋值
    ./build/tests/test_verilator_trace_parity
    ```
    Task 1 的用例 1/2 MUST 转绿；其余 bundle / stream / verilator 套件 MUST 保持全绿

- [x] ### Task 5: `fifo_bundle` 方向指认修正（D5）
  - **action**:
    - `include/bundle/common_bundles.h`：`push` MUST 归到 **master OUT / slave IN**
      （今天它在 master IN / slave OUT —— **其余 4 个字段（`data`/`pop`/`full`/`empty`）
      已经是镜像的**，问题只是 `push` 放错边）
    - ⚠️ 该方向变更对下游是**行为变更** —— 但方向此前**根本没生效**
      （普通信号字段的方向虽被记录，连接路径从未用过它）⇒ 下游不可能依赖它
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave
    ```
    MUST 有一个**逐字段枚举并比对方向**的 Catch2 断言
    （MUST NOT 只靠 `grep make_output|make_input` —— 那种检查证明不了方向正确）

- [x] ### Task 6: 顶层改用普通端口（D6，裁决方案 c）
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
    ⚠️ 两个 sample **不在 ctest / CI** ⇒ MUST 手工跑
    ⚠️ **判据 MUST 不止「初始检查绿」**：sample MUST 能演示完整序列
    `push → empty=0 → pop → empty=1`（否则「初始检查绿就交差」会让 pop 路径
    仍是坏的却无人发现）⇒ MUST 用 stdout 的 grep 断言这个序列
    ⇒ 且 MUST 有 Task 1 的 Catch2 用例兜底

- [x] ### Task 7: 文档同步 + ADR 同步 + 计数同步
  - **action**:
    - ⚠️ **ADR 同步（必做）**：D0 **反转了 ADR-014 的既定决策**
      （`docs/adr/ADR-014-bundle-reflection-system.md:131` 把
      「确认 `bundle_base::create_field_slices_from_node()` 能正确处理 >10 个字段的
      切片创建」列为**前置条件确认项**）⇒ MUST 新增 ADR 或在 ADR-014 追加
      supersede 注记，写明「切片模型 → 字段一等节点模型」的替换与理由
    - ⚠️ **文档一致性清扫（必做）**：D6 之后，下列文档仍在教
      「bundle 作顶层 IO + `set_bundle_value()`」的用法，MUST 同步：
      - `skills/cpphdl-bundle-patterns/SKILL.md:75, 262, 265, 410`
      - `docs/skills/SIMULATOR-API-GUIDE.md:232`
      - `docs/usage_guide/04-simulator-api.md:68, 415`
    - ⚠️ **`get/set_bundle_value` 的去留 MUST 显式裁决**：D6 之后它们接近死 API
      （只对未连接字段有效）⇒ MUST 记录「保留（调试用途）/ 标记 deprecated」的决策，
      MUST NOT 默默修完就留着
    - 根 `AGENTS.md`：`CTEST_TOTAL` 181 → **182**；non-perf 170 → **171**；
      `tests/test_*.cpp` 111 → **112**；「已消除的过时信息」新增本 change 条目
    - `include/bundle/common_bundles.h`：三个 bundle 的方向语义加注释
      （说明「master 是发起方、slave 是响应方；镜像只是必要条件」）
    - `include/core/bundle/bundle_base.h`：`set_role` / 字段连接的注释同步
      （说明「字段是一等节点；`operator=` 是软件拷贝语义，不是连接语义」）
  - **verify**:
    ```bash
    bash scripts/verify_test_counts.sh
    cd build && ctest -N | tail -1 && ctest -N -LE perf | tail -1
    grep -rn "set_bundle_value" skills/ docs/usage_guide/ docs/skills/   # 与 D6 裁决一致
    ls docs/adr/ | grep -i bundle
    ```

- [x] ### Task 8: Requirement ↔ 测试 映射核对 ✅
  - **action**:
    - 逐条核对 `specs/bundle-master-slare-io/spec.md` 的每条 Requirement / Scenario
      都有对应的 Catch2 断言，产出**显式映射表**（Requirement → 测试名）
    - ⚠️ `grep -c "TEST_CASE"` **不构成**映射验证 ⇒ MUST 用映射表
    - ⚠️ 无法自动断言的条款（「reviewer 人工对比」类）MUST 改写为可断言的形式
      或从 spec 中删除
  - **映射表（2026-10-22）**：

    | Requirement / Scenario | 对应断言 |
    |---|---|
    | R1 · 字段 MUST 拥有自己的节点 | `tests/test_bundle_master_slave.cpp` 的全部用例（DUT 的字段均为一等节点） |
    | R1 · 设计只读不写的字段 MUST NOT 恒为 0 | `bundle master/slave: TB 驱动的 push 到达 slave 侧`（`push` 是只读字段；用例先断言初值 `empty==1` 再断言 push 后 `empty==0` ⇒ 非空洞） |
    | R1 · 字段节点 MUST 能在仿真中被寻址 | 同上（无「Value not found」告警）+ `bundle: 未连接 bundle 的字段值可写可读` |
    | R1 · 未驱动的字段取值 MUST 确定 | `bundle: 未连接 bundle 的字段值可写可读`（未连接字段） |
    | R1 · 字段节点身份在方向切换前后 MUST 不变 | `bundle: 字段节点身份在方向切换前后 MUST 不变` ✅（**实测抓到过真缺陷**：实现最初不幂等 → 方向切换会换掉节点身份 ⇒ 已修为幂等） |
    | R2 · 连接后原驱动节点仍可达 | 变异验证 D3（改回 `operator=` ⇒ 红）+ `tests/test_bundle_connection.cpp` 的 `require_driven_by()` |
    | R2 · 连接后值能跨 bundle 边界传播 | `bundle master/slave: TB 驱动的 push 到达 slave 侧` / `slave 产生的 empty 到达 master 侧` |
    | R2 · offset 非零的字段 MUST 正确传播 | `bundle master/slave: offset 非零的字段（push）能正确传播` |
    | R3 · fifo_bundle 的方向指认 | `bundle: master/slave 的字段方向指认与协议语义一致` ✅（**实测**：D5 变异「push 方向反转」**只有**本断言能抓到 —— 连接级测试对同方向字段会碰巧接对） |
    | R3 · 方向指认 MUST 可被自动断言 | 同上（逐字段枚举 + `std::holds_alternative`） |
    | R4 · connect() 不覆盖接收方的输出字段 | `bundle master/slave: slave 产生的 empty 到达 master 侧`（`empty` 是 slave 的输出）+ 变异 D4 |
    | R4 · 同方向字段时 src 驱动 dst | `tests/test_bundle_connection.cpp` 的 `require_driven_by()`（`SimpleBundle` 双方 `as_master()` ⇒ 同方向） |
    | R5 · 读取运行时驱动的 bundle 字段 | `bundle master/slave: slave 产生的 empty 到达 master 侧`（`empty` 由设计驱动） |
    | R5 · 写入未连接字段并被设计观察到 | `bundle: 未连接 bundle 的字段值可写可读` |
    | R5 · 读写 MUST 对称 | 同上 |
    | R5 · 打包 API 的宽度约束 MUST 被强制 | `bundle: 总宽 > 64 位的打包式 API MUST 显式拒绝而非静默截断` ✅（变异「去掉宽度 enforcement」⇒ 红） |
    | R5 · 64 位宽字段 MUST 精确往返 | `bundle: 64 位宽字段的位段提取 MUST NOT 触发位移溢出` ⚠️ 锁的是**值往返**，**不是** UB 探测器（GCC 把常量 `1ULL << 64` 折叠为 0 ⇒ 掩码恰好正确 ⇒ 值断言无法暴露 UB） |
    | R6 · 回归护栏存在且非空转 | 变异验证：D0/D1/D2/D3/D4/D5 **六项全部被抓到** |
    | R6 · 护栏 MUST 用 bundle 级接线 | `tests/test_bundle_master_slave.cpp` 的 DUT 用 `connect(master, slave)` |
    | R6 · 护栏 MUST 覆盖 offset 非零与只读字段 | 同上（`fifo_bundle` 5 字段、`push` 在 offset 8、`push`/`pop` 只读） |
    | R6 · 护栏 MUST 另有一个「未连接 bundle」用例 | `bundle: 未连接 bundle 的字段值可写可读` |
    | R6 · 护栏 MUST 在 ctest 内 | `tests/CMakeLists.txt` 的 `add_catch_test(test_bundle_master_slave ...)`（`ctest -N` = 182） |

    ⇒ **无「无断言覆盖」的条款**；唯一不可自动检测的是 R5 的 UB 一条（已如实标注）。
  - **verify**: 映射表覆盖每条 Requirement / Scenario ✅（上表）

- [x] ### Task 9: 全量回归 + archive ✅
  - **action**:
    - `ctest -LE perf`（新增 1 个 target → 期望 **171/171** 全绿）
      → **实测 171/171 全绿**；`ctest -L perf` **11/11 全绿**（1330 s）
    - `./run_all_ported_tests.sh`
    - `./build/samples/fifo_bundle`（MUST 退出 0 且演示完整序列）
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
