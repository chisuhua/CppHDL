// samples/minimal_fifo_bundle.cpp
//
// `fifo_bundle` 的**最小**用法示例（只演示 bundle 的接线与状态位）。
//
// ⚠️ **ADR-044 记录的两处已修正的问题**：
// ① `io.full = false; io.empty = true;` 是**节点重绑定**
//    （`bundle_base::operator=`）—— ADR-042 已把 `operator=` 定性为
//    「**不是**连接手段」。本 sample 改用 `<<=`（数据流边）。
// ② ADR-044 给 `fifo_bundle` 加了**独立的** `rdata`（读数据）字段
//    （master IN / slave OUT）⇒ 本 sample MUST **显式驱动**它
//    （否则新字段恒 0 ⇒ 一个**静默失效**的字段）。
#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

template <typename T, unsigned N> class SimpleFifo : public ch::Component {
public:
    static_assert((N & (N - 1)) == 0, "FIFO size must be power of 2");

    fifo_bundle<T> io;

    SimpleFifo(ch::Component *parent = nullptr,
               const std::string &name = "fifo")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void describe() override {
        // Simple implementation - just pass through for testing
        // ⚠️ MUST 用 `<<=`（数据流边）而不是 `=`（节点重绑定）——
        //    ADR-042 已把 `operator=` 定性为「不是连接手段」。
        io.full <<= ch_bool(false);
        io.empty <<= ch_bool(true);
        // ⚠️ `rdata`（读数据，ADR-044）**MUST 被显式驱动** —— 否则它恒 0，
        //    成为一个**静默失效**的字段。本 sample 不实现真正的 FIFO ⇒
        //    驱动成常量 0 并在注释里说明。
        io.rdata <<= T(0_d);
    }
};

// 顶层用**普通端口**承载 TB 接口，bundle 只用于**模块间**连接。
// ⚠️ 顶层用 bundle 的问题：bundle 成员**不会**被注册为组件 IO ⇒ 它的信号不会
// 成为生成 Verilog 的顶层端口（实测：改前生成的 fifo_bundle.v 顶层只有
// default_clock / default_reset）⇒ 外部 TB 无法驱动。
// 见 OpenSpec change `bundle-master-slave-io-2026-10` 的 D6。
class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<2>> data; ch_in<ch_bool> push; ch_in<ch_bool> pop;
         ch_out<ch_uint<2>> rdata;   // 读数据（ADR-044）
         ch_out<ch_bool> full; ch_out<ch_bool> empty;)

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleFifo<ch_uint<2>, 2>> fifo_inst{"fifo_inst"};
        fifo_inst.instance().io.push <<= io().push;
        fifo_inst.instance().io.pop <<= io().pop;
        fifo_inst.instance().io.data <<= io().data;
        // ⚠️ 读数据通路（ADR-044）：slave 侧的 `rdata` → 顶层输出端口
        io().rdata <<= fifo_inst.instance().io.rdata;
        io().full <<= fifo_inst.instance().io.full;
        io().empty <<= fifo_inst.instance().io.empty;
    }
};

int main() {
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());

    // Test basic Bundle functionality（顶层是普通端口）
    auto &io = top_device.instance().io();
    sim.set_input_value(io.data, 0);
    sim.set_input_value(io.push, 0);
    sim.set_input_value(io.pop, 0);

    std::cout << "Starting minimal FIFO Bundle test..." << std::endl;

    for (int cycle = 0; cycle < 3; ++cycle) {
        sim.tick();
        uint64_t full_val =
            static_cast<uint64_t>(sim.get_port_value(io.full));
        uint64_t empty_val =
            static_cast<uint64_t>(sim.get_port_value(io.empty));
        // ⚠️ `rdata` 是**读数据**（ADR-044）—— 本 sample 的 FIFO 是
        //    「直通占位」实现 ⇒ `rdata` 恒为 0（但**必须被驱动**，
        //    否则它会是一个**未被连接**的字段）。
        uint64_t rdata_val =
            static_cast<uint64_t>(sim.get_port_value(io.rdata));

        std::cout << "Cycle " << cycle << ": full=" << full_val
                  << ", empty=" << empty_val << ", rdata=0x" << std::hex
                  << rdata_val << std::dec << std::endl;
    }

    std::cout << "Minimal FIFO Bundle test completed successfully!"
              << std::endl;
    return 0;
}
