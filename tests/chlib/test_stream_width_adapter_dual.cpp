// ⚠️⚠️⚠️ **ADR-064 的行为判据 —— chlib stream 宽度适配器（双后端 parity）**（2026-11-03）
//
// ## 为什么要这个文件
//
// `tests/test_stream_width_adapter.cpp` 有 **8 个 TEST_CASE**，但 `Simulator`
// 出现 **0 次** ⇒ 全裸 `context` + 只断 `payload.width` ⇒ **图从不被实例化**
// ⇒ ⇒ 「空操作数的 mux」永不兑现 ⇒ ⇒ **一仿真就 SIGSEGV 的缺陷完全不可见**。
// ⇒ 既有 8 个用例**保留**（锁宽度/类型），本文件补**真跑仿真**的行为判据。
//
// ## 缺陷实测（探针，ADR-064 §1.2 / §1.4）
//
// | API | 修复前 |
// |---|---|
// | `stream_narrow_to_wide` | 💥 `initialize()` SIGSEGV |
// | `stream_wide_to_narrow` | 💥 SIGSEGV |
// | `stream_width_adapter`（同宽） | 💥 SIGSEGV |
// 且数据通路：宽字**缺最后一个窄字** + `accumulator` 从不零清 ⇒ **跨组污染**。
//
// ## 输出形态（ADR-064 D3）：**寄存**（+1 拍）
//
// 不是实现偏好 —— 组合输出在本仓 `set_input → tick → get_port_value` 读模型下
// **不可观测**（读到沿后组合值，而正确值出现在沿前）⇒ 写不出 parity 判据。
//
// ## ⚠️ 接线纪律（ADR-064 D2）
//
// 组合子要驱动上游 `ready`，而 `ch_stream::ready` **默认无节点**
// ⇒ 调用方 MUST 先 `as_master()`。本文件的 DUT 都显式做了这一步。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == 2 × 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "chlib/stream.h"
#include "chlib/stream_width_adapter.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ===========================================================================
// 拍表 1：narrow_to_wide 8→32（RATIO=4，下游恒 ready）
// ===========================================================================
// 手推（ADR-064 §1.4 的探针已逐拍核对）：
//   tick4 出 0xA3A2A1A0（**含最后一个字**）· tick8 出 0xB3B2B1B0（**无跨组污染**）
// 字段顺序：in_payload, in_valid, out_ready | in_ready, out_valid, out_payload
struct N2wCase {
    uint64_t in_payload, in_valid, out_ready;
    uint64_t in_ready, out_valid, out_payload;
};

// ⚠️ **数据选择 MUST 满足「组 1 字节 | 组 2 字节 ≠ 组 2 字节」**（位错开）——
//    否则「累积器不清零」的跨组污染会被 `|` **掩盖**（实测：`0xA0|0xB0 == 0xB0`
//    ⇒ 变异 M3 恒绿 ⇒ 判别力归零）。这里组 1 用高半字节、组 2 用低半字节。
const N2wCase kN2wCases[] = {
    // 复位后：空闲，输入 ready（输出寄存器空 ⇒ out_free=1）
    {0x00, 0, 1, 1, 0, 0x00000000},
    {0xF0, 1, 1, 1, 0, 0x00000000},
    {0xE1, 1, 1, 1, 0, 0x00000000},
    {0xD2, 1, 1, 1, 0, 0x00000000},
    // 第 4 个窄字到达 ⇒ 完整宽字（低字节先到）
    {0xC3, 1, 1, 1, 1, 0xC3D2E1F0},
    // 下游取走 ⇒ valid 落；新的组开始
    {0x0F, 1, 1, 1, 0, 0xC3D2E1F0},
    {0x1E, 1, 1, 1, 0, 0xC3D2E1F0},
    {0x2D, 1, 1, 1, 0, 0xC3D2E1F0},
    // 第二组完成 ⇒ **不含第一组的残留**（不清零会得到 0xFF……）
    {0x3C, 1, 1, 1, 1, 0x3C2D1E0F},
};

