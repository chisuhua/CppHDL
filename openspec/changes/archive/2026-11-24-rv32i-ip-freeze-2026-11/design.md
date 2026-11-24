# design · `riscv-mini` IP 冻结（判据完备）

## 1. T1：核级 MMIO 判据的载体与观测面

### 1.1 为什么不用新端口也能判「读值」

`Rv32iCoreTop` 的根输出里有 **`obs_data_write_data`（端口 #9）** 与 `obs_data_write_en`（#10）。
⇒ ⇒ 用「**存回去**」把读值变成可观测量：

```
lui  x4, 0x80102     ; x4 = 0x8010_2000（kGpio.base，MMIO 段）
lw   x6, 4(x4)       ; MMIO 读 ⇒ stub 回 rdata = 0x5A
addi x7, x0, 1       ; **非 MMIO 后继**（其 EX/MEM 地址 = 1 ⇒ 逼出「读数据 mux 错拍」）
sw   x6, 0(x4)       ; **MMIO 写**到 0x8010_2000 ⇒ 观测该拍的 store 数据即证明读值正确
jal  x0, 0
```

⇒ ⇒ **判据 1（AC1）**：存在一拍 `obs_data_write_en == 1 且 obs_data_addr == 0x8010_2000`
⇒ 该拍 `obs_data_write_data == 0x5A`。
⇒ ⇒ **判据 2（AC2）**：store 事件总数 == 1 · 写回总数 == 3。

> ⚠️ **2026-11-24 · 实施期订正（critic 轮次 1 的 BLOCKER）**：本节初稿的 store 是
> `sw x6, 0(x0)`（写 **D-TCM 地址 0**）—— 实施时改用 `sw x6, 0(x4)`（**MMIO 写**），
> 理由：① 与 SoC 级 A9 的同一条程序**逐字一致**（两处期望值可以互相对照）
> ② 同时覆盖**写通路**（`sw` 也走 MMIO ⇒ 读写两个方向的通道都被这条程序驱动）。
> ⇒ ⇒ 旧稿的「地址 0 的 store」在本程序里**不存在**（地址 0 的 store 属 AC5 的非 MMIO 程序,
> 数据 53）⇒ ⇒ 按旧稿验收会失败 ⇒ ⇒ 已同步修正。

### 1.2 stub 从设备的时序模型（**2026-11-24 · 实施期订正**）

测试用 `sim.eval_combinational()` + `sim.tick()` 的**逐拍**模型（与 `test_axil_mmio_master` 同款）：

| 输入 | 规则 |
|---|---|
| `awready` / `wready` / `arready` | **每个事务独立**：「观察到该请求」后的第 `ready_lat + 1` 拍起拉高（请求线全撤 ⇒ 下一事务重新计时）|
| `bvalid` | 观察到 AW 与 W **都**握手后，下一拍给 1 拍；`bresp` = 配置值 |
| `rvalid` | 观察到 AR 握手后，下一拍给 1 拍 |
| `rdata` | **只在响应拍**有效（= 配置值）⇒ ⇒ 其余拍给**每拍都变**的垃圾值（AXI：`rdata` 仅在 `rvalid=1` 时有效）|
| `dead = true` | **永不应答**（ready 恒 0 · valid 恒 0）⇒ ⇒ **反证档** |

⚠️ **订正说明**：初稿写的是「复位释放后第 `lat` 拍起恒 1」⇒ ⇒ **不可实现** ——
从设备 MUST 先**观察**到请求才能应答 ⇒ ⇒ 它的 ready 天然比请求晚（≥ 1 拍）⇒ ⇒
「以全局拍数为准」会与「请求何时发出」耦合（而请求发出的拍本身是实现相关的）⇒ ⇒
改为「**相对首次观察到的请求**」⇒ ⇒ 模型与实现的时序**无关**。

### 1.3 为什么 AC3 取「**反证**」形态而不是「事务拍数上界」

- 初稿形态（「完成 ≤ 手推上界」）的**缺陷**：上界必须由「master 的状态机拍数」推出
  ⇒ ⇒ **把实现时序写进判据**（改一发即红）⇒ ⇒ 违反「期望值不得依赖实现时序」。
- 实施期改定的形态（**两侧夹逼**）：
  - **正向**：从设备**应答** ⇒ 程序**完成**（store 提交 + 写回达标）；
  - **反向（反证）**：从设备**永不应答** ⇒ 程序**永不完成**（store 不提交 · 写回不足）⇒ ⇒
    ⇒ ⇒ 证明正向的「完成」**不是「核自说自话」** ⇒ ⇒ 证明核**真的在等从设备**。
- ⇒ ⇒ 这条反证**与 master 的内部拍数无关** ⇒ ⇒ 既有判别力，又不锁实现时序。
- ⇒ ⇒ 另加**协议级**判据（VALID 保持到握手 · VALID 期间载荷稳定 · 响应不早于请求）
  ⇒ ⇒ 覆盖 master 的行为契约，但不写死拍数。

