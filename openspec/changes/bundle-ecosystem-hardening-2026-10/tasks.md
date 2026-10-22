## Tasks

> **状态：已实施（2026-10-22）。**
> 立项自 2026-10-22 对「`bundle-master-slave-io-2026-10` 之后全仓遗留与遗漏」的
> 系统审计。审计结论：**11 项遗留 / 遗漏 + 5 项测试完备性缺口**（见 `proposal.md`）。
> 本 change 收敛 **L1-L9 + L11 + T3/T4**（bundle 生态的**边角**设施）；
> **T1/T2（samples 零覆盖 + 既有 segfault）归独立的
> `samples-regression-guard-2026-10`**。
>
> ⚠️ **本任务表经 2026-10-22 双路评审修订**：D1 改写为「ctor 只存 prefix +
> 建节点时命名 + 非虚嵌套下推」（原「推迟到 `set_role()`」修不了 L11）；D2 拆成
> **三条唯一裁决**（MUST NOT 留析取）；D6 拆成**两阶段**（先表征测试再收敛）；
> 新增 prefix 生命周期决策、测试计数同步、samples 豁免移除三组动作。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7。
> ⚠️ **跨 change 顺序**：`samples-regression-guard-2026-10` **先**落地（护栏带
> `nested_bundle_demo` 显式豁免，失效条件 = 本 change archive），本 change **后**落地
> ⇒ Task 7 MUST 移除该豁免。
> ⚠️ **TDD 纪律**：Task 1 先写会失败的用例（RED），Task 2/4 逐条转绿；
> Task 3 是**物理移除**（无 RED→GREEN，只有「移除后仍全绿」）；
> Task 5 显式**两阶段**（先表征测试钉现状、再收敛）。
> 每个 Task 的 verify 必须是**可执行命令**。

- [x] ### Task 0: 审计取证 + 评审修订（已完成，结论折进 `proposal.md`）
  - **action**:
    - 逐项确认 L1-L9 / L11 / T1-T5 的**现状与根因**（复现命令见 `proposal.md` 附录 A）
    - ⚠️ L1 的根因 MUST 用 **gdb 精确帧**确认（MUST NOT 依赖「崩在
      `set_name_prefix` 里」这种被内联误导的归因）：
      ```
      #0 bundle_field_traits<ch_stream>::set_name at bundle_traits.h:135
      #8 bundle_base::bundle_base at bundle_base.h:34   ← 基类构造期调虚函数
      ```
    - ⚠️ **2026-10-22 评审修订已折进**：L2 / L3 / L6 / T4 的**取证更正**、
      **L11**（prefix 机制空壳）的**新增**、L7 的**重载决议实测**（探针 ⇒ `FREE`）
  - **verify**: `proposal.md` 附录 A 的每条命令 MUST 可复现

