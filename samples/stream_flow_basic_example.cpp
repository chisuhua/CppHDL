/**
 * @file stream_flow_basic_example.cpp
 * @brief Stream / Flow 基础演示 —— 带反压的 Stream 与无反压的 Flow
 *
 * ⚠️ API 漂移记录（2026-10-22）：旧 API 用 `Stream<T>` / `Flow<T>` 与全局
 * `simulator` 对象；当前 API 是 `ch_stream<T>` / `ch_flow<T>` +
 * `ch::Simulator sim(&ctx)`（节点创建需要活跃 context）。
 *
 * ⚠️ **所有节点 MUST 在 `ch::Simulator` 构造之前创建** —— Simulator 在构造时
 * 建立 data_map_，之后新建的节点不在其中 ⇒ `get_value()` 会读到
 * "Value not found" 警告。
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
    std::cout << "CppHDL Stream/Flow Basic Example:" << std::endl;

    // 节点创建需要活跃 context（`make_uint` / `ch_bool` 都会建节点）
    ch::core::context ctx("stream_flow_basic_example");
    ch::core::ctx_swap swap(&ctx);

    // 创建 Stream（带反压）
    ch_stream<ch_uint<8>> stream_io;
    stream_io.payload = make_uint<8>(0x5A);
    stream_io.valid = ch_bool(true);
    stream_io.ready = ch_bool(false);  // 初始时未就绪

    // 创建 Flow（无反压）
    ch_flow<ch_uint<8>> flow_io;
    flow_io.payload = make_uint<8>(0xBC);
    flow_io.valid = ch_bool(true);

    // 模拟一个简单的握手过程
    ch_stream<ch_uint<8>> master_stream;
    master_stream.payload = make_uint<8>(0xDE);
    master_stream.valid = ch_bool(true);
    master_stream.ready = ch_bool(false);

    ch_stream<ch_uint<8>> slave_stream;
    slave_stream.payload = make_uint<8>(0x00);  // 从设备的 payload 是输入，可以是任意值
    slave_stream.valid = ch_bool(false);        // 从设备的 valid 是输出
    slave_stream.ready = ch_bool(true);         // 从设备的 ready 是输入

    // 设置 Stream 为主设备方向 / Flow 为从设备方向
    // ⚠️ 方向指认 MUST 在 Simulator 构造**之前**完成（它决定 data_map_ 的槽位）
    stream_io.as_master();
    flow_io.as_slave();

    ch::Simulator sim(&ctx);
    sim.tick();

    std::cout << "Stream payload: 0x" << std::hex << sim.get_value(stream_io.payload) << std::endl;
    std::cout << "Stream valid: " << std::dec << sim.get_value(stream_io.valid) << std::endl;
    std::cout << "Stream ready: " << sim.get_value(stream_io.ready) << std::endl;

    std::cout << "Flow payload: 0x" << std::hex << sim.get_value(flow_io.payload) << std::endl;
    std::cout << "Flow valid: " << std::dec << sim.get_value(flow_io.valid) << std::endl;

    std::cout << "Stream configured as master" << std::endl;
    std::cout << "Flow configured as slave" << std::endl;

    // 演示 Stream 握手协议
    std::cout << "\nStream Handshake Example:" << std::endl;

    // 在实际应用中，master_stream.ready 会被 slave_stream.ready 驱动，
    // slave_stream.valid 会被 master_stream.valid 驱动。
    std::cout << "Master stream valid: " << sim.get_value(master_stream.valid) << std::endl;
    std::cout << "Slave stream ready: " << sim.get_value(slave_stream.ready) << std::endl;

    // 传输在 valid 和 ready 同时为高时发生
    // ⚠️ `sim.get_value()` 返回的是 `sdata_type`（宿主侧读回值）⇒ 这里在宿主侧
    //    计算握手结果。⚠️ `sdata_type` 的 `operator&` 不是位与（`operator<<`
    //    是左移）⇒ MUST NOT 直接用它做逻辑与。
    bool transfer_occurs =
        (static_cast<uint64_t>(sim.get_value(master_stream.valid)) != 0) &&
        (static_cast<uint64_t>(sim.get_value(slave_stream.ready)) != 0);
    std::cout << "Transfer occurs: " << transfer_occurs << std::endl;

    // 自检：master 的 valid=1 且 slave 的 ready=1 ⇒ 传输发生
    if (!transfer_occurs) {
        std::cerr << "ERROR: transfer must occur when valid & ready" << std::endl;
        return 1;
    }

    std::cout << "\nStream/Flow Basic example completed successfully!" << std::endl;

    return 0;
}
