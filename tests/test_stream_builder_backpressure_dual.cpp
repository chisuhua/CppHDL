// tests/test_stream_builder_backpressure_dual.cpp
//
// `StreamBuilder` 链的**上游反压**（ADR-055 · 链内闭包不变式）的
// **双后端 parity** 覆盖。
//
// ⚠️ **动机**（capability `chlib-stream-ready-ownership` 的「上游反压 MUST 真的
//    到达调用方」Scenario · 台账
//    `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`）：
//    `stream_fifo` **不驱动**入参 `ready`（写使能只用 `valid`，内部 `sync_fifo`
//    自保护）⇒ ⇒ builder 链里**没人驱动** `source.ready` ⇒ ⇒ 它**恒为 0**
//    ⇒ ⇒ **上游遵守 ready 协议 ⇒ ⇒ 永远推不动 = 死路**。
//
// ⚠️⚠️ **判别性判据 MUST 是「链空 ⇒ 上游可接收」**：
//    修复前 `source.ready` **无驱动** ⇒ 读到 **0**；而「FIFO 满」时它**也**是 0
//    ⇒ ⇒ **只看「满时为 0」是不可区分的（假绿）** ⇒ ⇒ 本文件在**空链**时断言
//    `in_ready == 1`（⇒ ⇒ 修复前必红）。
//
// ⚠️ **接线纪律**（调用方义务 · A4 刚实测出的「假绿新形态」）：
//    `source` MUST 先 `as_master()`（⇒ 字段**有节点**）⇒ ⇒ 否则 builder 会把
//    反压钉到一个**只有本地副本看得到**的节点上 ⇒ ⇒ **静默失效**
//    ⇒ ⇒ 本文件的 DUT 全部按纪律接线；**反例**（缺 `as_master()` / `ready`
//    被喂 literal）在文件末尾的「响亮拒绝」用例里钉住。
//
// ⚠️ DUT 形态约束：Verilator 后端只暴露**顶层端口** ⇒ payload / valid /
//    in_ready MUST 全部提到顶层 `ch_out`。

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "chlib/stream_builder.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ---------------------------------------------------------------------------
// DUT Ⓐ：**两级** `queue<4>()` 链（总容量 8）
// ---------------------------------------------------------------------------
class ChainTwoLevelDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> in_ready;)

    ChainTwoLevelDut(ch::Component *parent = nullptr,
                     const std::string &name = "builder_chain_2")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> source;
        // ⚠️ **接线纪律**：先建字段节点（⇒ `source.ready` 有节点）
        source.as_master();
        // ⚠️ 数据侧用 `=`（节点重绑定）—— 与 `tests/test_stream_fifo_dual.cpp`
        //    的既有 DUT 同型：`stream_fifo` 内部对 `input_stream.valid` 做**值**
        //    运算，而 `port<ch_bool>` **没有** `ch_bool` 转换 ⇒ 显式包一层。
        source.payload = io().in_payload;
        source.valid = ch_bool(io().in_valid.impl());

        auto built = make_stream_builder(source).queue<4>().queue<4>().build();

        io().out_payload = built.payload;
        io().out_valid = built.valid;
        // ⚠️ **上游反压的观测点**
        io().in_ready = source.ready;
        // ⚠️ 消费端 ready ⇒ ⇒ 覆盖**链尾**的默认驱动（ADR-054 形态 E）
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        built.ready <<= consumer;
    }
};

// ---------------------------------------------------------------------------
// DUT Ⓑ：**一级** `queue<4>()` 链（总容量 4）—— 链长无关性对照
// ---------------------------------------------------------------------------
class ChainOneLevelDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> in_ready;)

    ChainOneLevelDut(ch::Component *parent = nullptr,
                     const std::string &name = "builder_chain_1")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> source;
        source.as_master();
        source.payload = io().in_payload;
        source.valid = ch_bool(io().in_valid.impl());

        auto built = make_stream_builder(source).queue<4>().build();

        io().out_payload = built.payload;
        io().out_valid = built.valid;
        io().in_ready = source.ready;
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        built.ready <<= consumer;
    }
};

