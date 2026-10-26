# Tasks: RV32I 字节道写语义（`sb` / `sh`）

## Phase 1: 实现

- [x] T1.1 `DataTCM` 新增 `strbe` 输入端口 + 写路径改读-改-写
      （`aread` → `byte_lane_mask` → `byte_lane_apply` → `write`）
      —— `include/cpu/pipeline/rv32i_tcm.h`
- [x] T1.2 流水线侧 lane 对齐：`data_write_data <<= shl<32>(exmem_rs2_data, lane_shift)`，
      `lane_shift = 8*addr[1:0]` 且**只对 SB/SH** —— `include/cpu/pipeline/rv32i_pipeline.h`
- [x] T1.3 SoC 接线：`dtcm.io().strbe <<= pipeline.io().data_strbe;`
      —— `examples/riscv-mini/src/rv32i_pipeline_top.h`
- [x] T1.4 全部既有 `DataTCM` 实例化点显式驱动 `strbe`
      （`test_pipeline_exec.cpp` ×3、`test_riscv_tests_pipeline.cpp` ×1）

## Phase 2: 测试

- [x] T2.1 固件尾部追加 `sb`（lane 1）/ `sh`（lane 2-3）+ 两条 `lw` 读回整字
      （**尾部追加 ⇒ 既有 word 索引一个都不移动**）
- [x] T2.2 编码器自校验：`SB(2,0,0x101) == 0x102000A3`、`SH(3,0,0x122) == 0x12301123`
- [x] T2.3 golden：store 拍的写数据 = `0x300` / `0xD0000`；读回整字 = `0x030A` / `0xD0000`
- [x] T2.4 反向断言：未对齐的写数据（`3` / `13`）MUST NOT 出现
- [x] T2.5 store 拍计数 3 → 5
- [x] T2.6 写回序列 11 → 13 条（追加 `{12, 0x030A}`、`{13, 0xD0000}`）

## Phase 3: 验收

- [x] T3.1 `ctest -LE perf` 零回归（191/191）
- [x] T3.2 `scripts/verify_test_counts.sh` 绿（测试源数**未变** ⇒ 计数不变）
- [x] T3.3 `openspec validate --strict` 通过
- [x] T3.4 变异验证：去掉 lane 对齐 ⇒ store 拍数据变 `3`/`13` ⇒ 断言转红；
      去掉 `strbe` 接线 ⇒ 整字覆盖 ⇒ 读回值转红
- [x] T3.5 ✅ **对抗审稿**（critic 第 1 轮，2026-10-26）：**APPROVE_WITH_CONDITIONS**
      （`mergeable: yes` / `scope_exceeded: true`）—— **无 BLOCKER**；
      3 条 SHOULD_FIX + 2 条 NITS **同批处理**（见下表）
- [ ] T3.6 `openspec archive`（**待 T3.7 的门**）

## 审稿记录（2026-10-26 · critic 第 1 轮）

**结论**：**APPROVE_WITH_CONDITIONS**（`mergeable: yes` / `scope_exceeded: true`）——
**无 BLOCKER**；3 条 SHOULD_FIX + 2 条 NITS + 1 条 scope_exceeded backlog。

| # | finding | 处置 |
|---|---|---|
| SHOULD_FIX 1 | `design.md` §5 声称「非对齐 `sw` / `sh` 保持既有行为（整字写）」—— 对 **`sh` 不成立**：非对齐 sh 的行为**已改变**（改为「按 `addr[1:0]` 移位 + 按 `addr[1]` 选半字掩码」的**部分写**） | ✅ **改正 design.md §5**：拆成两条（`sw` 保持不变 / `sh` 已改变并给出具体行为），并注明「测试固件 MUST 只用对齐访问」；同时在 §1 数据通路图注明两套偏移来源（`addr[1]` vs `addr[1:0]`）只在**对齐 sh** 时一致 |
| SHOULD_FIX 2 | `test_riscv_tests_pipeline.cpp` 把 `strbe` **恒驱动为 0xF** —— 对**流水线写路径**为假：该 fixture 跑 riscv-tests（含 `rv32ui-p-sb`/`sh`）⇒ 会变成「**lane 对齐数据的整字覆盖**」⇒ 结果错误（当前 CI 全 SKIP ⇒ **已定位未引爆**的地雷） | ✅ 改为 mux：`select(io().dtcm_write_en, 0xF, pipeline.io().data_strbe)`（与 `wdata` 的 mux **同构**） |
| SHOULD_FIX 3 | spec delta 的 Requirement「新增的输入端口 MUST 被全部实例化点显式驱动」**范围无界、无核验机制、不可核**，且是**框架层约定**却放在 `rv32i-isa-semantics` capability 里 | ✅ **收窄**为「`DataTCM.strbe` MUST 被其实例化点显式驱动」（全仓 3 处，grep 可证）+ 新增 Scenario「整字写与字节写走不同的车道来源」；框架层约定**移到 `AGENTS.md` 约定层**（含「独立立项」标注） |
| NIT 1 | §1 数据通路图的两套偏移来源未注明 | ✅ 与 SHOULD_FIX 1 同批处理 |
| NIT 2 | `ch_in<ch_uint<4>> strbe` 的宽度硬编码（`byte_lane_mask` 要求 `DATA_WIDTH/8` 位） | ✅ design.md §5 已如实声明「只支持 32 位」⇒ 足够，不改 |

