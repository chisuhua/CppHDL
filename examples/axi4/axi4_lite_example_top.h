// examples/axi4/axi4_lite_example_top.h
//
// AXI4-Lite 示例的**顶层组件**（从 `examples/axi4/tb_axi4_lite_example.cpp` 抽出）。
//
// ⚠️ **住址变更**（2026-11-18 · `example-file-boundary-2026-11` A1/A2）：本头**从
//   `include/axi4/` 搬到 `examples/axi4/`** ⇒ ⇒ 理由 = 它是 **example 专属设计**
//   （库 `include/` 不装 example 设计）⇒ ⇒ 公开面移除登记 =
//   `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章。
//   ⇒ ⇒ 消费者**只有** `examples/**`（护栏 C3 禁止 `tests/**` 反向 include examples）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1 · 20 个）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。

#pragma once

#include "axi4/axi4_lite_slave.h"
#include "ch.hpp"
#include "component.h"

#include <memory>

#include <string>

namespace axi4 {
namespace example {

using namespace ch::core; // ch_uint / ch_bool / ch_reg / select / 0_d 等

/// **根组件名**（**单一真相源** · 2026-11-18）—— 发射 factory 与验证模式的
/// 根**共用**此常量 ⇒ ⇒ 消除「字面量恰好一致」的巧合。
inline constexpr const char *kLiteRootName = "top";

class AxiLiteTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> awaddr{"awaddr"};
        ch_in<ch_uint<2>> awprot{"awprot"};
        ch_in<ch_bool> awvalid{"awvalid"};
        ch_out<ch_bool> awready{"awready"};

        ch_in<ch_uint<32>> wdata{"wdata"};
        ch_in<ch_uint<4>> wstrb{"wstrb"};
        ch_in<ch_bool> wvalid{"wvalid"};
        ch_out<ch_bool> wready{"wready"};

        ch_out<ch_uint<2>> bresp{"bresp"};
        ch_out<ch_bool> bvalid{"bvalid"};
        ch_in<ch_bool> bready{"bready"};

        ch_in<ch_uint<32>> araddr{"araddr"};
        ch_in<ch_uint<2>> arprot{"arprot"};
        ch_in<ch_bool> arvalid{"arvalid"};
        ch_out<ch_bool> arready{"arready"};

        ch_out<ch_uint<32>> rdata{"rdata"};
        ch_out<ch_uint<2>> rresp{"rresp"};
        ch_out<ch_bool> rvalid{"rvalid"};
        ch_in<ch_bool> rready{"rready"};

        // 复位（低有效，在时钟沿生效）—— AxiLiteSlave 的必需端口
        ch_in<ch_bool> rst_n{"rst_n"};
    )

    AxiLiteTop(ch::Component *parent = nullptr, const std::string &name = "axi_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<AxiLiteSlave<32, 32, 4>> slave{"slave"};

        slave.io().awaddr <<= io().awaddr;
        slave.io().awprot <<= io().awprot;
        slave.io().awvalid <<= io().awvalid;
        io().awready <<= slave.io().awready;

        slave.io().wdata <<= io().wdata;
        slave.io().wstrb <<= io().wstrb;
        slave.io().wvalid <<= io().wvalid;
        io().wready <<= slave.io().wready;

        io().bresp <<= slave.io().bresp;
        io().bvalid <<= slave.io().bvalid;
        slave.io().bready <<= io().bready;

        slave.io().araddr <<= io().araddr;
        slave.io().arprot <<= io().arprot;
        slave.io().arvalid <<= io().arvalid;
        io().arready <<= slave.io().arready;

        io().rdata <<= slave.io().rdata;
        io().rresp <<= slave.io().rresp;
        io().rvalid <<= slave.io().rvalid;
        slave.io().rready <<= io().rready;

        slave.io().rst_n <<= io().rst_n;
    }
};

} // namespace example
} // namespace axi4
