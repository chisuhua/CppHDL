/**
 * @file stream_fork_example.cpp
 * @brief Stream Fork 演示 —— 同步 / 异步两种复制模式
 *
 * 与 `examples/stream/stream_fork_demo.cpp` 的差别：本 sample 额外演示了
 * **异步** fork 模式（任意一个输出就绪即可传输），而 demo 只演示同步模式。
 *
 * ⚠️ **为什么本 sample 不读 `.ready`**（2026-10-22 实测）：
 * `stream_fork()` 在**构造**结果对象时就读取 `result.output_streams[i].ready`
 * 来算 `all_ready`，而调用方此刻**还没机会**驱动它（结果对象尚未返回）
 * ⇒ 该节点为 null ⇒ `all_ready` 及其下游全部失效 ⇒ 读 `.ready` 会
 * CHABORT（"ch_bool implementation is null"）。
 * ⇒ 本 sample 只演示 payload / valid —— 与 `examples/stream/stream_fork_demo.cpp`
 * 的做法一致。`.ready` 的正确修法属 chlib 侧的 API 问题（独立立项）。
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
    std::cout << "Stream Fork Example:" << std::endl;

    // 节点创建需要活跃 context（`make_uint` / `ch_bool` 都会建节点）
    ch::core::context ctx("stream_fork_example");
    ch::core::ctx_swap swap(&ctx);

    // 创建 Stream 接口
    ch_stream<ch_uint<8>> input_stream;
    input_stream.payload = make_uint<8>(0xAB);
    input_stream.valid = ch_bool(true);
    // ⚠️ 异步模式的 `output_streams[i].valid` 由 `input_stream.valid &
    //    input_stream.ready` 驱动 ⇒ **输入的 ready 必须在调用前**设好，
    //    否则该节点无效。
    input_stream.ready = ch_bool(true);

    // 创建同步模式的 Fork（所有输出都就绪才传输）
    auto sync_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, true);

    // 创建异步模式的 Fork（任意输出就绪即可传输）
    auto async_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);

    ch::Simulator sim(&ctx);
    sim.tick();

    std::cout << "\nSynchronous Fork:" << std::endl;
    std::cout << "Input payload: 0x" << std::hex << sim.get_value(input_stream.payload) << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << sim.get_value(sync_fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 1 payload: 0x" << std::hex << sim.get_value(sync_fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Input valid: " << sim.get_value(input_stream.valid) << std::endl;
    std::cout << "Output 0 valid: " << sim.get_value(sync_fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << sim.get_value(sync_fork_result.output_streams[1].valid) << std::endl;

    std::cout << "\nAsynchronous Fork:" << std::endl;
    std::cout << "Input payload: 0x" << std::hex << sim.get_value(input_stream.payload) << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << sim.get_value(async_fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 1 payload: 0x" << std::hex << sim.get_value(async_fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Input valid: " << sim.get_value(input_stream.valid) << std::endl;
    std::cout << "Output 0 valid: " << sim.get_value(async_fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << sim.get_value(async_fork_result.output_streams[1].valid) << std::endl;

    // 自检：两种模式下 fork 都把 payload 复制到两个输出，且输出有效
    for (int mode = 0; mode < 2; ++mode) {
        auto &r = (mode == 0) ? sync_fork_result : async_fork_result;
        for (unsigned i = 0; i < 2; ++i) {
            auto d = static_cast<uint64_t>(sim.get_value(r.output_streams[i].payload));
            auto v = static_cast<uint64_t>(sim.get_value(r.output_streams[i].valid));
            if (d != 0xAB || v != 1) {
                std::cerr << "ERROR: fork mode " << mode << " output " << i
                          << ": payload=0x" << std::hex << d << " valid=" << std::dec
                          << v << " (expected 0xAB / 1)" << std::endl;
                return 1;
            }
        }
    }

    std::cout << "\nStream Fork example completed successfully!" << std::endl;

    return 0;
}
