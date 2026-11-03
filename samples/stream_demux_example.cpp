/**
 * @file stream_demux_example.cpp
 * @brief Stream Demux 演示 —— 按 select 信号把一个输入路由到多个输出之一
 *
 * ⚠️ **接线纪律（ADR-056）**：`input_stream` MUST 先 `as_master()`，
 *    `input_stream.ready` **由 `stream_demux()` 自己驱动**
 *    （`= in_range & OR_i((select==i) & out_i.ready)`）⇒ 调用方 MUST NOT 自行
 *    给它喂常量 —— 否则 literal 不随 src 变化 ⇒ 反压被静默丢弃 ⇒ 组件会
 *    **建图期响亮拒绝**（`CHFATAL_EXCEPTION`）。
 *
 * ⚠️ select 信号用 `ch_uint<1> select_signal(0_d);` + `sim.set_input_value()`
 * 驱动 —— 与 `examples/stream/stream_demux_demo.cpp` 同款：
 * ① `ch_uint<N> sig(0_d)` 建的是一个**可被 TB 驱动**的节点；
 * ② `make_uint<1>(1)` 建的是**常量**节点（不在 data_map_ 里 ⇒ 读不到）。
 */
#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

int main() {
    std::cout << "Stream Demux Example:" << std::endl;

    // 节点创建需要活跃 context（`make_uint` / `ch_bool` 都会建节点）
    ch::core::context ctx("stream_demux_example");
    ch::core::ctx_swap swap(&ctx);

    // 创建输入 Stream
    ch_stream<ch_uint<8>> input_stream;
    // ⚠️ MUST 先 `as_master()`：字段才有可被组合子驱动的节点
    //    （缺它 ⇒ `input_stream.ready` 是 nullptr ⇒ 组合子的驱动**只绑本地副本**）。
    input_stream.as_master();
    input_stream.payload = make_uint<8>(0xAB);
    input_stream.valid = ch_bool(true);
    // ⚠️ **不要**驱动 `input_stream.ready` —— 那是 `stream_demux()` 的职责
    //    （ADR-056 D5 的多驱动守卫会拒绝字面量驱动）。

    // 选择信号（由 TB 驱动）
    ch_uint<1> select_signal(0_d);

    // 创建解复用器（⚠️ MUST 在 Simulator 构造之前）
    auto demux_result = stream_demux<ch_uint<8>, 2>(input_stream, select_signal);

    ch::Simulator sim(&ctx);

    // ---- select = 1：路由到输出 1 ----
    sim.set_input_value(select_signal, 1);
    sim.tick();
    sim.tick();

    std::cout << "Input payload: 0x" << std::hex << sim.get_value(input_stream.payload) << std::endl;
    std::cout << "Input valid: " << std::dec << sim.get_value(input_stream.valid) << std::endl;
    std::cout << "Input ready: " << sim.get_value(input_stream.ready) << std::endl;
    std::cout << "Select signal: " << sim.get_value(select_signal) << std::endl;

    std::cout << "\nOutput streams (select = 1):" << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << sim.get_value(demux_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 0 valid: " << std::dec << sim.get_value(demux_result.output_streams[0].valid) << std::endl;

    std::cout << "Output 1 payload: 0x" << std::hex << sim.get_value(demux_result.output_streams[1].payload) << std::endl;
    std::cout << "Output 1 valid: " << std::dec << sim.get_value(demux_result.output_streams[1].valid) << std::endl;

    if (sim.get_value(demux_result.output_streams[0].valid) != 0 ||
        sim.get_value(demux_result.output_streams[1].valid) != 1) {
        std::cerr << "ERROR: select=1 must route only to output 1" << std::endl;
        return 1;
    }

    // ---- select = 0：路由到输出 0 ----
    sim.set_input_value(select_signal, 0);
    sim.tick();
    sim.tick();

    std::cout << "\nWhen select signal is 0:" << std::endl;
    std::cout << "Select signal: " << sim.get_value(select_signal) << std::endl;
    std::cout << "Output 0 valid: " << sim.get_value(demux_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << sim.get_value(demux_result.output_streams[1].valid) << std::endl;

    if (sim.get_value(demux_result.output_streams[0].valid) != 1 ||
        sim.get_value(demux_result.output_streams[1].valid) != 0) {
        std::cerr << "ERROR: select=0 must route only to output 0" << std::endl;
        return 1;
    }

    std::cout << "\nStream Demux example completed successfully!" << std::endl;

    return 0;
}
