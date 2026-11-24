/**
 * @file test_axil_mmio_master.cpp
 * @brief `AxilMmioMaster` 的组件级判据（change `rv32i-soc-mmio-channel-2026-11` · P2）
 *
 * ## 判据的靶子（**不是**「跑通就算」）
 *
 * 1. **`ready` 是单拍完成脉冲**（T2.3）⇒ ⇒ 「一次事务**恰好一拍**」
 *    ⇒ ⇒ 电平实现（`ready = in_rd_resp`）会给出「事务时长」那么多拍
 *    ⇒ ⇒ 本文件的读用例把 `rvalid` 延迟 **4 拍** ⇒ ⇒ 电平实现得 **5 拍** ⇒ **必红**。
 * 2. **AW / W 相位分离**：`awready` 延迟 2 拍、`wready` 立即 ⇒ ⇒
 *    `wvalid` MUST 先撤、`awvalid` MUST **保持到自己的握手**（AXI 协议）。
 * 3. **VALID 期间地址 / 数据稳定**：`awaddr` / `wdata` / `wstrb` 在整个
 *    VALID 窗口内 MUST 等于锁存值（直接接输入端口会让它们随流水线漂移）。
 * 4. **`bready` / `rready` 恒 1** · `awprot` / `arprot` 恒 0（本核的 MMIO 语义）。
 * 5. **读数据保持**：完成之后 `prdata` MUST 仍可读（保持寄存器）。
 * 6. **复位**（高有效）：`rst` 拍之后 VALID 全撤。
 *
 * ## 期望值**手推**（禁抄仿真输出）
 *
 * 逐拍表由设计的三条规则推出：① `start = (state==IDLE) & preq`（**只在 IDLE 采样**）；
 * ② `awvalid = (state==WrAddr) & !aw_done`（握手当拍仍高、下一拍撤）；
 * ③ `ready = (state==WrResp) & bvalid | (state==RdResp) & rvalid`。
 * ⇒ ⇒ 每拍的期望值写在各用例的表里，对照的是**手推表**，不是回放。
 *
 * ## 时序模型（`eval_combinational` + `tick` 各一次）
 *
 * `set_input_value` → `eval_combinational`（**算本拍组合**）→ 采样输出 →
 * `tick`（**本拍沿**）。⇒ ⇒ 采样的是**本拍**的组合输出，不是下一拍
 * （`get_port_value` 只读 `data_map_` ⇒ ⇒ 不自己重算 ⇒ ⇒ 必须显式 `eval_combinational`）。
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "axil_mmio_master.h"

#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

/// 一拍的**全部**输入（主侧 + 从侧）
struct CycleIn {
    uint64_t preq = 0, pwe = 0, paddr = 0, pwdata = 0, pstrbe = 0;
    uint64_t awready = 0, wready = 0, bvalid = 0;
    uint64_t arready = 0, rvalid = 0, rdata = 0;
    uint64_t rst = 0;
};

/// 一拍的**全部**观测（组合输出）
struct CycleOut {
    uint64_t ready = 0, prdata = 0;
    uint64_t awaddr = 0, awprot = 0, awvalid = 0;
    uint64_t wdata = 0, wstrb = 0, wvalid = 0;
    uint64_t bready = 0;
    uint64_t araddr = 0, arprot = 0, arvalid = 0;
    uint64_t rready = 0;
};

/// 逐拍驱动 + 采样（`eval_combinational` 先于采样 ⇒ ⇒ 取到的是**本拍**组合值）
std::vector<CycleOut> run(ch::ch_device<AxilMmioMaster>& dut,
                          const std::vector<CycleIn>& script) {
    Simulator sim(dut.context());
    const auto& io = dut.instance().io();
    std::vector<CycleOut> out;
    out.reserve(script.size());
    for (const auto& c : script) {
        sim.set_input_value(io.preq, c.preq);
        sim.set_input_value(io.pwe, c.pwe);
        sim.set_input_value(io.paddr, c.paddr);
        sim.set_input_value(io.pwdata, c.pwdata);
        sim.set_input_value(io.pstrbe, c.pstrbe);
        sim.set_input_value(io.awready, c.awready);
        sim.set_input_value(io.wready, c.wready);
        sim.set_input_value(io.bvalid, c.bvalid);
        sim.set_input_value(io.arready, c.arready);
        sim.set_input_value(io.rvalid, c.rvalid);
        sim.set_input_value(io.rdata, c.rdata);
        sim.set_input_value(io.rst, c.rst);

        sim.eval_combinational();

        // ⚠️ `get_port_value` 返回 `sdata_type` ⇒ ⇒ MUST 显式转 `uint64_t`
        //    （隐式转换不存在 ⇒ ⇒ 直接赋值编译不过）
        auto get = [&](const auto& port) {
            return static_cast<uint64_t>(sim.get_port_value(port));
        };
        CycleOut o;
        o.ready   = get(io.ready);
        o.prdata  = get(io.prdata);
        o.awaddr  = get(io.awaddr);
        o.awprot  = get(io.awprot);
        o.awvalid = get(io.awvalid);
        o.wdata   = get(io.wdata);
        o.wstrb   = get(io.wstrb);
        o.wvalid  = get(io.wvalid);
        o.bready  = get(io.bready);
        o.araddr  = get(io.araddr);
        o.arprot  = get(io.arprot);
        o.arvalid = get(io.arvalid);
        o.rready  = get(io.rready);
        out.push_back(o);

        sim.tick();
    }
    return out;
}

uint64_t count_ready(const std::vector<CycleOut>& v) {
    uint64_t n = 0;
    for (const auto& o : v) n += o.ready;
    return n;
}

/// 相邻两拍都 ready 的次数（**单拍脉冲**的反向判据：电平实现会 > 0）
uint64_t adjacent_ready(const std::vector<CycleOut>& v) {
    uint64_t n = 0;
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i].ready && v[i - 1].ready) ++n;
    }
    return n;
}

constexpr uint64_t kGpioAddr  = 0x80102000ull;  // kGpio.base（DATA_OUT）
constexpr uint64_t kClintAddr = 0x80100000ull;  // kClint.base（读用例）
constexpr uint64_t kStoreData = 0xA5ull;
constexpr uint64_t kLoadData  = 0xDEADBEEFull;

}  // namespace

TEST_CASE("axil_mmio_master: 写事务 ⇒ VALID 序列 + ready 恰好一拍",
          "[axil-mmio-master]") {
    ch::ch_device<AxilMmioMaster> dut;
    // 从设备：awready / wready 恒 1；bvalid 在握手后**下一拍**给一拍
    std::vector<CycleIn> s(6);
    for (int i = 0; i < 6; ++i) { s[i].awready = 1; s[i].wready = 1; }
    s[0].preq = 1; s[0].pwe = 1; s[0].paddr = kGpioAddr;
    s[0].pwdata = kStoreData; s[0].pstrbe = 0xF;
    s[1] = s[0];
    s[2] = s[0]; s[2].bvalid = 1;
    s[3].preq = 0; s[3].pwe = 1;   // 事务已完成 ⇒ preq 撤（否则会起第二个事务）

    const auto o = run(dut, s);

    // 手推：cyc0 state=IDLE（start 采样当拍 ⇒ VALID 尚未发出）
    REQUIRE(o[0].awvalid == 0u);
    REQUIRE(o[0].wvalid == 0u);
    REQUIRE(o[0].ready == 0u);
    // 常量面：本核的 MMIO 语义（bready/rready 恒 1 · prot 恒 0）
    REQUIRE(o[0].bready == 1u);
    REQUIRE(o[0].rready == 1u);
    REQUIRE(o[0].awprot == 0u);
    REQUIRE(o[0].arprot == 0u);

    // cyc1 state=WrAddr ⇒ AW 与 W **同拍**发出，且地址 / 数据 / 字节道是**锁存值**
    REQUIRE(o[1].awvalid == 1u);
    REQUIRE(o[1].wvalid == 1u);
    REQUIRE(o[1].awaddr == kGpioAddr);
    REQUIRE(o[1].wdata == kStoreData);
    REQUIRE(o[1].wstrb == 0xFu);
    REQUIRE(o[1].ready == 0u);

    // cyc2 state=WrResp：VALID 已撤（握手完成），B 握手 ⇒ **一拍** ready
    REQUIRE(o[2].awvalid == 0u);
    REQUIRE(o[2].wvalid == 0u);
    REQUIRE(o[2].ready == 1u);

    // cyc3 回到 IDLE ⇒ 不再 ready
    REQUIRE(o[3].ready == 0u);

    REQUIRE(count_ready(o) == 1u);
    REQUIRE(adjacent_ready(o) == 0u);
}

TEST_CASE("axil_mmio_master: awready 延迟 ⇒ awvalid 保持 · wvalid 先撤（相位分离）",
          "[axil-mmio-master]") {
    ch::ch_device<AxilMmioMaster> dut;
    std::vector<CycleIn> s(8);
    for (int i = 0; i < 8; ++i) { s[i].wready = 1; }
    s[0].preq = 1; s[0].pwe = 1; s[0].paddr = kGpioAddr;
    s[0].pwdata = kStoreData; s[0].pstrbe = 0xF;
    s[1] = s[0];                 // awready 仍 0（从设备没准备好）
    s[2] = s[0]; s[2].awready = 1;  // 本拍 AW 握手完成
    // ⚠️ **AXI 稳定性判据**：AWVALID 仍高的这一拍把 `paddr` 改掉 ⇒ ⇒
    //    `awaddr` MUST **仍是锁存值**（直接 `awaddr = io().paddr` 的实现在这里必红）
    s[2].paddr = 0xDEAD0000ull;
    s[3] = s[2]; s[3].bvalid = 1;   // 两握手都完成后的下一拍给 B
    s[4].preq = 0; s[4].pwe = 1; s[4].awready = 1; s[4].wready = 1;

    const auto o = run(dut, s);

    // 手推：cyc1 起进入 WrAddr；awready=0 ⇒ AW 不握手
    REQUIRE(o[1].awvalid == 1u);
    REQUIRE(o[1].wvalid == 1u);
    // cyc1 末：W 已握手（wready=1）⇒ cyc2 的 wvalid MUST 撤，而 awvalid MUST 仍在
    REQUIRE(o[2].awvalid == 1u);
    REQUIRE(o[2].wvalid == 0u);
    // VALID 窗口内地址 / 数据 MUST 稳定（锁存寄存器）——
    // 本拍的 `paddr` 已被改成 0xDEAD0000 ⇒ ⇒ `awaddr` MUST 仍是 kGpioAddr
    REQUIRE(o[2].awaddr == kGpioAddr);
    REQUIRE(o[2].wdata == kStoreData);
    REQUIRE(o[2].ready == 0u);
    // cyc3：AW 已握手 ⇒ state=WrResp ⇒ B 握手 ⇒ 一拍 ready
    REQUIRE(o[3].awvalid == 0u);
    REQUIRE(o[3].ready == 1u);
    REQUIRE(o[4].ready == 0u);

    REQUIRE(count_ready(o) == 1u);
    REQUIRE(adjacent_ready(o) == 0u);
}

TEST_CASE("axil_mmio_master: 读事务 rvalid 延迟 4 拍 ⇒ ready MUST 仍恰好一拍",
          "[axil-mmio-master]") {
    ch::ch_device<AxilMmioMaster> dut;
    std::vector<CycleIn> s(10);
    for (int i = 0; i < 10; ++i) { s[i].arready = 1; }
    s[0].preq = 1; s[0].pwe = 0; s[0].paddr = kClintAddr;
    for (int i = 1; i <= 6; ++i) s[i] = s[0];
    s[5].rvalid = 1; s[5].rdata = kLoadData;   // 手推：AR 在 cyc1 握手 ⇒ R 在 cyc5
    s[7].preq = 0; s[7].pwe = 0; s[7].arready = 1;

    const auto o = run(dut, s);

    // cyc0：IDLE（采样请求）
    REQUIRE(o[0].arvalid == 0u);
    // cyc1：RdAddr ⇒ AR 发出，地址是**锁存值**
    REQUIRE(o[1].arvalid == 1u);
    REQUIRE(o[1].araddr == kClintAddr);
    REQUIRE(o[1].arprot == 0u);
    // cyc2：AR 已握手 ⇒ ARVALID 撤，等 R
    REQUIRE(o[2].arvalid == 0u);
    REQUIRE(o[2].ready == 0u);
    // ⚠️ 核心判据：R 没来的每一拍都 MUST NOT ready（电平实现会连续拉高）
    for (int i = 2; i <= 4; ++i) {
        REQUIRE(o[i].ready == 0u);
    }
    // cyc5：R 握手 ⇒ **一拍** ready
    REQUIRE(o[5].ready == 1u);
    // cyc6 / cyc7：已回 IDLE ⇒ ready 撤；**读数据保持**
    REQUIRE(o[6].ready == 0u);
    REQUIRE(o[6].prdata == kLoadData);
    REQUIRE(o[7].prdata == kLoadData);

    REQUIRE(count_ready(o) == 1u);
    REQUIRE(adjacent_ready(o) == 0u);
}

TEST_CASE("axil_mmio_master: 两个事务 ⇒ 两个脉冲（不合并、不重复）",
          "[axil-mmio-master]") {
    ch::ch_device<AxilMmioMaster> dut;
    std::vector<CycleIn> s(12);
    for (int i = 0; i < 12; ++i) { s[i].awready = 1; s[i].wready = 1; }
    // 事务 A：cyc0 起
    s[0].preq = 1; s[0].pwe = 1; s[0].paddr = kGpioAddr;
    s[0].pwdata = 0x11; s[0].pstrbe = 0xF;
    s[1] = s[0];
    s[2] = s[0]; s[2].bvalid = 1;
    s[3].pwe = 1; s[3].paddr = kGpioAddr;          // preq=0 ⇒ 空隙
    // 事务 B：cyc5 起
    s[5].preq = 1; s[5].pwe = 1; s[5].paddr = kGpioAddr;
    s[5].pwdata = 0x22; s[5].pstrbe = 0xF;
    s[6] = s[5];
    s[7] = s[5]; s[7].bvalid = 1;
    s[8].pwe = 1; s[8].paddr = kGpioAddr;

    const auto o = run(dut, s);

    REQUIRE(o[2].ready == 1u);          // A 完成
    REQUIRE(o[3].ready == 0u);
    REQUIRE(o[6].wdata == 0x22u);       // B 的数据（锁存，不是残留 A）
    REQUIRE(o[7].ready == 1u);          // B 完成
    REQUIRE(count_ready(o) == 2u);
    REQUIRE(adjacent_ready(o) == 0u);
}

TEST_CASE("axil_mmio_master: 复位拍之后 VALID 全撤", "[axil-mmio-master]") {
    ch::ch_device<AxilMmioMaster> dut;
    std::vector<CycleIn> s(6);
    for (int i = 0; i < 6; ++i) { s[i].awready = 1; s[i].wready = 1; }
    s[0].preq = 1; s[0].pwe = 1; s[0].paddr = kGpioAddr;
    s[0].pwdata = kStoreData; s[0].pstrbe = 0xF;
    s[1] = s[0];                 // 进入 WrAddr（VALID 已发出）
    s[2] = s[0]; s[2].rst = 1;   // 复位拍
    s[3].preq = 0; s[3].pwe = 1; s[3].awready = 1; s[3].wready = 1;
    s[4] = s[3];
    s[5] = s[3];

    const auto o = run(dut, s);

    // 复位**前**：cyc2 的组合输出仍是 WrAddr 的（复位在沿上生效）
    REQUIRE(o[1].awvalid == 1u);
    // 复位**后**（cyc3 起）：state=IDLE 且 preq=0 ⇒ 一切撤销
    REQUIRE(o[3].awvalid == 0u);
    REQUIRE(o[3].wvalid == 0u);
    REQUIRE(o[3].arvalid == 0u);
    REQUIRE(o[3].ready == 0u);
    REQUIRE(count_ready(o) == 0u);
}