- [x] ### Task 1: 写会失败的用例（RED）
  - **action**:
    - 新增 `tests/test_bundle_ecosystem.cpp`，覆盖 T3 的每一类边角：
      1. **嵌套 bundle 字段**的连接（`ch_stream` 成员）
      2. **`flip()`** 的方向取反（master→slave、slave→master、**unknown→slave**）
      3. **`master()` / `slave()` 工厂**的字段节点**独立性**
      4. **`deserialize_bits_to_fields()`** 路径（字段节点 **≠** 源位串切片节点）
      5. **`ch_stream`** 的连接语义 —— ⚠️ **行为级**（驱动源字段 → sink 字段读到值），
         MUST NOT 只断言 `impl() != nullptr`（那种断言在两种语义下都绿 = 零信号）
         ⚠️ **光有「值传播」仍然零区分**（重绑定与数据流边**都**会让值传播）⇒
         MUST 另有一条**能区分两种语义**的断言，二选一：
         (a) 连接后 **sink 字段的节点身份 ≠ source 字段的节点身份**（数据流边下成立、
             重绑定下不成立）；或 (b) 连接前 sink 字段**已有驱动**时，该驱动节点在连接后
             **仍可达**（重绑定会把它孤儿化）
      6. ⚠️ **已移除设施**：`bundle_concat` / `bundle_slice_view` / 工厂
         `bundle_cat` / `bundle_slice` MUST **不再出现**在公开派生类清单里
         （⇒ 一条断言清单内容的用例；**MUST NOT** 给将被移除的设施补行为断言）
    - ⚠️ **prefix 生命周期**（D1 的新决策）MUST 各有一条断言：
      - prefix 构造 + 角色已设定 ⇒ 叶子节点名**带** `prefix.` 前缀
      - 同一 bundle `as_master()` 后 `as_slave()` ⇒ 节点名**不重复拼接** prefix
      - 拷贝构造出的 bundle 再 `as_master()` ⇒ 节点名**仍带** prefix
      - 节点已建立后**再改 prefix** ⇒ 节点名**不变**（事后改名无效）
      - 角色**从未**设定 ⇒ 字段**无节点**（现有语义，`test_bundle_node_management.cpp:169-171`）
    - ⚠️ 每条用例 MUST 能**单独**抓住对应的回归（否则是空转）
    - MUST 在实现修复前 **全红**
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_ecosystem
    ./build/tests/test_bundle_ecosystem
    ```
    MUST 观察到**红**

- [x] ### Task 2: 修 prefix 命名路径（L1 / L11 / D1）
  - **action**:
    - `include/core/bundle/bundle_base.h` **三步，缺一不可**：
      1. `bundle_base(const std::string &prefix)`（`:32-35`）MUST **只存 prefix**
         （新增基类成员 `std::string prefix_`），MUST NOT 调用虚函数 `set_name_prefix()`
         ⇒ 构造期不再触碰派生类成员 ⇒ **L1 的 UB 根除**
      2. `create_field_proxy_nodes()`（`:348`）建代理节点时 MUST 用
         `prefix.empty() ? field_name : prefix + "." + field_name` 命名
         ⇒ 「命名与建节点同时完成」⇒ **L11 修复**
      3. `set_role()`（`:288-308`）MUST 把 prefix **非虚**地传给嵌套 bundle 字段
         （MUST NOT 走 `set_name_prefix()` 虚函数）⇒ 嵌套 bundle 的叶子节点在自己的
         `set_role()` 里拿到 `outer.inner.field` 形式的完整名字
    - **幂等**：既有 guard（`:341`，字段已有节点就跳过）⇒ 节点与名字只生成一次
      ⇒ 「prefix 不重复拼接」免费成立
    - **prefix 生命周期**（proposal D1 的决策表，MUST 全部落实）：
      - 拷贝构造 MUST **复制** prefix
      - 公开的 `set_name_prefix()` 事后调用 MUST **不立即改名**（节点已建立），
        MUST 在注释中写明
      - 「角色从未设定」的 bundle：字段无节点 ⇒ prefix 无可观测效果
    - ⚠️ 影响**所有** bundle 的命名时机 ⇒ MUST 全量回归
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/samples/nested_bundle_demo     # MUST 不再 segfault（exit=0）
    ```
    「嵌套 bundle 带 prefix 构造」「prefix 不重复拼接」「拷贝后仍带 prefix」三条
    MUST 转绿

