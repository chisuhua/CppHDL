// tests/test_stream_demux_fork_dual.cpp
//
// `stream_demux`（ADR-056）与 `stream_fork`（ADR-057）的 **ready 语义** 的
// **双后端 parity** 覆盖。
//
// ⚠️ **动机**：这两个组件此前**零 dual 覆盖**，且它们在护栏 R8 里是**豁免**
//    状态（「方向修正是行为变更 ⇒ 需独立 ADR」/「同步语义从未工作过」）
//    ⇒ ⇒ **不在任何 ready 契约的覆盖范围内**。
//
// ⚠️⚠️ **判别性判据 MUST 是「默认（未接反压）时 `in_ready == 1`」**：
//    修复前 `input_stream.ready` 用 `operator=` 赋值 ⇒ 只重绑定**本地句柄**
//    ⇒ ⇒ **调用方持有的是孤儿节点 ⇒ 读 0**；而「(假) 同步语义下输出被挡住」时
//    它**也**是 0 ⇒ ⇒ 只断言「为 0」**不可区分**（假绿）⇒ ⇒ 必须断言**默认态为 1**。
//
// ⚠️ **接线纪律**（ADR-056 D5 / ADR-057 D5）：`in` MUST 先 `as_master()`
//    （⇒ 字段有节点）；**MUST NOT** 自行驱动 `in.ready`（由组件驱动，
//    否则触发三处响亮拒绝之一）。
//
// ⚠️ DUT 形态约束：Verilator 后端只暴露**顶层端口** ⇒ 所有被观测的信号
//    MUST 提到顶层 `ch_out`。

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ---------------------------------------------------------------------------
// DUT Ⓐ：`stream_demux<8, 3>`（⚠️ 3 路 ⇒ select 宽 2 ⇒ **可表达越界 3**）
// ---------------------------------------------------------------------------
class DemuxReadyDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_uint<2>> select;
         ch_in<ch_bool> r0;
         ch_in<ch_bool> r1;
         ch_in<ch_bool> r2;
         ch_out<ch_bool> in_ready;
         ch_out<ch_bool> o0_valid;
         ch_out<ch_bool> o1_valid;
         ch_out<ch_bool> o2_valid;
         ch_out<ch_uint<8>> o0_payload;)

    DemuxReadyDut(ch::Component *parent = nullptr,
                  const std::string &name = "demux_ready")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> in;
        in.as_master();                     // ADR-056 D5
        in.payload = io().in_payload;
        in.valid = ch_bool(io().in_valid.impl());

        auto d = stream_demux<ch_uint<8>, 3>(in, io().select);

        // 消费端施加反压（覆盖形态 E 的默认驱动）
        d.output_streams[0].ready <<= ch_bool(io().r0.impl());
        d.output_streams[1].ready <<= ch_bool(io().r1.impl());
        d.output_streams[2].ready <<= ch_bool(io().r2.impl());

        io().in_ready = in.ready;
        io().o0_valid = d.output_streams[0].valid;
        io().o1_valid = d.output_streams[1].valid;
        io().o2_valid = d.output_streams[2].valid;
        io().o0_payload = d.output_streams[0].payload;
    }
};

// ---------------------------------------------------------------------------
// DUT Ⓑ：`stream_fork<8, 2>`（synchronous = true）
// ---------------------------------------------------------------------------
class ForkSyncDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> r0;
         ch_in<ch_bool> r1;
         ch_out<ch_bool> in_ready;
         ch_out<ch_bool> o0_valid;
         ch_out<ch_bool> o1_valid;
         ch_out<ch_uint<8>> o0_payload;)

    ForkSyncDut(ch::Component *parent = nullptr,
                const std::string &name = "fork_sync")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> in;
        in.as_master();                     // ADR-057 D5
        in.payload = io().in_payload;
        in.valid = ch_bool(io().in_valid.impl());

        auto f = stream_fork<ch_uint<8>, 2>(in, true);
        f.output_streams[0].ready <<= ch_bool(io().r0.impl());
        f.output_streams[1].ready <<= ch_bool(io().r1.impl());

        io().in_ready = in.ready;
        io().o0_valid = f.output_streams[0].valid;
        io().o1_valid = f.output_streams[1].valid;
        io().o0_payload = f.output_streams[0].payload;
    }
};

}  // anonymous namespace

