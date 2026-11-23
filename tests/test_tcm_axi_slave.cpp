// 测试：`axi4::TcmAxiSlave`（AXI4 full slave 包装 TCM）
//
// ## 判据来源（EARS）
//
// `openspec/changes/rv32i-dcache-burst-2026-11/specs/rv32i-dcache-timing/spec.md` 的第 3 个
// Requirement（line 填充 MUST 用 4-beat burst）的**对端**：slave 必须能吃下 burst。
//
// ## 手推期望值（**禁抄仿真输出**）
//
// 写：`AWLEN=3`（4 beat），地址 0x0000/0x0004/0x0008/0x000C，值 0x11111111/0x22222222/
//     0x33333333/0x44444444 ⇒ TCM 的字地址 0/1/2/3。
// 读：`ARLEN=3` 从 0x0000 起 ⇒ 4 个 beat MUST 依次回上面 4 个值，且第 4 个 `r_last=1`。
//
// ⚠️ **双后端**（`DUAL_BACKEND_TEST_PAIR`）：本组件是「纯组合 + 1 拍 `sread`」的
//    时序逻辑 ⇒ 正是两后端最易分叉的形态。

#include <catch_amalgamated.hpp>

#include <cstdint>

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "dual_backend_fixture.h"

using namespace ch::core;

#include "axi4/tcm_axi_slave.h"
#include "cpu/pipeline/rv32i_tcm.h"

namespace {

constexpr unsigned kAddrW = 32;
constexpr unsigned kDataW = 32;
constexpr unsigned kTcmAw = 12;

/// DUT = `TcmAxiSlave` + `DataTCM` 的固定接线
class TcmSlaveDut : public ch::Component {
public:
    __io(
        // AX 侧（demo 只需 AR/R/AW/W/B 的最小面）
        ch_in<ch_uint<kAddrW>>  ar_addr;
        ch_in<ch_uint<4>>       ar_len;
        ch_in<ch_uint<3>>       ar_size;
        ch_in<ch_uint<2>>       ar_burst;
        ch_in<ch_bool>          ar_valid;
        ch_out<ch_bool>         ar_ready;

        ch_out<ch_uint<kDataW>> r_data;
        ch_out<ch_bool>         r_last;
        ch_out<ch_bool>         r_valid;
        ch_in<ch_bool>          r_ready;

        ch_in<ch_uint<kAddrW>>  aw_addr;
        ch_in<ch_uint<4>>       aw_len;
        ch_in<ch_uint<3>>       aw_size;
        ch_in<ch_uint<2>>       aw_burst;
        ch_in<ch_bool>          aw_valid;
        ch_out<ch_bool>         aw_ready;

        ch_in<ch_uint<kDataW>>  w_data;
        ch_in<ch_uint<4>>       w_strb;
        ch_in<ch_bool>          w_last;
        ch_in<ch_bool>          w_valid;
        ch_out<ch_bool>         w_ready;

        ch_out<ch_bool>         bvalid;
        ch_in<ch_bool>          bready;
    )

