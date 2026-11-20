// examples/stream/stream_mux_top.h
//
// Stream Mux 示例的**顶层组件**（从 `stream_mux_demo.cpp` 抽出 · 三角色分离：
// 设计定义 `*_top.h` → 激励驱动 `tb_*.cpp` → 激励源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。
//
// ⚠️ **端口面（16 根）**：
//   · 4 个输入流 × {payload[7:0], valid, ready}（`ready` 是根自有 **output**）
//   · `select[1:0]`
//   · 输出流 {payload[7:0], valid, ready}（`ready` 是根自有 **input**）
//
// ⚠️ **组合子契约**（capability `chlib-stream-ready-ownership`）：
//   · 调用方 MUST **先建字段节点**（`as_master()`）再传给组合子 ⇒ ⇒ 否则组合子
//     无法把反压钉到调用方的 ready 字段上；
//   · ready 链 MUST 用 `<<=`（MUST NOT 用 `operator=` 重绑定）。

#pragma once

#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"

#include <array>
#include <string>

namespace stream_example {

using namespace ch::core; // ch_uint / ch_bool / ch_in / ch_out / _d 等

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kStreamMuxRootName = "top";

/// 4 选 1 流多路选择器（`chlib::stream_mux` 的具名端口包装）。
class StreamMuxTop : public ch::Component {
  public:
    __io(
        ch_in<ch_uint<8>> in0_payload{"in0_payload"};
        ch_in<ch_bool> in0_valid{"in0_valid"};
        ch_out<ch_bool> in0_ready{"in0_ready"};

        ch_in<ch_uint<8>> in1_payload{"in1_payload"};
        ch_in<ch_bool> in1_valid{"in1_valid"};
        ch_out<ch_bool> in1_ready{"in1_ready"};

        ch_in<ch_uint<8>> in2_payload{"in2_payload"};
        ch_in<ch_bool> in2_valid{"in2_valid"};
        ch_out<ch_bool> in2_ready{"in2_ready"};

        ch_in<ch_uint<8>> in3_payload{"in3_payload"};
        ch_in<ch_bool> in3_valid{"in3_valid"};
        ch_out<ch_bool> in3_ready{"in3_ready"};

        ch_in<ch_uint<2>> select{"select"};

        ch_out<ch_uint<8>> out_payload{"out_payload"};
        ch_out<ch_bool> out_valid{"out_valid"};
        ch_in<ch_bool> out_ready{"out_ready"};)

    StreamMuxTop(ch::Component *parent = nullptr,
                 const std::string &name = "stream_mux_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::array<ch_stream<ch_uint<8>>, 4> inputs;

        // ⚠️ MUST 先建字段节点（`as_master()`）⇒ ⇒ 组合子才能把反压钉到这些 ready 上
        for (auto &s : inputs) {
            s.as_master();
        }

        // ⚠️ 输入端口 → 组合子形参：显式经 `impl()` 取节点
        //    （`ch_bool` 的 port 无隐式转换；`ch_uint` 的 port 有但统一写法更清楚）
        inputs[0].payload = ch_uint<8>(io().in0_payload.impl());
        inputs[0].valid = ch_bool(io().in0_valid.impl());
        inputs[1].payload = ch_uint<8>(io().in1_payload.impl());
        inputs[1].valid = ch_bool(io().in1_valid.impl());
        inputs[2].payload = ch_uint<8>(io().in2_payload.impl());
        inputs[2].valid = ch_bool(io().in2_valid.impl());
        inputs[3].payload = ch_uint<8>(io().in3_payload.impl());
        inputs[3].valid = ch_bool(io().in3_valid.impl());

        auto mux = chlib::stream_mux<4, ch_uint<8>>(
            inputs, ch_uint<2>(io().select.impl()));

        // 反压面：组合子驱动的是**共享的** ready 节点（`<<=` 数据流边）
        io().in0_ready <<= mux.input_streams[0].ready;
        io().in1_ready <<= mux.input_streams[1].ready;
        io().in2_ready <<= mux.input_streams[2].ready;
        io().in3_ready <<= mux.input_streams[3].ready;

        // 数据面（组合 0-cycle）
        io().out_payload <<= mux.output_stream.payload;
        io().out_valid <<= mux.output_stream.valid;

        // 消费端覆盖默认「恒就绪」驱动（`<<=` 覆盖 ⇒ ⇒ 单阶段）
        mux.output_stream.ready <<= io().out_ready;
    }
};

} // namespace stream_example
