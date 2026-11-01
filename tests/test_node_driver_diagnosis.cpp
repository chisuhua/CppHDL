// tests/test_node_driver_diagnosis.cpp
//
// B7 · capability `core-node-driver-visibility`：**未驱动字段的只读诊断**
// （`ch::core::diagnose_drive` / `ch::core::drive_state`）的**可执行**判据。
//
// ⚠️ **为什么需要**：框架对「读一个**未被驱动**的字段 / 代理 / 端口」返回 **0**
//    且**不报错** ⇒ ⇒ **静默**（本仓第 3 个实例，前两个是 `ch_in` 的
//    `operator=` 与未驱动代理节点）。
//    ⇒ ⇒ 本文件**不改**任何读取语义（⇒ ⇒ 零回归）⇒ ⇒ 只**回答**
//    「它被驱动了没有」。
//
// ⚠️ **五种状态必须逐一钉住**（`drive_state` 的**全部**取值）——
//    少一格 ⇒ 「诊断说未驱动、实际已驱动」这类**误判**就可能溜过去。
//
// ⚠️ **与两个「响亮拒绝」helper 的联动**：组合子 MUST 用**同一个**判定
//    （`ch::core::diagnose_drive`）⇒ ⇒ 否则 pipe 族与 demux/fork 的判据
//    **各写各的** ⇒ ⇒ 必然漂移 ⇒ ⇒ 本文件同时断言「helper 对
//    `literal` / `driven` 两种状态都**响亮拒绝**」。

#include "catch_amalgamated.hpp"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "core/uint.h"

#include <memory>

using namespace ch;
using namespace ch::core;

namespace {

/// 一个**非字面量**的信号源（`op` 节点）⇒ ⇒ 用来区分
/// `driven_by_literal`（默认值）与 `driven`（真信号）。
ch_uint<8> make_real_signal(ch_uint<8> a, ch_uint<8> b) {
    ch_uint<8> sum = a + b;   // `op` 节点（非 literal）
    return sum;
}

}  // anonymous namespace

TEST_CASE("drive_state: 未驱动字段的只读诊断 —— 五种状态逐一钉住",
          "[core][diagnose][drive_state]") {
    auto ctx = std::make_unique<ch::core::context>("test_drive_state");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // ── ① `no_node`：从未被使用过的字段 ⇒ `impl()` 为空 ──
    // ⚠️ `logic_buffer()` 的默认构造 = `node_impl_(nullptr)`
    //    ⇒ ⇒ `<<=` 在这种节点上会**另建**一个 assign 节点并**只绑到本地副本**
    //    ⇒ ⇒ 调用方看不到任何效果（**静默失效**）⇒ ⇒ 诊断 MUST 报 `no_node`。
    ch_bool never_used;
    REQUIRE(never_used.impl() == nullptr);
    REQUIRE(ch::core::diagnose_drive(never_used.impl()) ==
            ch::core::drive_state::no_node);
    REQUIRE(ch::core::node_exists(never_used.impl()) == false);
    REQUIRE(ch::core::node_is_driven(never_used.impl()) == false);
    REQUIRE(ch::core::node_is_literal(never_used.impl()) == false);

    // ── ② `literal`：字面量节点（值不随 src 变化 ⇒ 驱动它无效）──
    ch_bool lit(true);
    REQUIRE(lit.impl() != nullptr);
    REQUIRE(ch::core::diagnose_drive(lit.impl()) ==
            ch::core::drive_state::literal);
    REQUIRE(ch::core::node_is_literal(lit.impl()) == true);

    // ── ③ `undriven`：字段**有节点**但**没有驱动源** ──
    //    （`as_master()` 建了字段节点，但没人驱动它）
    ch_stream<ch_uint<8>> master;
    master.as_master();
    REQUIRE(master.ready.impl() != nullptr);
    REQUIRE(ch::core::diagnose_drive(master.ready.impl()) ==
            ch::core::drive_state::undriven);
    REQUIRE(ch::core::node_is_driven(master.ready.impl()) == false);

    // ── ④ `driven_by_literal`：被一个**字面量**驱动（= 组合子的默认值）──
    //    ⇒ 这一格 MUST **允许**被覆盖（ADR-054 形态 E）
    ch_stream<ch_uint<8>> defaulted;
    defaulted.as_master();
    defaulted.ready <<= lit;
    REQUIRE(ch::core::diagnose_drive(defaulted.ready.impl()) ==
            ch::core::drive_state::driven_by_literal);
    REQUIRE(ch::core::node_is_driven(defaulted.ready.impl()) == true);

    // ── ⑤ `driven`：被**真信号**驱动（⇒ 再驱动 = last-wins ⇒ 静默接管）──
    ch_stream<ch_uint<8>> driven;
    driven.as_master();
    ch_uint<8> real = make_real_signal(ch_uint<8>(1_d), ch_uint<8>(2_d));
    driven.ready <<= real;
    REQUIRE(ch::core::diagnose_drive(driven.ready.impl()) ==
            ch::core::drive_state::driven);
    REQUIRE(ch::core::node_is_driven(driven.ready.impl()) == true);
}

TEST_CASE("drive_state: 组合子的响亮拒绝 MUST 走同一判定（B7 的联动面）",
          "[core][diagnose][drive_state][guard]") {
    auto ctx = std::make_unique<ch::core::context>("test_drive_state_guard");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // ① `literal` ⇒ `stream_demux` MUST **响亮拒绝**
    //    （调用方用 `operator=` 给 ready 喂了常量 ⇒ 驱动它无效）
    {
        ch_stream<ch_uint<8>> literal_ready;
        literal_ready.as_master();
        literal_ready.ready = ch_bool(true);      // ⚠️ 重绑定到字面量
        REQUIRE(ch::core::diagnose_drive(literal_ready.ready.impl()) ==
                ch::core::drive_state::literal);
        REQUIRE_THROWS_AS(
            (chlib::stream_demux<ch_uint<8>, 2>(literal_ready, ch_uint<1>(0_d))),
            std::runtime_error);
    }

    // ② `driven` ⇒ `stream_demux` MUST **响亮拒绝**
    //    （调用方自行驱动了 ready ⇒ 再驱动 = last-wins ⇒ 静默接管）
    {
        ch_stream<ch_uint<8>> driven_ready;
        driven_ready.as_master();
        ch_uint<8> real = make_real_signal(ch_uint<8>(1_d), ch_uint<8>(2_d));
        driven_ready.ready <<= real;
        REQUIRE(ch::core::diagnose_drive(driven_ready.ready.impl()) ==
                ch::core::drive_state::driven);
        REQUIRE_THROWS_AS(
            (chlib::stream_demux<ch_uint<8>, 2>(driven_ready, ch_uint<1>(0_d))),
            std::runtime_error);
    }

    // ③ `undriven` ⇒ **允许**驱动（⇒ ⇒ 组合子正常构造，不抛）
    {
        ch_stream<ch_uint<8>> free_ready;
        free_ready.as_master();
        REQUIRE(ch::core::diagnose_drive(free_ready.ready.impl()) ==
                ch::core::drive_state::undriven);
        auto demux = chlib::stream_demux<ch_uint<8>, 2>(free_ready,
                                                        ch_uint<1>(0_d));
        REQUIRE(demux.output_streams.size() == 2);
    }
}