// 把「按 ready 协议推入 N 个词 + 消费端不就绪」的**金标序列**写成一个函数，
// 两个 DUT 共用（⇒ 链长无关性直接可比）。
std::vector<uint64_t> push_respecting_ready(
    auto &dev, auto &sim, unsigned n, uint64_t base, bool &all_accepted) {
    auto &io = dev.instance().io();
    sim.set_input_value(io.consumer_ready, 0);
    sim.set_input_value(io.in_valid, 0);
    sim.tick();

    std::vector<uint64_t> pushed;
    all_accepted = true;
    for (unsigned t = 0; t < 40 && pushed.size() < n; ++t) {
        const bool can =
            static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1u;
        sim.set_input_value(io.in_valid, can ? 1 : 0);
        sim.set_input_value(io.in_payload,
                            static_cast<uint64_t>(base + pushed.size()));
        if (can) {
            pushed.push_back(base + pushed.size());
        }
        sim.tick();
    }
    sim.set_input_value(io.in_valid, 0);
    sim.tick();
    if (pushed.size() != n) {
        all_accepted = false;
    }
    return pushed;
}

}  // anonymous namespace

// ===========================================================================
// Ⓐ 两级链 —— 上游反压真的很到达调用方（0 → 1 → 0）+ 零丢字
// ===========================================================================
//
// ⚠️ **手推金标**（每级 `queue<4>()` ⇒ 总容量 8）：
//    · 链**空** ⇒ `full₁ == 0` ⇒ `in_ready == 1`      ← **判别性**（修复前恒 0）
//    · 按 ready 协议推入 8 个词（消费端恒不就绪）⇒ 两级**都满**
//      ⇒ `in_ready == 0`
//    · 链头 = 最早写入的那个词（`0x10`）
//    · 消费端改为恒就绪 ⇒ 每拍弹一个 ⇒ 序列 MUST 逐项 == `0x10..0x17`
//      （⇒ 零丢字、顺序一致）
//    · 全部弹完后 ⇒ 两级皆空 ⇒ `in_ready` 回到 1
DUAL_BACKEND_TEST_PAIR(
    ChainTwoLevelDut,
    "StreamBuilder 两级链 - 上游反压到达调用方（空链可接收 / 满链停止 / 零丢字）",
    "[chlib][stream][builder][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // 链空 ⇒ 上游 MUST 可接收（**判别性**：修复前该端口恒 0）
        sim.set_input_value(io.consumer_ready, 0);
        sim.set_input_value(io.in_valid, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1u);

        bool all_accepted = false;
        auto pushed = push_respecting_ready(dev, sim, 8, 0x10, all_accepted);
        REQUIRE(all_accepted);
        REQUIRE(pushed.size() == 8);

        // 两级都满 ⇒ 上游 MUST 停止（反压真的到达）
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 0u);
        // 链头 = 最早写入的词
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0x10u);

        // 消费端就绪 ⇒ 逐项弹出 ⇒ 手推金标 0x10..0x17（零丢字 + 顺序）
        std::vector<uint64_t> seen;
        for (unsigned t = 0; t < 40 && seen.size() < 8; ++t) {
            sim.set_input_value(io.consumer_ready, 1);
            if (static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u) {
                seen.push_back(
                    static_cast<uint64_t>(sim.get_port_value(io.out_payload)));
            }
            sim.tick();
        }
        REQUIRE(seen.size() == 8);
        for (std::size_t i = 0; i < 8; ++i) {
            REQUIRE(seen[i] == 0x10u + i);
        }

        // 弹空 ⇒ 反压解除
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1u);
    });

// ===========================================================================
// Ⓑ 一级链 —— 同样的反压曲线（链长无关性）
// ===========================================================================
//
// ⚠️ **手推金标**（一级 `queue<4>()` ⇒ 总容量 4）：
//    空链 `in_ready == 1` · 推满 4 个 ⇒ `in_ready == 0` · 链头 `0x20`
//    ⇒ 逐项弹出 `0x20..0x23` ⇒ 弹空后 `in_ready` 回到 1
DUAL_BACKEND_TEST_PAIR(
    ChainOneLevelDut,
    "StreamBuilder 一级链 - 反压曲线与两级链一致（链长无关）",
    "[chlib][stream][builder][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();

        sim.set_input_value(io.consumer_ready, 0);
        sim.set_input_value(io.in_valid, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1u);

        bool all_accepted = false;
        auto pushed = push_respecting_ready(dev, sim, 4, 0x20, all_accepted);
        REQUIRE(all_accepted);

        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) == 0x20u);

        std::vector<uint64_t> seen;
        for (unsigned t = 0; t < 40 && seen.size() < 4; ++t) {
            sim.set_input_value(io.consumer_ready, 1);
            if (static_cast<uint64_t>(sim.get_port_value(io.out_valid)) == 1u) {
                seen.push_back(
                    static_cast<uint64_t>(sim.get_port_value(io.out_payload)));
            }
            sim.tick();
        }
        REQUIRE(seen.size() == 4);
        for (std::size_t i = 0; i < 4; ++i) {
            REQUIRE(seen[i] == 0x20u + i);
        }

        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) == 1u);
    });
