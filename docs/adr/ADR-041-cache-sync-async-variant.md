# ADR-041: cache 子系统拆分为 Sync (1-cycle sread) + Async (0-cycle aread) 双变体

- Status: Accepted
- Date: 2026-10-19
- Author: build-agent
- Parent: ADR-039 (cache-complete-rewrite via ch_mem)

## Context

cache-complete-rewrite 第一次落地 (commit 91a0833, 1221819) 后, handoff §3.5
文档同步阶段评估 `ch_mem::aread` (0-cycle combinational) vs `ch_mem::sread`
(1-cycle registered) 在 RV32I L1 cache 集成时的实际表现.

发现:

1. **业界惯例**: 商用 L1 cache 主流是 1-cycle sread (Xilinx BRAM pipeline register,
   ARM Cortex-A 系列), 软核 TCM 用 0-cycle aread (SiFive ITCM/DTCM).
2. **项目已有约定**: root AGENTS.md `MEM READ PORT` 段 + `verilator-e2e-coverage`
   spec §R1 invariant 3 明确 RV32I **imem 必须 sread** (避免 pc_lag 错位 1 cycle).
3. **Fmax 影响**: sread 路径 SRAM tco + 1 dff = ~3-4 ns, aread 路径
   SRAM tco + 数据 mux + 输出 reg = ~5-7 ns (28nm 估算). **sread 接近 2 倍 Fmax**.
4. **Pipeline 影响**: imem 用 sread 后 RV32I 5-stage pipeline pc_lag **自然对齐**
   (无需 IF/ID reg 额外吸收). imem 用 aread 需要下游 reg 吞 1 cycle pc_lag.

权衡之后决定把 cache 子系统**拆分为两个变体**, 让用户按场景选择:

| 变体 | hit latency | 用途 | 默认? |
|---|---|---|---|
| `ICacheSync` / `DCacheSync` | **1 cycle** (sread) | RV32I L1 cache, 高 Fmax | **默认** |
| `ICacheAsync` / `DCacheAsync` | **0 cycle** (aread) | 软核 TCM, 低频设计 | 备选 |

旧 `ICacheComplete` / `DCacheComplete` 改为 **shim 别名指向 Sync 版本**, 保持向后兼容.

## Decision

1. **拆分文件**:
   - `include/cpu/cache/i_cache_sync.h` — ICacheSync (1-cycle sread)
   - `include/cpu/cache/d_cache_sync.h` — DCacheSync (1-cycle sread)
   - `include/cpu/cache/i_cache_async.h` — ICacheAsync (0-cycle aread)
   - `include/cpu/cache/d_cache_async.h` — DCacheAsync (0-cycle aread)
2. **shim 兼容**: `include/cpu/cache/{i,d}_cache_complete.h` 改为 `using ICacheComplete = ICacheSync;` 别名, 旧引用方不受影响.
3. **openspec spec**: `cpu-cache-subsystem/spec.md` 加 `Requirement: cache variant selection` 描述两个变体的选择规则.
4. **测试覆盖**: `tests/chlib/test_cache_complete.cpp` 扩展到 9 个 test_case, 含:
   - Sync 5 个 (原有 miss-fill, dcache-read, dcache-write, g1, g2)
   - Async 2 个 (miss-fill + dcache-read)
   - Sync/Async variant equivalence 2 个 (验证两种变体同一 fill 序列得到相同最终数据)

## Consequences

### Positive

- ✓ 用户按场景选 cache 变体, RV32I L1 用 Sync (默认, 符合项目 imem 约定)
- ✓ TCM 用例保留 (Async 类)
- ✓ Fmax 提升 (Sync 路径 ~2 倍 Fmax)
- ✓ 5-stage pipeline pc_lag 对齐 (Sync + RV32I IF/ID reg)
- ✓ 向后兼容 (旧 `ICacheComplete` 引用方零迁移成本)

### Negative

- ✗ 文件数量 × 2 (但每个文件规模 < 250 行, 总规模 < 1k 行)
- ✗ 测试覆盖 × 2 (已自动化, ~30 min 增量工作量)
- ✗ riscv-mini 集成需要选 Sync (与 imem sread 约定一致, 无额外工作)

### Risks

| Risk | Mitigation |
|---|---|
| 用户误用 Async 在 RV32I imem 集成 | spec `Requirement: cache variant selection` 明确 RV32I imem/dmem 用 Sync; shim 默认指向 Sync |
| 测试覆盖未发现某个变体的 bug | 9 个 test_case 覆盖 miss-fill / write-through / g1 / g2 + 2 个 variant equivalence |
| spec 与 cache 文件注释 drift | ADR-041 引用 spec 文件, 强制一致性 |

## Alternatives Considered

### A. 维持单一 `ICacheComplete` + `DCacheComplete`, 内部固化 1-cycle sread

- ✗ 用户无法选 0-cycle aread 模式 (TCM 用例缺失)
- ✗ 类名 `Complete` 不传达 latency 语义

### B. compile-time template 参数 (`template <bool AsyncRead>`)

- ✗ 二进制大小 × 2
- ✗ 测试覆盖 × 2 (但更容易, 不需要多个类)
- 优点: 单一文件, 单次 instantiate
- 缺点: 用户需要读 template param 才能知道 latency

### C. runtime config input port

- ✗ runtime mux 浪费面积
- ✗ 实际用户不会运行时切换 latency

### D. 不拆分, 保持 `ICacheComplete` 是 0-cycle aread

- ✗ 违反 RV32I imem 必须 sread 的项目约定
- ✗ Fmax 较低

## Verification

- `cmake --build build`: 0 error
- `ctest -LE perf`: 149/149 PASS (含 9 个 cache test_case)
- `ctest -N`: CTEST_TOTAL = 160 (不变, 新增 test_case 在同 binary 内)

## Follow-up

- riscv-mini 集成: `ICachePrefetch` / `DCacheWriteBuffer` wrapper 验证 (handoff §3.4)
- ADR-039 §2.2 R2 spike closure 状态更新: cache-complete-rewrite 进入 "variant selection" 阶段
- ADR-037 stub 物理删除 (deadline 2027-04-17) — wrapper 验证后启动

## See also

- ADR-039 (cache-complete-rewrite via ch_mem) — parent decision
- ADR-037 (cache-stub-removal) — stub physical deletion timeline
- root AGENTS.md `MEM READ PORT` 段 — imem sread 约定
- openspec/specs/verilator-e2e-coverage/spec.md §R1 invariant 3 — imem sread 强制要求
- openspec/specs/mem-read-timing/spec.md — `aread`/`sread` 时序契约