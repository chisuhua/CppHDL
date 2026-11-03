/**
 * @file stream_fork_example.cpp
 * @brief Stream Fork 演示 —— 同步复制可行；异步（`synchronous=false`）**显式拒绝**
 *
 * ⚠️ **两条接线 / 契约（ADR-056 + ADR-057）**：
 *
 * ① `input_stream` MUST 先 `as_master()`，且 `input_stream.ready`
 *    **由 `stream_fork()` 自己驱动**（`= AND(各输出 ready)`）
 *    ⇒ 调用方 MUST NOT 自行喂常量 —— literal 不随 src 变化 ⇒ 反压被静默丢弃
 *    ⇒ 组件会**建图期响亮拒绝**（`CHFATAL_EXCEPTION`）。
 *
 * ② `synchronous = false`（真异步：每输出独立消费）**尚未实现**
 *    ⇒ 传 `false` 会**建图期响亮拒绝**（ADR-057 D2；修复前的 `false` 分支把
 *    数据面的 `valid` 接到反压面上 ⇒ 上游不就绪时 `valid` 会**消失**，那是缺陷
 *    而不是「另一种语义」）。⇒ 本 sample **演示这条拒绝本身**（而不是假装它能跑）：
 *    `MUST throw` 就是它的契约。
 */
#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <iostream>
#include <stdexcept>
#include <string>

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
    // ⚠️ MUST 先 `as_master()`：字段才有可被组合子驱动的节点
    //    （缺它 ⇒ `input_stream.ready` 是 nullptr ⇒ 组合子的驱动**只绑本地副本**）。
    input_stream.as_master();
    input_stream.payload = make_uint<8>(0xAB);
    input_stream.valid = ch_bool(true);
    // ⚠️ **不要**驱动 `input_stream.ready` —— `= AND(各输出 ready)` 是
    //    `stream_fork()` 的职责（ADR-057 D1）。

    // 创建同步模式的 Fork（所有输出都就绪才传输）
    auto sync_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, true);

    // 异步模式：契约 = **建图期抛异常**（ADR-057 D2）
    std::cout << "\nAsynchronous Fork (contract: rejected at graph-build time):" << std::endl;
    bool async_rejected = false;
    std::string async_message;
    try {
        auto async_fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);
        (void)async_fork_result;
    } catch (const std::runtime_error &e) {
        async_rejected = true;
        async_message = e.what();
    }

    ch::Simulator sim(&ctx);
    sim.tick();

    std::cout << "\nSynchronous Fork:" << std::endl;
    std::cout << "Input payload: 0x" << std::hex << sim.get_value(input_stream.payload) << std::endl;
    std::cout << "Output 0 payload: 0x" << std::hex << sim.get_value(sync_fork_result.output_streams[0].payload) << std::endl;
    std::cout << "Output 1 payload: 0x" << std::hex << sim.get_value(sync_fork_result.output_streams[1].payload) << std::endl;
    std::cout << "Input valid: " << std::dec << sim.get_value(input_stream.valid) << std::endl;
    std::cout << "Input ready (driven by stream_fork = AND of output readys): "
              << sim.get_value(input_stream.ready) << std::endl;
    std::cout << "Output 0 valid: " << sim.get_value(sync_fork_result.output_streams[0].valid) << std::endl;
    std::cout << "Output 1 valid: " << sim.get_value(sync_fork_result.output_streams[1].valid) << std::endl;

    std::cout << "\nAsynchronous Fork result:" << std::endl;
    std::cout << "rejected: " << (async_rejected ? "yes" : "no") << std::endl;
    if (!async_message.empty()) {
        std::cout << "message: " << async_message << std::endl;
    }

    // 自检 ①：同步 fork 把 payload 复制到两个输出，且输出有效
    for (unsigned i = 0; i < 2; ++i) {
        auto d = static_cast<uint64_t>(sim.get_value(sync_fork_result.output_streams[i].payload));
        auto v = static_cast<uint64_t>(sim.get_value(sync_fork_result.output_streams[i].valid));
        if (d != 0xAB || v != 1) {
            std::cerr << "ERROR: sync fork output " << i << ": payload=0x" << std::hex << d
                      << " valid=" << std::dec << v << " (expected 0xAB / 1)" << std::endl;
            return 1;
        }
    }

    // 自检 ②：`input_stream.ready` 真的被组件驱动了（ADR-057 D1 的可观测面）
    if (static_cast<uint64_t>(sim.get_value(input_stream.ready)) != 1) {
        std::cerr << "ERROR: input_stream.ready must be driven by stream_fork (=1 when "
                     "every output is ready)" << std::endl;
        return 1;
    }

    // 自检 ③：`synchronous=false` MUST 抛（不是返回半成品）
    if (!async_rejected) {
        std::cerr << "ERROR: stream_fork(synchronous=false) must throw (ADR-057 D2)" << std::endl;
        return 1;
    }

    std::cout << "\nStream Fork example completed successfully!" << std::endl;

    return 0;
}