// ===========================================================================
// Ⓐ demux —— ready 方向 = 消费端 ⇒ 上游；非选中输出**隔离**；越界 ⇒ 挡住上游
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    DemuxReadyDut,
    "stream_demux - ready 由被选中输出的消费端驱动（非选中隔离 · 越界挡住上游）",
    "[chlib][stream][demux][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();

        auto rd = [&](auto &port) {
            return static_cast<uint64_t>(sim.get_port_value(port));
        };

        // ① **默认态**：消费端全不就绪也不驱动时…… 先给全就绪
        sim.set_input_value(io.r0, 1);
        sim.set_input_value(io.r1, 1);
        sim.set_input_value(io.r2, 1);
        sim.set_input_value(io.select, 0);
        sim.set_input_value(io.in_valid, 1);
        sim.set_input_value(io.in_payload, 0xAB);
        sim.tick();

        // 选中 0 ⇒ o0 valid；其余 0
        REQUIRE(rd(io.o0_valid) == 1u);
        REQUIRE(rd(io.o1_valid) == 0u);
        REQUIRE(rd(io.o2_valid) == 0u);
        REQUIRE(rd(io.o0_payload) == 0xABu);
        // 被选中输出就绪 ⇒ 上游就绪
        REQUIRE(rd(io.in_ready) == 1u);

        // ② 把**被选中**输出（index 0）拉低 ⇒ 上游 MUST 被挡住
        sim.set_input_value(io.r0, 0);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 0u);

        // ③ 恢复，再拉低**未选中**输出（index 1）⇒ 上游 MUST **不受影响**
        sim.set_input_value(io.r0, 1);
        sim.set_input_value(io.r1, 0);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 1u);

        // ④ select 切到 1 ⇒ 现在由 r1 决定 ⇒ r1 已低 ⇒ 上游挡住
        sim.set_input_value(io.select, 1);
        sim.tick();
        REQUIRE(rd(io.o1_valid) == 1u);
        REQUIRE(rd(io.in_ready) == 0u);

        // ⑤ **越界**（select = 3 ≥ 3 路）⇒ 全 valid = 0 **且** 上游挡住（ADR-056 D4）
        sim.set_input_value(io.select, 3);
        sim.tick();
        REQUIRE(rd(io.o0_valid) == 0u);
        REQUIRE(rd(io.o1_valid) == 0u);
        REQUIRE(rd(io.o2_valid) == 0u);
        REQUIRE(rd(io.in_ready) == 0u);
    });

// ===========================================================================
// Ⓑ fork —— `in_ready = AND(各输出 ready)`（**两面**）+ 数据面独立
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    ForkSyncDut,
    "stream_fork - in_ready = AND(各输出 ready)，且数据面不随 ready 变化",
    "[chlib][stream][fork][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();

        auto rd = [&](auto &port) {
            return static_cast<uint64_t>(sim.get_port_value(port));
        };

        sim.set_input_value(io.in_valid, 1);
        sim.set_input_value(io.in_payload, 0xA5);

        // ① **默认态**（两个消费端都就绪）⇒ 上游就绪
        //    ⚠️ 判别性：修复前 `input_stream.ready` 是**孤儿节点** ⇒ 恒 0
        sim.set_input_value(io.r0, 1);
        sim.set_input_value(io.r1, 1);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 1u);
        REQUIRE(rd(io.o0_valid) == 1u);
        REQUIRE(rd(io.o1_valid) == 1u);

        // ② **任一面**拉低 ⇒ 上游 MUST 被挡住（AND 的「全高才高」）
        sim.set_input_value(io.r0, 0);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 0u);

        // ③ **另一面**单独拉低同样成立（排除「只看第一个」的实现）
        sim.set_input_value(io.r0, 1);
        sim.set_input_value(io.r1, 0);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 0u);

        // ④ **数据面独立**：ready 为 0 时 valid / payload MUST 不变
        //    （ADR-057 D3：修复前 `valid` 被 `input.ready` 污染 ⇒ 会消失）
        REQUIRE(rd(io.o0_valid) == 1u);
        REQUIRE(rd(io.o1_valid) == 1u);
        REQUIRE(rd(io.o0_payload) == 0xA5u);

        // ⑤ 两面都恢复 ⇒ 上游恢复
        sim.set_input_value(io.r1, 1);
        sim.tick();
        REQUIRE(rd(io.in_ready) == 1u);
    });
