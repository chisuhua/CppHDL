// tests/test_stream_connect_dual.cpp
//
// `ch_stream` 的**自由 `operator<<=`** 连接路径的**双后端 parity** 覆盖。
//
// ⚠️ 动机（capability `dual-backend-test-coverage` 的 T4）：
//    `ch_stream` 已在双后端矩阵里（`tests/test_ch_stream.cpp` 的 2 个
//    `DUAL_BACKEND_TEST_PAIR`）—— 但那两处覆盖的是「**裸端口**的数据流」，
//    **没有**覆盖「**两个 `ch_stream` 之间**的连接」⇒ 而后者正是本仓库里
//    **唯一**走**自由 `operator<<=`** 的路径。
//
//    该路径此前是**节点重绑定**语义（`sink.valid = source.valid`）⇒ 与其它
//    bundle 的连接语义**不一致**（其它 bundle 走数据流边）⇒ 已在
//    `bundle-ecosystem-hardening-2026-10` 的 D6 收敛为数据流边。
//
//    ⇒ 本文件是那次收敛的**双后端**回归护栏：interpreter 与 verilator
//      MUST 给出**相同**的结果。
//
// （OpenSpec change `bundle-ecosystem-hardening-2026-10` 的 D6 阶段 2；
//   capability `bundle-master-slave-io` 的「连接运算符语义 MUST 唯一」）

#include "catch_amalgamated.hpp"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream_operators.h"
#include "component.h"
#include "core/context.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

// ---------------------------------------------------------------------------
// DUT：两个 `ch_stream` 之间的连接
//
// ⚠️ 顶层只暴露**窄**端口（8/1 位）⇒ 满足 ADR-035 R3 的 64-bit 端口上限。
//    （`ch_stream<ch_uint<8>>` 的总宽是 10 bit ⇒ 即便整体暴露也不超限，
//      但逐字段暴露更贴近真实用法。）
// ---------------------------------------------------------------------------
class StreamConnectDualDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> src_payload; ch_in<ch_bool> src_valid;
         ch_out<ch_uint<8>> dst_payload; ch_out<ch_bool> dst_valid;)

    StreamConnectDualDut(ch::Component *parent = nullptr,
                         const std::string &name = "stream_connect_dual")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> src;
        ch_stream<ch_uint<8>> dst;
        src.as_master();
        dst.as_slave();

        // 源由 TB 驱动
        src.payload <<= io().src_payload;
        src.valid <<= io().src_valid;

        // ← 被测的连接（**自由** `operator<<=` ⇒ 数据流边）
        dst <<= src;

        // 观测
        io().dst_payload <<= dst.payload;
        io().dst_valid <<= dst.valid;
    }
};

// ---------------------------------------------------------------------------
// parity：两个后端 MUST 给出相同的值
// ---------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    StreamConnectDualDut,
    "Stream Connect - 自由 operator<<= 的双后端 parity",
    "[stream][operators][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().src_payload, 0x5A);
        sim.set_input_value(dev.instance().io().src_valid, 1);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().dst_payload)) == 0x5Au);
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().dst_valid)) == 1u);
    });

// ---------------------------------------------------------------------------
// parity：源的值变化 MUST 逐拍传播（不是「一次性的重绑定」）
// ---------------------------------------------------------------------------
DUAL_BACKEND_TEST_PAIR(
    StreamConnectDualDut,
    "Stream Connect - 自由 operator<<= 逐拍传播",
    "[stream][operators][dual]",
    [&]() {
        for (uint64_t v = 1; v <= 4; ++v) {
            sim.set_input_value(dev.instance().io().src_payload, v);
            sim.set_input_value(dev.instance().io().src_valid, 1);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(dev.instance().io().dst_payload)) == v);
        }
    });
