// examples/stream/stream_demux_top.h
//
// Stream Demux 示例的**顶层组件**（从 `stream_demux_demo.cpp` 抽出 · 三角色分离：
// 设计定义 `*_top.h` → 激励驱动 `tb_*.cpp` → 激励源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。
//
// ⚠️ **端口面（16 根）**：
//   · 输入流 {payload[7:0], valid, ready}（`ready` 是根自有 **output** · 由 demux 驱动）
//   · `select[1:0]`
//   · 4 个输出流 × {payload[7:0], valid, ready}（`ready` 是根自有 **input** · 消费端驱动）
//
// ⚠️ **组合子契约**（capability `chlib-stream-ready-ownership` · ADR-056）：
//   · 调用方 MUST **先建字段节点**（`as_master()`）再传给组合子 ⇒ ⇒ 否则组合子
//     无法把反压钉到调用方的 ready 字段上；
//   · ready 链 MUST 用 `<<=`（MUST NOT 用 `operator=` 重绑定）。
//   · 消费端覆盖输出 ready 用 `<<=` **直接覆盖**组合子的「默认恒就绪」驱动
//     （ADR-054 形态 E · 单阶段）。

#pragma once

#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"

#include <string>

namespace stream_example {

using namespace ch::core; // ch_uint / ch_bool / ch_in / ch_out / _d 等

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kStreamDemuxRootName = "top";

/// 1 选 4 流解复用器（`chlib::stream_demux` 的具名端口包装）。
class StreamDemuxTop : public ch::Component {
  public:
    __io(
        ch_in<ch_uint<8>> in_payload{"in_payload"};
        ch_in<ch_bool> in_valid{"in_valid"};
        ch_out<ch_bool> in_ready{"in_ready"};

        ch_in<ch_uint<2>> select{"select"};

        ch_out<ch_uint<8>> out0_payload{"out0_payload"};
        ch_out<ch_bool> out0_valid{"out0_valid"};
        ch_in<ch_bool> out0_ready{"out0_ready"};

        ch_out<ch_uint<8>> out1_payload{"out1_payload"};
        ch_out<ch_bool> out1_valid{"out1_valid"};
        ch_in<ch_bool> out1_ready{"out1_ready"};

        ch_out<ch_uint<8>> out2_payload{"out2_payload"};
        ch_out<ch_bool> out2_valid{"out2_valid"};
        ch_in<ch_bool> out2_ready{"out2_ready"};

        ch_out<ch_uint<8>> out3_payload{"out3_payload"};
        ch_out<ch_bool> out3_valid{"out3_valid"};
        ch_in<ch_bool> out3_ready{"out3_ready"};)

    StreamDemuxTop(ch::Component *parent = nullptr,
                   const std::string &name = "stream_demux_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ MUST 先建字段节点（`as_master()`）⇒ ⇒ 组合子才能把反压钉到 ready 上
        ch_stream<ch_uint<8>> input;
        input.as_master();

        // ⚠️ 输入端口 → 组合子形参：显式经 `impl()` 取节点
        //    （`ch_bool` 的 port 无隐式转换；`ch_uint` 的 port 有但统一写法更清楚）
        input.payload = ch_uint<8>(io().in_payload.impl());
        input.valid = ch_bool(io().in_valid.impl());

        auto demux_result = chlib::stream_demux<ch_uint<8>, 4>(
            input, ch_uint<2>(io().select.impl()));

        // 反压面：demux 驱动上游 ready（`result.input_stream` 是**只读别名** ⇒
        // 与传入的 `input` 共享同一 ready 节点 ⇒ ⇒ 这里读到的是**被驱动后**的值）
        io().in_ready <<= demux_result.input_stream.ready;

        // 数据面（组合 0-cycle）：`out[i].valid = in.valid & (select == i)`；
        // `out[i].payload = (select == i) ? in.payload : 0`（ADR-056 D3）
        io().out0_payload <<= demux_result.output_streams[0].payload;
        io().out0_valid <<= demux_result.output_streams[0].valid;
        io().out1_payload <<= demux_result.output_streams[1].payload;
        io().out1_valid <<= demux_result.output_streams[1].valid;
        io().out2_payload <<= demux_result.output_streams[2].payload;
        io().out2_valid <<= demux_result.output_streams[2].valid;
        io().out3_payload <<= demux_result.output_streams[3].payload;
        io().out3_valid <<= demux_result.output_streams[3].valid;

        // 消费端覆盖默认「恒就绪」驱动（`<<=` 覆盖 ⇒ ⇒ 单阶段）
        // ⚠️ `in_ready` = `in_range & out[select].ready` ⇒ ⇒ **只有被选中路的
        //   ready 影响上游** ⇒ ⇒ 未选中路的 ready 传什么都无所谓。
        demux_result.output_streams[0].ready <<= io().out0_ready;
        demux_result.output_streams[1].ready <<= io().out1_ready;
        demux_result.output_streams[2].ready <<= io().out2_ready;
        demux_result.output_streams[3].ready <<= io().out3_ready;
    }
};

} // namespace stream_example