### scope_exceeded（记 backlog，**不**在本 change 返工）

- 「**任意**新增输入端口 ⇒ 全实例化点 MUST 显式驱动」的 **rst_n 同型自动化护栏**：
  `verify_rst_n_wiring.sh` 的形态（per-test-case 模拟驱动检查 + 组件注册表）
  **无法直接推广**到任意新端口 —— 需要先设计「必需端口注册表」
  （不同端口的未驱动读值语义不同：`rst_n` 读 0 = 永久复位、`strbe` 读全 1 = 碰巧整字写）。
  即时风险已被两层覆盖：实例化点仅 3 处且全已接线；SoC 侧若断线，
  golden 读回断言（`0x030A` vs 整字覆盖的 `0x0300`）会转红。
  ⇒ **独立立项**（已登记进 `AGENTS.md` 的约定层 + 阶段级执行计划的 backlog）。

### 对抗审稿第 2 轮（复审）结论

**结论**：**APPROVE**（`mergeable: yes` / `scope_exceeded: false`）——
**无 BLOCKER、无 SHOULD_FIX**；仅 1 条 NIT。

- 复审范围（严格限定）：第 1 轮 3 SHOULD_FIX + 2 NITS 的**闭合证据**
  + `b4ad90f` 的新增 diff ⇒ **不重审全文**（第 1 轮已审过的实现正文未被本轮 diff 触碰）
- critic 逐条核对：SHOULD_FIX 1 ✅（§5 拆两条 + §1 注明两套偏移来源）、SHOULD_FIX 2 ✅
  （`select(io().dtcm_write_en, 0xF, pipeline.io().data_strbe)` 与同处 `wdata` mux 逐字同构）、
  SHOULD_FIX 3 ✅（Requirement 收窄 + 全仓 3 个实例化点已逐一过目 + 框架层约定已移到
  `AGENTS.md`）、NIT 2 ✅（design.md §5「只支持 32 位」在档）、scope_exceeded 处置 ✅
- **NIT**（不阻断）：根 `AGENTS.md:125` 引用的 `openspec/specs/rv32i-isa-semantics/spec.md`
  在归档前是**悬空指针**（该 capability 由本 change 新建）⇒ **归档后自动消解**
  （已核实 `openspec/specs/rv32i-isa-semantics/spec.md` 存在且 `openspec validate --strict` 通过）

### 归档事实（2026-10-26）

- `openspec archive rv32i-store-byte-enable-2026-10 --yes`
  ⇒ 归档为 `openspec/changes/archive/2026-10-26-rv32i-store-byte-enable-2026-10/`
- 新 capability `rv32i-isa-semantics`（**2** 条 Requirement）已合并进
  `openspec/specs/rv32i-isa-semantics/spec.md`
- ⚠️ `openspec archive` 自动写的 `## Purpose` 是**占位句** ⇒ 已**补写**为该 capability 的
  长期契约说明（CLI 明确要求直接编辑主 spec；既有先例 = `axi-interconnect-routing`）
  ⇒ `openspec validate rv32i-isa-semantics --type spec --strict` ⇒ **valid**

### 归档后剩余门（**合并门**，非本地可完成）

- [ ] CI 的 `macos-14` job 绿（BSD/BWK awk + macOS 编译器；本机是 Linux ⇒ 不可本地完成）
      ⇒ ⚠️ 若该 job 红 ⇒ MUST **revert 本归档 commit**（含主 spec 的新增 capability）
- ⚠️ 本机 `openspec validate --specs --strict` 的既有红（15/17 主 spec）= **CLI 1.14.3
  与仓库钉的 1.4.1 的规则漂移**（与本 change 无关，纯净 HEAD 同样红）；CI **不跑**
  `openspec validate` ⇒ 不阻断合并

## Out of Scope

- 不改 `ch_mem`（不引入字节使能写 API）
- 不做非对齐 `sw` / `sh` 的异常处理
- 不把 SoC 的 dmem 换成 AXI 互连
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