- [x] ### Task 3: 嵌套 bundle 设施的语义（L2 / L3 / D2）—— **三条唯一裁决**
  - **action**:
    - **3.1 `bundle_concat` → 物理移除**：
      - 删除 `bundle_concat` 类型 + 其公开工厂 `bundle_cat()`
      - ⚠️ MUST NOT 仅标记 deprecated 后继续留在公开清单里（那会让「已废弃但仍计入
        清单」与新 Requirement 并存矛盾）
      - ⚠️ MUST NOT 留下会触发 deprecation 告警的入口（工厂必须一并移除 —— 仓库先例：
        `deprecate-hdl-logical-operators` 首轮 315 warning 回滚）
    - **3.2 `bundle_slice_view` → 物理移除**：同上，连带公开工厂 `bundle_slice()`。
      ⚠️ MUST NOT 顺手「实现真正的切片」（`Start`/`Count` 今天**根本未被使用**
      ⇒ 那是**新能力**，属 Non-Goal 4）
    - **3.3 清理移除后的残留**（ZERO-DEBT，防悬空引用）：
      - `include/core/bundle/bundle_operations.h` 移除这 4 个符号后**只剩 include**
        ⇒ MUST **删除该文件**（空壳文件违反 ZERO-DEBT）
      - MUST 删除两处悬空 include：`tests/test_bundle_advanced_features.cpp:9` 与
        `samples/bundle_advanced_features.cpp:8`（两个 target 均已注册：
        `tests/CMakeLists.txt:396`、`samples/CMakeLists.txt:10`）
      - MUST 删除 `tests/test_bundle_advanced_features.cpp:63-75` 的**被注释掉的**
        `BundleOperations` TEST_CASE 块（`:63` 是 `/*`、`:75` 是 `*/` —— ⚠️ MUST
        连注释符一起删，否则留下 `/*` 而无 `*/` ⇒ **unterminated comment** ⇒ 编译失败），
        其中含 `:72` 的 `ch::core::bundle_cat(...)` 引用
        —— ⚠️ grep 不识别 C 注释 ⇒ 该行**必须删**，否则本 Task 的 verify 必红；
        且它本身就是 ZERO-DEBT 的「No dead code」
      - MUST 同步 `include/core/bundle/bundle_base.h:84` 的注释（它列举了这两个
        已移除的类型名）—— ⚠️ 该同步**属于本 Task**，否则本 Task 的 verify 会红
    - **3.4 `deserialize_bits_to_fields()` → 改为字段一等节点**（**保留**，不废弃）：
      - `bundle_serialization.h:85-97` MUST NOT 再用 `bit_select`/`bits<>` 切片重绑定
      - ⚠️ 该函数**有活跃消费方**（`deserialize<>` → `tlm_bundle_converter.h:54` /
        `test_bundle_serialization.cpp:47,149` / `test_tlm_bundle_phase1.cpp:80,96` /
        `samples/tlm_bundle_demo.cpp:121`）⇒ MUST 修
      - ⚠️ 既有断言**全是宽度断言**（`.width ==` / `bundle_width_v<>`，**零**
        `impl()` 身份断言）⇒ 它们对两种语义**同样通过** ⇒ **不需要**反转，
        也**不会**因此变红 ⇒ 但这正说明该路径**零行为覆盖** ⇒ MUST **新增**
        行为级断言（字段节点 ≠ 源位串切片节点 + 值正确），
        MUST NOT 删除既有宽度断言来「让它变绿」
    - **3.5 同步 `bundle-base-integer-ctor` capability**（D12）：
      - 该 capability 主 spec 的派生类清单 MUST 由 **22 → 20**（去掉
        `bundle_slice_view` / `bundle_concat`）
      - ⚠️ MUST 通过**本 change 的 delta**（`specs/bundle-base-integer-ctor/spec.md`
        的 `## MODIFIED Requirements` 段）完成，MUST NOT 直接改 `openspec/specs/`
      - ⚠️ 该 capability 主 spec 的 `## Purpose` 段**不是** Requirement、delta 不合并
        ⇒ MUST 在 archive **之后**人工订正（仓库先例：`axi4-full-slave-burst`
        的 Purpose 即「archive 后已补写」）
    - **3.6 写 ADR**（公开 API 变更 ⇒ MUST 先有 ADR，仓库先例 ADR-036 / ADR-037）：
      - 新建 `docs/adr/ADR-043-bundle-operations-removal.md`：记录移除
        `bundle_concat` / `bundle_slice_view` + 工厂 `bundle_cat` / `bundle_slice`
        的决策、理由、迁移路径
      - ⚠️ MUST NOT 出现「spec 里要求有 ADR、但仓库里没有 ADR 产物」的状态
        ⇒ delta 的「移除 MUST 走 ADR 流程」Scenario 在 ADR 落地前**不满足**
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_bundle_serialization
    ./build/tests/test_tlm_bundle_phase1
    ./build/tests/test_bundle_bitstream
    ./build/tests/test_nested_bundle
    ./build/tests/test_bundle_advanced_features
    ./build/samples/bundle_advanced_features
    # 4 个符号 + 头文件本身 MUST 全部消失
    # ⚠️ 排除 `tests/test_bundle_ecosystem.cpp` —— 该文件的**职责**就是断言
    #    它们**不存在**（`__has_include` 守卫 + 注释里点名）⇒ 它必然命中。
    #    （先例：`scripts/verify_rst_n_wiring.sh` 同样豁免自己的运行时护栏。）
    grep -rn "bundle_concat\|bundle_slice_view\|bundle_cat\|bundle_slice\b\|bundle_operations" \
      --include=*.h --include=*.cpp . | grep -v third_party | grep -v openspec \
      | grep -v "tests/test_bundle_ecosystem.cpp"
    #   ⇒ MUST 命中 0 行
    test -f docs/adr/ADR-043-bundle-operations-removal.md   # MUST 存在
    ```

- [x] ### Task 4: `flip()` / 工厂 / `operator=` 的契约（L4 / L5 / L6 / D3-D5）
  - **action**:
    - `flip()`（`bundle_base.h:187-194`）：方向**取反**（MUST NOT 硬编码 `as_slave()`）
      + **未设角色（unknown）MUST 按 unknown→slave 处理**
      （理由：8 个现存调用点里 **6 个** flip 的是从未设角色的 bundle、**2 个** flip 的是
      已设 `master` 角色的 bundle（后者 master→slave 本就与现状一致）⇒ unknown→slave
      保持**零行为变化**；unknown→master 会让它们全部翻转）
    - `master()` / `slave()`（`bundle_utils.h:10-17`）：产出**字段节点独立**的 bundle；
      ⚠️ 「独立」**只指字段节点**（总线节点按拷贝构造语义仍共享，
      `bundle_base.h:38-41`，被 `tests/test_bundle_node_management.cpp:164` 钉住）
      ⇒ MUST NOT 顺手改总线节点语义（会破坏既有断言）
    - `bundle_base::operator=`（`:176-181`）：MUST NOT 改变行为，但 MUST 加注释
      写明它是**软件拷贝**语义（与连接语义区分）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_bundle
    ./build/tests/test_bundle_advanced
    ./build/tests/test_bundle_advanced_features
    ./build/tests/test_bundle_connection
    ./build/tests/test_bundle_node_management
    ./build/tests/test_nested_bundle
    ```