// ===========================================================================
// 拍表 2：narrow_to_wide 的**背压**（下游恒不 ready）
// ===========================================================================
// 契约（ADR-064 §5.2）：下游 ready=0 ⇒ 上游 ready=0 且输出保持不变。
// 手推：tick4 装载完整字后 out_valid=1、out_free=0 ⇒ in_ready=0 一直保持。
const N2wCase kN2wBpCases[] = {
    {0x00, 0, 0, 1, 0, 0x00000000},
    {0xF0, 1, 0, 1, 0, 0x00000000},
    {0xE1, 1, 0, 1, 0, 0x00000000},
    {0xD2, 1, 0, 1, 0, 0x00000000},
    // 完整字已装载，但下游不取 ⇒ 上游被挡住（out_free = !1 | 0 = 0）
    {0xC3, 1, 0, 0, 1, 0xC3D2E1F0},
    {0x0F, 1, 0, 0, 1, 0xC3D2E1F0},
    {0x1E, 1, 0, 0, 1, 0xC3D2E1F0},
    // 下游恢复 ⇒ 同一拍放行并取走（未取走的数据不丢）
    {0x2D, 1, 1, 1, 0, 0xC3D2E1F0},
    {0x3C, 1, 1, 1, 0, 0xC3D2E1F0},
};

// ===========================================================================
// 拍表 3：wide_to_narrow 32→8（RATIO=4，下游恒 ready）
// ===========================================================================
// W = 0xA1B2C3D4 ⇒ byte0..3 = D4, C3, B2, A1（**低字节先出**）
struct W2nCase {
    uint64_t in_payload, in_valid, out_ready;
    uint64_t in_ready, out_valid, out_payload;
};

const W2nCase kW2nCases[] = {
    {0x00000000, 0, 1, 1, 0, 0x00},
    {0xA1B2C3D4, 1, 1, 0, 1, 0xD4},   // 装载 ⇒ 同时吐出 byte0（输出满 ⇒ 上游停）
    {0x00000000, 0, 1, 0, 1, 0xC3},   // 输出满 ⇒ 上游停
    {0x00000000, 0, 1, 0, 1, 0xB2},
    {0x00000000, 0, 1, 0, 1, 0xA1},   // 最后一个字节
    {0x00000000, 0, 1, 1, 0, 0xD4},   // 拆完 ⇒ valid 落，上游恢复
};

// ===========================================================================
// DUT：narrow_to_wide 8→32
// ===========================================================================
struct N2wDut : public ch::Component {
    __io(
        ch_in<ch_uint<8>>   in_payload;
        ch_in<ch_bool>      in_valid;
        ch_in<ch_bool>      out_ready;
        ch_out<ch_bool>     in_ready;
        ch_out<ch_bool>     out_valid;
        ch_out<ch_uint<32>> out_payload;
    )

    N2wDut(ch::Component* parent = nullptr, const std::string& name = "n2w")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> in;
        in.as_master();                       // ⚠️ ADR-064 D2：先建 ready 节点
        in.payload <<= io().in_payload;
        in.valid   <<= io().in_valid;
        auto out = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>(in);
        out.ready <<= ch_bool(io().out_ready.impl());   // 下游覆盖默认恒就绪
        io().in_ready    <<= in.ready;
        io().out_valid   <<= out.valid;
        io().out_payload <<= out.payload;
    }
};

// ===========================================================================
// DUT：wide_to_narrow 32→8
// ===========================================================================
struct W2nDut : public ch::Component {
    __io(
        ch_in<ch_uint<32>> in_payload;
        ch_in<ch_bool>     in_valid;
        ch_in<ch_bool>     out_ready;
        ch_out<ch_bool>    in_ready;
        ch_out<ch_bool>    out_valid;
        ch_out<ch_uint<8>> out_payload;
    )

