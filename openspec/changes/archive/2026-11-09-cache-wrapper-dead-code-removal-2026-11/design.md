# Design: cache wrapper 死代码清零 + 地址稳定性契约

## 1. 为什么「只删」而不是「实现」

phase-6 **P9** 的出口判据 ① ② 是**二选一**：「删死成员」**或**「真实现」⇒ ⇒ 本 change 选
**删** ⇒ 理由：

- 真 FIFO 写缓冲 = **状态机 + 队列 + 背压语义**（`ready` 的语义要从「W 握手成立」改成
  「有空位」）⇒ ⇒ 那是**独立立项**（含 capability spec 变更与双后端 parity 覆盖），
  不属于「债务清零」；
- 真预取 = **地址预取策略 + 填充时机 + 命中路径改写** ⇒ ⇒ 同上；
- ⚠️ 本仓**当前没有任何调用方**需要缓冲 / 预取（`ICachePrefetch` 的消费者只把它当
  AXI 主端转发层用）⇒ ⇒ 现在实现 = **为想象中的需求写代码**（AI slop 的典型形态）。

⇒ ⇒ **真实现 = 另立 P 步 / 另立 improvement**（已写入 P9 的出口判据 ① ②）。

## 2. 删哪些 / 不删哪些（逐项裁决）

| 符号 | 裁决 | 理由 |
|---|---|---|
| `WriteBufferEntry`（公开 struct） | **删** | 零消费者；`(void)buffer` 掩盖了它 |
| `buffer[4]` + `(void)buffer` | **删** | 同上（死数据的本体） |
| `WriteBufferConfig`（公开 struct） | **删** | 只被 `buffer[4]` 引用 ⇒ 随之为死配置面 |
| `prefetch_data` / `prefetch_addr` / `prefetch_valid` | **删** | 零消费者（无读端口也无写端口） |
| `PrefetchConfig`（公开 struct） | **删** | 只被上三个引用 |
| `__io(...)` 签名 | **不动** | capability `cpu-cache-subsystem` 的
  「external IO signature unchanged」Requirement **逐字约束**它 ⇒ ⇒ 改它 = 契约变更 |
| wrapper 的**类名** | **不动** | 改名 = 公开 API 移除 + 改名（更大的登记面）⇒ ⇒
  「名字 over-promise」**维持登记**（`include/cpu/AGENTS.md` 债务段）⇒ ⇒ 不在本 change |

## 3. P14 登记（为什么「仓内零消费者」不够）

根 `AGENTS.md` ZERO-DEBT 的判决边界（2026-11-09 · issue #28）明写：「零『**仓内**』消费者的
『**公开**』符号**不算** dead code」⇒ ⇒ 删除 MUST 走「移除 / 改名登记」⇒ ⇒ 本 change 对
6 个符号全部登记（人读表 2 行 + 机器探针 6 行 + CHANGELOG 一行）。

## 4. 第三项债务（`miss` 与 `axi_addr` 1 拍错位）—— 怎么变成**可执行**契约

**实测**（内层 `ICacheSync` 的 `describe()`）：

- `miss` = `!hit`，而 `hit` 由 **`sread`** 出来的 tag / valid 推导 ⇒ ⇒ 对应
  **上一拍**采样的地址（`sread` = 1 拍注册读）；
- `axi_addr` = `icache_get_block_addr(io().addr)` = **当拍**地址的块地址。

⇒ ⇒ **`addr` 在 `miss` 期间变化 ⇒ 两者错位 1 拍** ⇒ ⇒ AXI 请求会跟**新地址**发出
⇒ ⇒ 取回的是**新块**的数据。

⇒ ⇒ **裁决 = 写成「调用方 MUST 在 `miss` 期间保持 `addr` 不变」的契约**（不修内层 cache）：
修它 = 改内层的时序（`sread` → 保留采样地址的寄存器）⇒ ⇒ 那是**内层 cache 本体**的
重设计（独立立项）⇒ ⇒ 本 change 只把「错位**可观测**」写成**可执行断言**：

- **判别性探针**（新增用例）：`miss` 期间改 `addr` ⇒ 断言 `axi_addr` == **当拍**地址的块地址
  ⇒ ⇒ 若有人把内层改成「跟随 `miss` 所指地址」⇒ ⇒ 该断言**转红** ⇒ ⇒ 它是「错位确实存在」
  的**可执行**证据（不是散文）。
- **契约载体**：`include/cpu/AGENTS.md` 债务段 + capability `cpu-cache-subsystem` 的
  **ADDED Requirement**（含正/负 Scenario）⇒ ⇒ **双载体**（人读 + 机器判）。

⚠️ **已知边界**：该契约**只**约束**调用方** ⇒ ⇒ 仓内唯一的调用方（RV32I 流水线的 imem）
MUST 遵守 ⇒ ⇒ **待办**：流水线的取指侧是否真的在 `miss` 期间保持 `pc` 不变 ⇒ ⇒
**不在本 change**（属流水线取指时序的独立核对）⇒ ⇒ 已登记为 backlog。

## 5. 判据（可复跑）

```bash
# A1 死代码清零（剥注释口径）
grep -rnE "WriteBufferEntry|WriteBufferConfig|PrefetchConfig|prefetch_data|prefetch_addr|prefetch_valid" include/cpu/cache/ | grep -vE "^[^:]+:[0-9]+: *(//|\*)" || echo "OK: 无代码引用"

# A2/A4 双后端用例
ctest -R riscv_mini_cache_smoke --output-on-failure

# A3 P14 登记
bash scripts/verify_public_api_visibility.sh

# A5 契约落库
bash scripts/verify_openspec_delta.sh
```