    TcmSlaveDut(ch::Component* parent = nullptr, const std::string& name = "dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<axi4::TcmAxiSlave<kAddrW, kDataW, kTcmAw>> slave{"slave"};
        ch::ch_module<riscv::DataTCM<kTcmAw, kDataW>> tcm{"tcm"};

        // AX → slave
        slave.io().ar_addr  <<= io().ar_addr;
        slave.io().ar_id    <<= ch_uint<4>(0_d);
        slave.io().ar_len   <<= io().ar_len;
        slave.io().ar_size  <<= io().ar_size;
        slave.io().ar_burst <<= io().ar_burst;
        slave.io().ar_valid <<= io().ar_valid;
        slave.io().r_ready  <<= io().r_ready;

        slave.io().aw_addr  <<= io().aw_addr;
        slave.io().aw_id    <<= ch_uint<4>(0_d);
        slave.io().aw_len   <<= io().aw_len;
        slave.io().aw_size  <<= io().aw_size;
        slave.io().aw_burst <<= io().aw_burst;
        slave.io().aw_valid <<= io().aw_valid;

        slave.io().w_data   <<= io().w_data;
        slave.io().w_strb   <<= io().w_strb;
        slave.io().w_last   <<= io().w_last;
        slave.io().w_valid  <<= io().w_valid;
        slave.io().bready   <<= io().bready;

        // slave ↔ TCM
        tcm.io().addr  <<= slave.io().tcm_addr;
        tcm.io().write <<= slave.io().tcm_write;
        tcm.io().wdata <<= slave.io().tcm_wdata;
        tcm.io().strbe <<= slave.io().tcm_strbe;
        tcm.io().valid <<= slave.io().tcm_valid;
        slave.io().tcm_ready <<= tcm.io().ready;
        slave.io().tcm_rdata <<= tcm.io().rdata;

        // slave → AX
        io().ar_ready <<= slave.io().ar_ready;
        io().r_data   <<= slave.io().r_data;
        io().r_last   <<= slave.io().r_last;
        io().r_valid  <<= slave.io().r_valid;
        io().aw_ready <<= slave.io().aw_ready;
        io().w_ready  <<= slave.io().w_ready;
        io().bvalid   <<= slave.io().bvalid;
    }
};

// ⚠️ 手推期望（**禁抄仿真输出**）：地址 0x0/0x4/0x8/0xC ⇒ 4 个字。
//    ⚠️ MUST 放在宏**外面** —— 宏实参里的花括号 `{a, b, c}` 中的逗号会被
//    预处理器当成**实参分隔符**（实测：报 "passed 10 arguments, but takes just 4"）。
constexpr uint64_t kTcmAddrs[4] = {0x0, 0x4, 0x8, 0xC};
constexpr uint64_t kTcmVals[4]  = {0x11111111ull, 0x22222222ull,
                                   0x33333333ull, 0x44444444ull};

} // namespace

DUAL_BACKEND_TEST_PAIR(
    TcmSlaveDut,
    "TcmAxiSlave: AWLEN=3 写 4 字，ARLEN=3 读回同样 4 字（手推）",
    "[axi4][tcm-slave][burst][dual]",
    [&]() {
        auto& io = dev.instance().io();

        // 默认态
        sim.set_input_value(io.ar_valid, 0); sim.set_input_value(io.ar_addr, 0);
        sim.set_input_value(io.ar_len, 0);  sim.set_input_value(io.ar_size, 2);
        sim.set_input_value(io.ar_burst, 1); sim.set_input_value(io.r_ready, 1);
        sim.set_input_value(io.aw_valid, 0); sim.set_input_value(io.aw_addr, 0);
        sim.set_input_value(io.aw_len, 0);  sim.set_input_value(io.aw_size, 2);
        sim.set_input_value(io.aw_burst, 1);
        sim.set_input_value(io.w_valid, 0); sim.set_input_value(io.w_data, 0);
        sim.set_input_value(io.w_strb, 0xF); sim.set_input_value(io.w_last, 0);
        sim.set_input_value(io.bready, 1);

        // ---- 写 burst：AWLEN=3 ----
        sim.set_input_value(io.aw_addr, 0);
        sim.set_input_value(io.aw_len, 3);
        sim.set_input_value(io.aw_valid, 1);
        sim.tick();
        sim.set_input_value(io.aw_valid, 0);
        for (int b = 0; b < 4; ++b) {
            sim.set_input_value(io.w_valid, 1);
            sim.set_input_value(io.w_data, kTcmVals[b]);
            sim.set_input_value(io.w_last, b == 3 ? 1 : 0);
            sim.tick();
        }
        sim.set_input_value(io.w_valid, 0);
        sim.set_input_value(io.w_last, 0);
        // B 握手
        sim.tick();

        // ---- 读 burst：ARLEN=3 ----
        sim.set_input_value(io.ar_addr, 0);
        sim.set_input_value(io.ar_len, 3);
        sim.set_input_value(io.ar_valid, 1);
        sim.tick();
        sim.set_input_value(io.ar_valid, 0);

        // 4 个 beat：`sread` 1 拍延迟 ⇒ 每个 beat 至少 1 拍
        int got = 0;
        for (int guard = 0; guard < 32 && got < 4; ++guard) {
            if (static_cast<uint64_t>(sim.get_port_value(io.r_valid)) != 0) {
                INFO("beat " << got << " addr=0x" << std::hex << kTcmAddrs[got]);
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r_data)) == kTcmVals[got]);
                REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r_last)) ==
                        (got == 3 ? 1u : 0u));
                ++got;
            }
            sim.tick();
        }
        REQUIRE(got == 4);
    });