- [x] ### Task 5: `ch_stream` 的连接语义收敛 + 双后端覆盖（L7 / T4 / D6）—— **两阶段**
  - **action**:
    - **5.1 阶段 1（表征测试）**：MUST 先补**行为级**断言钉住**当前**语义
      （驱动源字段 → sink 字段读到值，interpreter + verilator 双跑）
      ⚠️ 现有 `tests/test_stream_operators.cpp` 的 10 个用例**全是结构性断言**
      （`impl() != nullptr`，无一次 `tick()`）⇒ 两种语义下都绿 ⇒ **零信号**
    - **5.2 阶段 2（收敛）**：`include/chlib/stream_operators.h:25-32` 的自由
      `operator<<=` 改为**数据流边**：
      ```
      sink.valid   <<= source.valid;
      sink.payload <<= source.payload;
      source.ready <<= sink.ready;     // ready 反向流动 ⇒ 两条边
      ```
      ⇒ 与 ADR-042 一致
    - ⚠️ **爆炸半径**（评审修正）：`bundle_base::connect_field_based_on_direction_and_source`
      （`bundle_base.h:144-146`）对 **bundle 类型字段**递归调 `left_field <<= right_field`
      ⇒ **任何嵌套 ch_stream 字段**（`CustomNested.data_stream`、AXI / stream 子系统）
      的 bundle 级连接同样落到自由函数上 ⇒ 收敛 MUST 跑**全量**双后端矩阵
    - ⚠️ **DUT 约束（更正）**：Verilator 后端的 **64-bit 顶层端口上限**对本用例
      **不构成约束** —— `ch_stream<ch_uint<32>>` 的总宽是 **34 bit**（32+1+1，
      `tests/test_bundle_serialization.cpp:58`），其字段为 `payload(32)` /
      `valid(1)` / `ready(1)`，**单个**端口宽度均 ≤ 64 ⇒ **无需**「把连接藏进
      组件内部、顶层只暴露窄端口」的规避手法（此前提案里的「66 bit」是**算错**的）
    - ⚠️ 新失败模式：两侧字段**皆无节点**时，今天是静默 nullptr 别名，收敛后会触发
      CHERROR ⇒ 收敛前 MUST 清点所有 `ch_stream` 间 `<<=` 调用点，确认角色已设定
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_ch_stream
    ./build/tests/test_stream_operators
    ctest -L verilator_e2e --output-on-failure
    ctest -R "stream" --output-on-failure
    ```

- [x] ### Task 6: 死代码清理 + 计数/文档同步（L8 / L9 / D8 / D9 / D11）
  - **action**:
    - 删除 `include/core/direction.h` 的 `internal_direction` + `is_internal_v`
      （全仓零使用 ⇒ ZERO-DEBT POLICY 的 "No dead code"）
    - 文档写明 `get/set_bundle_value` 的**适用范围**（仅未连接 bundle）
    - `include/core/bundle/bundle_base.h` / `bundle_serialization.h` /
      `bundle_utils.h` 的注释同步
      ⚠️ **不含** `bundle_operations.h` —— 该文件已在 Task 3 被**删除**（4 个符号移除后
      只剩 include ⇒ 空壳违反 ZERO-DEBT）
    - ⚠️ **测试计数同步**（D11）：新增 `tests/test_bundle_ecosystem.cpp` ⇒
      根 `AGENTS.md` 的 `CTEST_TOTAL: 182` → **183**、`tests/test_*.cpp` 112 → **113**
      ⇒ MUST 跑 `scripts/verify_test_counts.sh` 确认仍绿（CI 2 个 job 都跑它）
    - 根 `AGENTS.md`：「已消除的过时信息」新增本 change 条目
    - ⚠️ **ADR 同步**：
      - `docs/adr/ADR-012-io-port-dual-api.md:107,268` 引用了 `internal_direction`
        ⇒ 删除该类型后 MUST 一并订正（否则 ADR 变陈旧）
      - `docs/adr/ADR-042-bundle-field-first-class-nodes.md:82-84` 把
        `bundle_concat` / `bundle_slice_view` 记为「遗留债务，独立 follow-up」
        ⇒ 本 change **就是**那个 follow-up ⇒ MUST 同步（否则 ADR 变陈旧）
    - ⚠️ **测试注释同步**：`tests/test_bundle_literal.cpp:9,15` 的「all 22 derived
      classes」注释 ⇒ MUST 随移除同步
    - ⚠️ 顺手修正**已存在的计数漂移**（评审发现，非本 change 引入）：
      根 `AGENTS.md` 的 `bundle-master-slave-io-2026-10` 条目写「5 用例 / 19 断言」，
      实际 `tests/test_bundle_master_slave.cpp` 是 **8 个 TEST_CASE / 30 个断言**
      ⇒ MUST 一并订正
    - ⚠️ **验证记录**（D13）：新建
      `docs/reports/verification-bundle-ecosystem-hardening-2026-10.md`，
      记录四类变异验证结果等
  - **verify**:
    ```bash
    grep -rn "internal_direction\|is_internal_v" --include=*.cpp --include=*.h . \
      | grep -v third_party | grep -v openspec        # MUST 命中 0 行
    # ⚠️ `docs/archive/` 是**归档日报**（历史快照，按惯例冻结不改写）
    #    ⇒ MUST 豁免，否则 guard 永远命中 docs/archive/daily-reports/DESIGN_io.md
    grep -rn "internal_direction" docs/ | grep -v "docs/archive/"   # MUST 命中 0 行
    bash scripts/verify_test_counts.sh                # MUST 仍通过
    test -f docs/reports/verification-bundle-ecosystem-hardening-2026-10.md   # MUST 存在
    ```

- [x] ### Task 7: 全量回归 + 摘除 samples 豁免 + archive
  - **action**:
    - `ctest -LE perf`（新增测试 target，期望数 182 → **183**）
    - `ctest -L perf`
    - `./run_all_ported_tests.sh`
    - `./build/samples/nested_bundle_demo`（MUST exit=0）
    - ⚠️ **摘除 samples 护栏的豁免**：`samples-regression-guard-2026-10` 落地时为
      `nested_bundle_demo` 建的**显式豁免** MUST 在此**移除**
      （豁免的失效条件就是本 change archive）⇒ 然后全量重跑护栏
    - `openspec archive bundle-ecosystem-hardening-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
    - ⚠️ **archive 后**：MUST 人工订正
      `openspec/specs/bundle-base-integer-ctor/spec.md` 的 `## Purpose` 段
      （派生类清单 22 → 20；delta **不合并** Purpose ⇒ 必须手工改；
      仓库先例：`axi4-full-slave-burst` 的 Purpose 即「archive 后已补写」）
  - **verify**:
    ```bash
    # ⚠️ 全部命令 MUST 在**仓库根**执行（`cd build` 会让后续 guard 静默失真：
    #    `grep` 找不到目录时 exit=2、`openspec validate` 打印「No items found」且 exit=0
    #    ⇒ **假绿**）⇒ 用**子 shell** 形式，失败也不会污染后续 cwd
    (cd build && ctest -LE perf -j4 --output-on-failure)
    (cd build && ctest -L perf --output-on-failure)
    ./run_all_ported_tests.sh
    bash scripts/run_all_samples.sh          # MUST 全绿（豁免已摘除）
    openspec validate --specs --strict       # MUST 17/17 通过（本 change 不新增 capability）
    # ⚠️ **CLI 版本前置**：该断言依赖 openspec CLI **1.4.1**（仓库现有 spec 是按该
    #    版本的校验规则写就的）。CLI **1.14.x** 新增了两条 strict 规则
    #    （「Requirement 正文 > 500 字符」「Purpose 仍是占位符」），仓库现有 **15/17**
    #    个主 spec 会因此变红 ⇒ 与本 change 无关（纯净 HEAD 同样红）。
    #    ⇒ 本 change 的 delta 已按新规则收敛（ADDED 段每条 Requirement 正文 ≤ 500 字符），
    #    但「17/17」这个断言在 CLI 升级后需要**先解决仓库级迁移**（见下）才能成立。
    #    ⚠️ CI **不跑** `openspec validate`（`.github/workflows/ci.yml` 无此步骤）⇒
    #    该漂移**不阻断合并**，但会让本 Task 的断言在本地判红。
    # 主 spec MUST NOT 残留本 change 名（delta 内 MUST NOT 写含 change 名的具体文件名）
    grep -rn "bundle-ecosystem-hardening-2026-10" openspec/specs/   # MUST 命中 0 行
    # 派生类清单句里 MUST NOT 再列已移除的类型
    # ⚠️ 此 guard 在 archive **之后、且 Purpose 手工订正之后**才会通过 ——
    #    archive 不会合并 delta 的 `## Purpose` ⇒ 订正前该句仍列着旧类型（预期红）
    ! grep -nE "派生类（[^）]*(bundle_slice_view|bundle_concat)" \
      openspec/specs/bundle-base-integer-ctor/spec.md
    # ⚠️ 本 guard 只锁「MODIFIED 已合并」（Requirement 正文里的「20 个」）；
    #    锁「Purpose 已订正」的是上一条 guard
    grep -c "20 个" openspec/specs/bundle-base-integer-ctor/spec.md   # MUST ≥ 1
    ```