    W2nDut(ch::Component* parent = nullptr, const std::string& name = "w2n")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<32>> in;
        in.as_master();
        in.payload <<= io().in_payload;
        in.valid   <<= io().in_valid;
        auto out = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>>(in);
        out.ready <<= ch_bool(io().out_ready.impl());
        io().in_ready    <<= in.ready;
        io().out_valid   <<= out.valid;
        io().out_payload <<= out.payload;
    }
};

// ===========================================================================
// DUT：同宽直通（`stream_width_adapter` 的 `else` 分支）
// ===========================================================================
struct PassDut : public ch::Component {
    __io(
        ch_in<ch_uint<16>>  in_payload;
        ch_in<ch_bool>      in_valid;
        ch_in<ch_bool>      out_ready;
        ch_out<ch_bool>     in_ready;
        ch_out<ch_bool>     out_valid;
        ch_out<ch_uint<16>> out_payload;
    )

    PassDut(ch::Component* parent = nullptr, const std::string& name = "pass")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<16>> in;
        in.as_master();
        in.payload <<= io().in_payload;
        in.valid   <<= io().in_valid;
        auto out = stream_width_adapter<ch_uint<16>, ch_uint<16>>(in);
        out.ready <<= ch_bool(io().out_ready.impl());
        io().in_ready    <<= in.ready;
        io().out_valid   <<= out.valid;
        io().out_payload <<= out.payload;
    }
};

}  // namespace

// ===========================================================================
// pair 1：narrow_to_wide 的数据通路 + 跨组隔离
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    N2wDut,
    "stream_narrow_to_wide：宽字含最后一个窄字 + 组间清零（无跨组污染）"
    " + 构造 Simulator 不崩溃（ADR-064 D4）",
    "[chlib][stream][width_adapter][narrow_to_wide][dual][adr064]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kN2wCases) {
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.out_ready, tc.out_ready);
            sim.tick();
            INFO("in_payload=0x" << std::hex << tc.in_payload
                                 << " in_valid=" << std::dec << tc.in_valid
                                 << " out_ready=" << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 2：narrow_to_wide 的背压（存量缺陷正是「从不测背压」）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    N2wDut,
    "stream_narrow_to_wide：下游 ready=0 ⇒ 上游 ready=0 且输出保持"
    "（不覆盖未取走的数据）",
    "[chlib][stream][width_adapter][backpressure][dual][adr064]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kN2wBpCases) {
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.out_ready, tc.out_ready);
            sim.tick();
            INFO("in_payload=0x" << std::hex << tc.in_payload
                                 << " out_ready=" << std::dec << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 3：wide_to_narrow 的拆分顺序 + 保持到 ready
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    W2nDut,
    "stream_wide_to_narrow：按 byte0→byte3 拆分 + 输出满时上游停（ADR-064 D5）",
    "[chlib][stream][width_adapter][wide_to_narrow][dual][adr064]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kW2nCases) {
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.out_ready, tc.out_ready);
            sim.tick();
            INFO("in_payload=0x" << std::hex << tc.in_payload
                                 << " out_ready=" << std::dec << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 4：同宽直通分支（修前同样会 SIGSEGV）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    PassDut,
    "stream_width_adapter 同宽分支：直通 + 上游 ready 跟随下游（ADR-064 D6）",
    "[chlib][stream][width_adapter][passthrough][dual][adr064]",
    [&]() {
        auto &io = dev.instance().io();
        // 下游就绪 ⇒ 直通
        sim.set_input_value(io.in_payload, 0xBEEF);
        sim.set_input_value(io.in_valid, 1);
        sim.set_input_value(io.out_ready, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xBEEF);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1);

        // 下游不就绪 ⇒ 上游 ready 跟随为 0
        sim.set_input_value(io.out_ready, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 0);
    });
