// tests/test_stream_fifo_dual.cpp
//
// `chlib` FIFO 的 **FWFT（0-cycle）** 语义的**双后端 parity** 覆盖。
//
// ⚠️ 动机（capability `chlib-stream-semantics` 的
//    「`chlib::stream_fifo` 的行为 MUST 有双后端 parity 覆盖」）：
//    `sim-mem-read-timing-align-2026-10` 才发现 interpreter 的 `ch_mem::aread`
//    与生成的 Verilog `assign proxy = mem[addr];` 差 1 拍 ⇒ **0-cycle 语义正是
//    历史上漂移过的地方**。⇒ 只跑 interpreter 的断言 MUST NOT 被当作
//    「FWFT 契约已被守住」的证据。
//
//    本文件覆盖两个 DUT：
//    ① `StreamFifoDualDut` —— 包装 `chlib::stream_fifo`（capability 的显式主体）
//    ② `SyncFifoDualDut`  —— 包装 `chlib::sync_fifo` 的 `q_async`（FWFT 的**源头**）
//
//    ② 的必要性：`chlib::stream_fifo` 的读使能**硬编码** `pop_ready = true`
//    ⇒ FIFO 永远在排空 ⇒ **队列深度永远 ≤ 1** ⇒ 「pop 沿之后 payload 指向
//    **下一个**队首」这条契约在 `stream_fifo` 上**无法被观测**（队列里从来不会
//    同时有 2 个元素）。只有直接驱动 `sync_fifo` 的 `rden` 才能把队列填到
//    多元素并观测到队首的推进。
//
// ⚠️ DUT 形态约束：Verilator 后端**无法观测深层内部状态**（它的 dlopen
//    访问表只暴露顶层端口）⇒ payload / valid / empty / full MUST 全部提到
//    **顶层 `ch_out`**。
//
// ⚠️ 顶层端口宽度 MUST ≤ 64 bit（ADR-035 R3 的端口上限）⇒ 本文件用
//    8-bit payload + 1-bit 标志 + 3-bit 计数。

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/fifo.h"
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
// DUT ①：`chlib::stream_fifo` 的包装
//
// ⚠️ `in.payload = io().in_payload;` MUST 用 `=`（节点重绑定）而不是 `<<=`：
//    `stream_fifo` 内部要对 `input_stream.valid` 做**值**运算
//    （`valid & ready`），而 `<<=` 建立的是 proxy 节点 ⇒ "Invalid operands
//    for binary operation"（见 `samples/stream_fifo_example.cpp:66-75`）。
// ---------------------------------------------------------------------------
class StreamFifoDualDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> out_empty;
         ch_out<ch_bool> out_full;)

    StreamFifoDualDut(ch::Component *parent = nullptr,
                      const std::string &name = "stream_fifo_dual")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> in;
        in.payload = io().in_payload;          // 节点重绑定 ⇒ 值节点
        in.valid = ch_bool(io().in_valid.impl());
        in.ready = ch_bool(true);              // 消费端恒就绪

        auto fifo = stream_fifo<ch_uint<8>, 4>(in);

        io().out_payload = fifo.pop_stream.payload;
        io().out_valid = fifo.pop_stream.valid;
        io().out_empty = fifo.empty;
        io().out_full = fifo.full;
    }
};

// ---------------------------------------------------------------------------
// DUT ②：`chlib::sync_fifo` 的 `q_async`（0-cycle 队首视图）
//
// ⚠️ 这同时是本仓库**第一例**「同一个 `ch_mem` 同时挂 `aread` 与 `sread`
//    两个读端口」的设计 ⇒ 它顺带覆盖了 codegen 的「一个 mem 多个读端口」
//    路径（`src/codegen_verilog.cpp::print_mem_read_port` 按端口分别发射
//    `assign` / `always_ff`）。
// ---------------------------------------------------------------------------
class SyncFifoDualDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> wr_data;
         ch_in<ch_bool> wr_en;
         ch_in<ch_bool> rd_en;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> out_empty;
         ch_out<ch_bool> out_full;
         ch_out<ch_uint<3>> out_count;)

    SyncFifoDualDut(ch::Component *parent = nullptr,
                    const std::string &name = "sync_fifo_dual")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ADDR_WIDTH = 2 ⇒ 4 个槽位、count 为 3 bit
        auto fifo = sync_fifo<8, 2>(ch_bool(io().wr_en.impl()),
                                    ch_uint<8>(io().wr_data.impl()),
                                    ch_bool(io().rd_en.impl()));

        io().out_payload = fifo.q_async;      // ⚠️ 0-cycle 队首视图
        io().out_valid = ch_bool(!fifo.empty);
        io().out_empty = fifo.empty;
        io().out_full = fifo.full;
        io().out_count = fifo.count;
    }
};

}  // anonymous namespace

