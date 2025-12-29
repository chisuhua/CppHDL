#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("stream_flow_basic_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Stream/Flow Basic Example:" << std::endl;

    // 创建Stream (带反压)
    Stream<ch_uint<8>> stream_io("my_stream");
    stream_io.payload = 0x5A_d;
    stream_io.valid = true;
    stream_io.ready = false;  // 初始时未就绪

    std::cout << "Stream payload: 0x" << std::hex << simulator.get_value(stream_io.payload) << std::endl;
    std::cout << "Stream valid: " << std::dec << simulator.get_value(stream_io.valid) << std::endl;
    std::cout << "Stream ready: " << simulator.get_value(stream_io.ready) << std::endl;

    // 设置Stream为主设备方向
    stream_io.as_master();
    std::cout << "Stream configured as master" << std::endl;

    // 创建Flow (无反压)
    Flow<ch_uint<8>> flow_io("my_flow");
    flow_io.payload = 0xBC_d;
    flow_io.valid = true;

    std::cout << "\nFlow payload: 0x" << std::hex << simulator.get_value(flow_io.payload) << std::endl;
    std::cout << "Flow valid: " << std::dec << simulator.get_value(flow_io.valid) << std::endl;

    // 设置Flow为从设备方向
    flow_io.as_slave();
    std::cout << "Flow configured as slave" << std::endl;

    // 演示Stream握手协议
    std::cout << "\nStream Handshake Example:" << std::endl;
    
    // 模拟一个简单的握手过程
    Stream<ch_uint<8>> master_stream;
    master_stream.payload = 0xDE_d;
    master_stream.valid = true;
    master_stream.ready = false;
    
    Stream<ch_uint<8>> slave_stream;
    slave_stream.payload = 0x00_d;  // 从设备的payload是输入，可以是任意值
    slave_stream.valid = false;     // 从设备的valid是输出
    slave_stream.ready = true;      // 从设备的ready是输入
    
    // 在实际应用中，master_stream.ready会被slave_stream.ready驱动
    // slave_stream.valid会被master_stream.valid驱动
    // 这里我们简单展示如何连接它们
    std::cout << "Master stream valid: " << simulator.get_value(master_stream.valid) << std::endl;
    std::cout << "Slave stream ready: " << simulator.get_value(slave_stream.ready) << std::endl;
    
    // 传输在valid和ready同时为高时发生
    ch_bool transfer_occurs = simulator.get_value(master_stream.valid) && simulator.get_value(slave_stream.ready);
    std::cout << "Transfer occurs: " << transfer_occurs << std::endl;

    std::cout << "\nStream/Flow Basic example completed successfully!" << std::endl;

    return 0;
}