## 2. T2：19 根 `axil_*` 纳入 flat≡hier 逐拍等价

- 复用 `ch::hier_eq`（`include/hierarchy_equivalence.h`）的既有 harness：`DrivenInput[]` + `Observation[]` + `stim[][]`。
- **新增 8 根输入**（`awready` / `wready` / `bresp` / `bvalid` / `arready` / `rdata` / `rresp` / `rvalid`）
  ⇒ 驱动成「**恒快从设备**」：`awready=wready=arready=1` · `bvalid=rvalid=1` · `rdata=0x5A` · `bresp=rresp=0`
  ⇒ ⇒ 这是**等价性**用例（不是协议合规用例）：两侧收到**同一组**激励 ⇒ 输出逐拍必须相同。
- **新增 11 根输出观测**（`awaddr` / `awprot` / `awvalid` / `wdata` / `wstrb` / `wvalid` / `bready` /
  `araddr` / `arprot` / `arvalid` / `rready`）⇒ ⇒ 程序含 MMIO 访问 ⇒ 这些端口**不是恒定量**。
- **变异判别力**：hier 产物里把 `assign top_axil_awaddr = …` 的 RHS 改掉（同 `test_rv32i_hier_rtl` 既有篡改手法）
  ⇒ ⇒ 该用例 MUST 转红。

## 3. 「先见红」的落法

IP 侧的新判据有两条必须证明「不是恒绿」：

| 变异 | 内容 | 实测结果 |
|---|---|---|
| **M8** | 读数据 mux 退回 S4a 缺陷形态（选择线用**当拍** `addr_in_mmio`） | **3 用例转红**（AC1 / AC2 / AC4）|
| **M9** | 分流方向反转（`addr_in_mmio` 取补） | **6 用例全红** |
| **M-A** | master 的 `rdata` 在 `ar_fire` 就锁存（早于 `rvalid`） | **AC1 转红**（靠「`rdata` 只在响应拍有效」的 stub）|
| **M-B** | 写完成在 AW∧W 握手即报 `ready`（不等 `bvalid`） | **AC6f 转红**（完成事件早于 B 响应）|
| **T2** | hier 产物篡改 `assign top_axil_awaddr = …` | 等价性**转红**（`equal==false`）|

> ⚠️ **2026-11-24 · 实施期订正**：初稿的这一行是「AC3 = 把 stub 的 `rvalid` 延迟拉到 > 上界
> ⇒ 期望 AC3 红」—— AC3 已改为**反证档**（§1.3），该变异路径**不存在**；
> 现表只列**实测过**的变异（M-A / M-B 是 critic 轮次 1 指出的二阶逃逸面，均已实测转红）。

## 4. T3：IP 集成契约段的内容（写进 `examples/riscv-mini/AGENTS.md`）

1. **端口面**：53 根（框架 2 + 核自有 51 = 12 IN + 39 OUT）· 单一真相 = 本文件端口表
2. **复位**：`ext_rst` **高有效**；核内 master 的 `rst` 与之同极性；框架 `default_reset` 另有一条（ADR-046）
3. **MMIO 通道**：19 根 AXI4-Lite 主口（无 ID / 无 burst）· 分流判据 = 地址 ∈ **MMIO 段**（派生自库层表）
4. **事务语义**：核内 `ready` 是**单拍完成脉冲**（与事务时长无关）；从设备**不应答** ⇒ 核**一直等**（不超时、不自造响应）
5. **错误语义（现状）**：`bresp` / `rresp` **不消费** ⇒ DECERR 表现为「事务完成、数据取回」⇒ 软件无法区分（**S5** 补）
6. **`irq`**：**无**（G-5 契约已定 · 实施在 S5）⇒ ⇒ **IP v1 = polling-only**
7. **已知限制**：数据侧 TCM 越界**静默别名**（登记 `rv32i_mem_map.h`）· MMIO 段内未接的外设由 SoC 侧互联回 DECERR

## 5. 风险与替代方案

| 方案 | 结论 |
|---|---|
| 加 `obs_mmio_*` 观测端口来判读值 | ❌ 动端口面 ⇒ C6/契约表/V1/R8′ 全要重算；且 `obs_data_write_data` 已够用 ⇒ ⇒ 选「存回去」 |
| 只做 SoC 级判据（不补 IP 级） | ❌ 「IP 可复用」的前提是**自己的接口**被单独验证；换 SoC 后无底 |
| 用响应式 stub 做 hier 等价 | ❌ `hier_eq` 的激励是**预先算好的** `stim[][]`，不能逐拍反应 ⇒ ⇒ 改用「恒快从设备」的开放环激励（等价性用例足够） |