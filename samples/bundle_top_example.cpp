// samples/bundle_top_example.cpp
//
// Bundle 的推荐用法示例：将 Bundle 作为 Component 的直接成员
// 而非通过 __io() 宏和 bundle_port_expanded 包装
//
// 设计模式：
// 1. Producer 组件：有 ch_stream<T> as_master()，产生数据
// 2. Consumer 组件：有 ch_stream<T> as_slave()，消费数据  
// 3. 顶层模块：有 ch_stream<T> 成员作为外部接口，用 <<= 连接子模块和内部流
// 4. 仿真：直接对顶层 bundle 字段调用 set_value()/get_value()
//

#include "bundle/common_bundles.h"
#include "bundle/stream_bundle.h"
#include "bundle/flow_bundle.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "core/bundle/bundle_utils.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace ch;
// ============================================================================
// Producer：产生数据流的组件
// ============================================================================
template <typename T> class Producer : public ch::Component {
public:
    ch::ch_stream<T> stream_out;

    Producer(ch::Component *parent = nullptr,
             const std::string &name = "producer")
        : ch::Component(parent, name), stream_out() {
        stream_out.as_master();
    }

    void describe() override {
        ch_reg<T> counter(0_d, "counter");
        auto fire = stream_out.valid && stream_out.ready;
        counter->next = select(fire, counter + 1_b, counter);
        stream_out.payload = counter;
        stream_out.valid = 1_b;
    }
};

// ============================================================================
// Consumer：消费数据流并输出结果到 out_flow
// ============================================================================
template <typename T> class Consumer : public ch::Component {
public:
    ch::ch_stream<T> stream_in;   // 接收流（Slave）
    ch::ch_flow<T> out_flow;      // 输出结果（Master）

    Consumer(ch::Component *parent = nullptr,
             const std::string &name = "consumer")
        : ch::Component(parent, name), stream_in(), out_flow() {
        stream_in.as_slave();
        out_flow.as_master();
    }

    void describe() override {
        ch_reg<T> received_count(0_d, "received_count");
        auto data_received = stream_in.valid;
        received_count->next = select(data_received, received_count + 1_b, received_count);
        stream_in.ready = 1_b;

        // 将处理结果通过 out_flow 输出
        out_flow.payload = received_count;
        out_flow.valid = stream_in.valid;
    }
};

// ============================================================================
// 顶层模块：连接 Producer 和 Consumer，并暴露用于仿真的顶层 bundles
// ============================================================================
class Top : public ch::Component {
public:
    // 顶层外部接口：用于由 Simulator 驱动 consumer2
    ch::ch_stream<ch_uint<8>> ext_stream;

    // 顶层用于收集 consumer 输出，供 Simulator 读取
    ch::ch_flow<ch_uint<8>> result1; // 连接 consumer1.out_flow
    ch::ch_flow<ch_uint<8>> result2; // 连接 consumer2.out_flow

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name), ext_stream(), result1(), result2() {
        ext_stream.as_master();
        result1.as_slave();
        result2.as_slave();
    }

    void describe() override {
        // 在描述阶段创建子模块并连接
        ch::ch_module<Producer<ch_uint<8>>> producer_inst{"producer"};
        ch::ch_module<Consumer<ch_uint<8>>> consumer1_inst{"consumer1"};
        ch::ch_module<Consumer<ch_uint<8>>> consumer2_inst{"consumer2"};

        // producer -> consumer1
        consumer1_inst.instance().stream_in <<= producer_inst.instance().stream_out;

        // consumer2 由顶层 ext_stream 驱动（Simulator 写到 ext_stream）
        consumer2_inst.instance().stream_in <<= ext_stream;

        // 将 consumer 的输出连接回顶层的 result bundles，供仿真读取
        result1 <<= consumer1_inst.instance().out_flow;
        result2 <<= consumer2_inst.instance().out_flow;
    }
};

// ============================================================================
// main: 运行仿真，演示如何由 Simulator 写入 ext_stream，读取 result bundles
// ============================================================================
int main() {
    std::cout << "=== Bundle Stream Producer/Consumer Example (refined) ===" << std::endl;

    // 创建顶层设备
    ch::ch_device<Top> top_device;

    // 获取仿真器
    ch::Simulator sim(top_device.context());

    // 演示：在 cycle 0，通过 Simulator 向 ext_stream 写入一个 bundle（payload=5, valid=1）
    // 假设 ch_flow<ch_uint<8>> 的序列化为: low 8 bits = payload, bit 8 = valid
    uint64_t ext_value = (1ULL << 8) | 5ULL; // valid=1, payload=5
    sim.set_bundle_value(top_device.instance().ext_stream, ext_value);

    std::cout << "Running simulation for 6 cycles..." << std::endl;
    for (int cycle = 0; cycle < 6; ++cycle) {
        std::cout << "Cycle " << cycle << ": ";
        sim.tick();

        // 读取顶层 result bundles
        uint64_t r1 = sim.get_bundle_value(top_device.instance().result1);
        uint64_t r2 = sim.get_bundle_value(top_device.instance().result2);

        uint64_t payload1 = r1 & 0xff;
        uint64_t valid1 = (r1 >> 8) & 0x1;
        uint64_t payload2 = r2 & 0xff;
        uint64_t valid2 = (r2 >> 8) & 0x1;

        std::cout << "result1(payload=" << payload1 << ", valid=" << valid1 << ") ";
        std::cout << "result2(payload=" << payload2 << ", valid=" << valid2 << ")";
        std::cout << std::endl;

        // 在 cycle 2，再次刺激 ext_stream
        if (cycle == 2) {
            uint64_t ext_value2 = (1ULL << 8) | 42ULL; // payload=42
            sim.set_bundle_value(top_device.instance().ext_stream, ext_value2);
        }
    }

    // 生成 Verilog（可选）
    try {
        ch::toVerilog("bundle_stream_example.v", top_device.context());
        std::cout << "Generated: bundle_stream_example.v" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Verilog generation skipped: " << e.what() << std::endl;
    }

    return 0;
}