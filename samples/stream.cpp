// samples/stream.cpp - 使用新操作符支持的版本
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/bundle.h"
#include "../include/codegen.h"
#include <iostream>

using namespace ch::core;

// Producer 模块 - 使用 Stream Bundle
template<typename T>
class Producer : public ch::Component {
public:
    __io(
        ch::stream<T> out;  // 输出流接口
        ch_out<ch_uint<1>> debug_valid;
        ch_out<T> debug_data;
    )

    Producer(ch::Component* parent = nullptr, const std::string& name = "producer")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
        CHDBG("Producer IO created");
    }

    void describe() override {
        CHDBG_FUNC();
        
        // 创建一个计数器作为数据源
        ch_reg<ch_uint<8>> data_counter(0);
        data_counter->next = data_counter + 1;
        
        // 创建有效信号 - 使用按位取反实现翻转
        ch_reg<ch_uint<1>> valid_reg(0);
        valid_reg->next = ~valid_reg;  // 按位取反实现翻转
        
        // 连接到输出端口
        io().out.data = data_counter;
        io().out.valid = valid_reg;
        
        // 调试输出
        io().debug_valid = valid_reg;
        io().debug_data = data_counter;
        
        CHDBG("Producer logic described");
    }
};

// Consumer 模块 - 使用 Stream Bundle
template<typename T>
class Consumer : public ch::Component {
public:
    __io(
        ch::stream<T> in;  // 输入流接口
        ch_out<T> received_data;
        ch_out<ch_uint<1>> received_valid;
    )

    Consumer(ch::Component* parent = nullptr, const std::string& name = "consumer")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
        CHDBG("Consumer IO created");
    }

    void describe() override {
        CHDBG_FUNC();
        
        // 读取输入信号
        auto data_ln = io().in.data;
        auto valid_ln = io().in.valid;
        auto ready_ln = io().in.ready;
        
        // 创建数据寄存器存储接收到的数据
        ch_reg<ch_uint<8>> data_storage(0);
        
        // 使用条件赋值更新数据存储
        if(ch::when(valid_ln & ready_ln)) {
            data_storage->next = data_ln;
        }
        
        // 简单的就绪信号 - 总是准备好
        //io().in.ready = ch_uint<1>(1);
        io().in.ready = 1;
        
        // 调试输出
        io().received_data = data_storage;
        io().received_valid = valid_ln & ready_ln;
        
        CHDBG("Consumer logic described");
    }
};

// Pipeline Stage 模块 - 流水线级
template<typename T>
class PipelineStage : public ch::Component {
public:
    __io(
        ch::stream<T> input;   // 输入流
        ch::stream<T> output;  // 输出流
    )

    PipelineStage(ch::Component* parent = nullptr, const std::string& name = "pipeline_stage")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
        CHDBG("PipelineStage IO created");
    }

    void describe() override {
        CHDBG_FUNC();
        
        // 流水线寄存器
        ch_reg<T> data_reg(0);
        ch_reg<ch_uint<1>> valid_reg(0);
        
        // 流水线逻辑
        if(ch::when(io().input.ready)) {
            data_reg->next = io().input.data;
            valid_reg->next = io().input.valid;
        }
        
        // 连接到输出
        io().output.data = data_reg;
        io().output.valid = valid_reg;
        io().input.ready = io().output.ready;
        
        CHDBG("PipelineStage logic described");
    }
};

// 顶层模块连接 Producer、Pipeline 和 Consumer
class Top : public ch::Component {
public:
    __io(
        ch_out<ch_uint<8>> debug_producer_data;
        ch_out<ch_uint<1>> debug_producer_valid;
        ch_out<ch_uint<8>> debug_consumer_data;
        ch_out<ch_uint<1>> debug_consumer_valid;
    )

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type();
        CHDBG("Top IO created");
    }

    void describe() override {
        CHDBG_FUNC();
        
        // 实例化模块
        CH_MODULE(Producer<ch_uint<8>>, producer);
        CH_MODULE(PipelineStage<ch_uint<8>>, pipeline);
        CH_MODULE(Consumer<ch_uint<8>>, consumer);
        
        // 连接 Stream 接口 - 使用端口翻转
        // Producer -> Pipeline
        producer.io().out.data = pipeline.io().input.data.flip();
        producer.io().out.valid = pipeline.io().input.valid.flip();
        pipeline.io().input.ready = producer.io().out.ready.flip();
        
        // Pipeline -> Consumer
        pipeline.io().output.data = consumer.io().in.data.flip();
        pipeline.io().output.valid = consumer.io().in.valid.flip();
        consumer.io().in.ready = pipeline.io().output.ready.flip();
        
        // 调试输出
        io().debug_producer_data = producer.io().debug_data;
        io().debug_producer_valid = producer.io().debug_valid;
        io().debug_consumer_data = consumer.io().received_data;
        io().debug_consumer_valid = consumer.io().received_valid;
        
        CHDBG("Top logic described");
    }
};

int main() {
    std::cout << "=== Stream Bundle Example with New Operations ===" << std::endl;
    
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());
    
    std::cout << "Starting simulation..." << std::endl;
    
    // 运行几个周期观察数据流
    for (int i = 0; i < 20; ++i) {
        sim.tick();
        
        auto producer_data = sim.get_port_value(top_device.instance().io().debug_producer_data);
        auto producer_valid = sim.get_port_value(top_device.instance().io().debug_producer_valid);
        auto consumer_data = sim.get_port_value(top_device.instance().io().debug_consumer_data);
        auto consumer_valid = sim.get_port_value(top_device.instance().io().debug_consumer_valid);
        
        std::cout << "Cycle " << i 
                  << ": Producer(data=" << producer_data.to_string_dec() 
                  << ", valid=" << producer_valid.to_string_dec() << ")"
                  << " -> Consumer(data=" << consumer_data.to_string_dec() 
                  << ", valid=" << consumer_valid.to_string_dec() << ")"
                  << std::endl;
    }
    
    // 生成 Verilog
    ch::toVerilog("stream_example.v", top_device.context());
    std::cout << "Generated Verilog file: stream_example.v" << std::endl;
    
    std::cout << "Simulation completed." << std::endl;
    return 0;
}