// ===========================================================================
// ① `chlib::stream_fifo` —— FWFT 写穿透
// ===========================================================================
//
// ⚠️ 断言必须**逐拍钉住**（tick 序号 + valid + payload 三元组）：
//    只断言 payload 会被 1-cycle `sread` 实现在 `valid == 0` 的那一拍骗过。
DUAL_BACKEND_TEST_PAIR(
    StreamFifoDualDut,
    "chlib::stream_fifo - FWFT 写穿透：push 沿后**同拍** payload == 刚写入的数据",
    "[chlib][stream][fifo][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // tick 1：空 FIFO ⇒ valid == 0（payload 是 don't-care，不断言）
        sim.set_input_value(io.in_valid, 0);
        sim.set_input_value(io.in_payload, 0x00);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 1u);

        // tick 2：推入 0xA1 ⇒ **同拍** valid == 1 且 payload == 0xA1
        // （1-cycle `sread` 在这一拍会显示 0x00 ⇒ 可区分）
        sim.set_input_value(io.in_valid, 1);
        sim.set_input_value(io.in_payload, 0xA1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xA1u);

        // tick 3：推入 0xB2 ⇒ payload 必须是 **0xB2**（不是上一拍的 0xA1）
        sim.set_input_value(io.in_payload, 0xB2);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xB2u);

        // tick 4：推入 0xC3 ⇒ payload 必须是 **0xC3**
        sim.set_input_value(io.in_payload, 0xC3);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xC3u);
    });

// ===========================================================================
// ① `chlib::stream_fifo` —— 停止推送后 FIFO 排空
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    StreamFifoDualDut,
    "chlib::stream_fifo - 停止推送后 FIFO 排空（valid 回落）",
    "[chlib][stream][fifo][dual]",
    [&]() {
        auto &io = dev.instance().io();

        sim.set_input_value(io.in_valid, 1);
        sim.set_input_value(io.in_payload, 0x5A);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0x5Au);

        // 停止推送 ⇒ `stream_fifo` 的读使能恒为 true ⇒ 下一拍即排空
        sim.set_input_value(io.in_valid, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 1u);
    });

// ===========================================================================
// ② `chlib::sync_fifo::q_async` —— 0-cycle 队首视图 + 队首推进
// ===========================================================================
//
// ⚠️ 这是「pop 沿之后 payload 指向**下一个**队首」这条契约的**唯一**可观测点
//    （`stream_fifo` 的队列深度恒 ≤ 1 ⇒ 观测不到）。
DUAL_BACKEND_TEST_PAIR(
    SyncFifoDualDut,
    "chlib::sync_fifo - q_async 是 0-cycle 队首视图（写穿透 + 队首推进）",
    "[chlib][fifo][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // tick 1：空 FIFO
        sim.set_input_value(io.wr_en, 0);
        sim.set_input_value(io.rd_en, 0);
        sim.set_input_value(io.wr_data, 0x00);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 0u);

        // tick 2：写入 0xA1 ⇒ **同拍** q_async == 0xA1（0-cycle 写穿透）
        // （1-cycle `sread` 在这一拍仍是 0x00 ⇒ 可区分）
        sim.set_input_value(io.wr_en, 1);
        sim.set_input_value(io.wr_data, 0xA1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xA1u);

        // tick 3：写入 0xB2（不读）⇒ 队首**仍是** 0xA1，count == 2
        sim.set_input_value(io.wr_data, 0xB2);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 2u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xA1u);

        // tick 4：读一拍 ⇒ 队首**推进**到 0xB2，count == 1
        //         （「pop 沿之后 payload 指向下一个队首」）
        sim.set_input_value(io.wr_en, 0);
        sim.set_input_value(io.rd_en, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xB2u);

        // tick 5：再读一拍 ⇒ 队列空，count == 0
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 1u);
    });

// ===========================================================================
// ② `chlib::sync_fifo` —— 自保护（空 FIFO 上读不推进指针 / 满 FIFO 上写不覆盖）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    SyncFifoDualDut,
    "chlib::sync_fifo - 自保护：空读不推进指针、满写不覆盖数据",
    "[chlib][fifo][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // 空 FIFO 上连续读 3 拍 ⇒ count 恒为 0，读指针**不得**前进
        sim.set_input_value(io.wr_en, 0);
        sim.set_input_value(io.rd_en, 1);
        sim.tick();
        sim.tick();
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_empty)) == 1u);

        // 读指针没前进 ⇒ 现在写入 0xA1 MUST 能被读回
        sim.set_input_value(io.rd_en, 0);
        sim.set_input_value(io.wr_en, 1);
        sim.set_input_value(io.wr_data, 0xA1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xA1u);

        // 填满（4 个槽位）⇒ 满标志置位
        sim.set_input_value(io.wr_data, 0xB2);
        sim.tick();
        sim.set_input_value(io.wr_data, 0xC3);
        sim.tick();
        sim.set_input_value(io.wr_data, 0xD4);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 4u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_full)) == 1u);

        // 满时再写 0xFF ⇒ **MUST NOT** 覆盖队首 0xA1，count 也不得溢出
        sim.set_input_value(io.wr_data, 0xFF);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_count)) == 4u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0xA1u);
    });
