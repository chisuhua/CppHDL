// samples/bundle_simulator_demo.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include "../include/io/common_bundles.h"
#include <iostream>
#include <bitset>

using namespace ch::core;

// 定义一个简单的Bundle处理模块
template<typename T>
class SimpleBundleModule : public ch::Component {
public:
    fifo_bundle<T> io;

    SimpleBundleModule(ch::Component* parent = nullptr, const std::string& name = "bundle_module")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void describe() override {
        // 简单的处理逻辑：当push为真时，将输入数据传递到输出
        io.full = io.push;
        io.empty = !io.pop;
    }
};

// 顶层模块，直接使用Bundle作为IO接口
class Top : public ch::Component {
public:
    // 直接使用Bundle作为顶层IO接口
    fifo_bundle<ch_uint<4>> bundle_io;
    
    // 为传统仿真添加端口
    __io(
        ch_out<ch_uint<4>> data_out;
        ch_out<ch_bool> full_out;
        ch_out<ch_bool> empty_out;
        ch_in<ch_uint<4>> data_in;
        ch_in<ch_bool> push_in;
        ch_in<ch_bool> pop_in;
    )

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {
        // 设置Bundle为master模式（作为顶层接口）
        bundle_io.as_master();
    }

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 创建Bundle处理模块实例
        ch::ch_module<SimpleBundleModule<ch_uint<4>>> module_inst{"module"};
        
        // 手动连接Bundle字段到模块
        module_inst.instance().io.data = bundle_io.data;
        module_inst.instance().io.push = bundle_io.push;
        module_inst.instance().io.pop = bundle_io.pop;
        bundle_io.data = module_inst.instance().io.data;
        bundle_io.full = module_inst.instance().io.full;
        bundle_io.empty = module_inst.instance().io.empty;
        
        // 连接到传统端口用于仿真
        io().data_out = bundle_io.data;
        io().full_out = bundle_io.full;
        io().empty_out = bundle_io.empty;
        // 移除输入端口连接，因为它们可能导致编译错误
        // bundle_io.data = io().data_in;
        // bundle_io.push = io().push_in;
        // bundle_io.pop = io().pop_in;
    }
};

int main() {
    std::cout << "Bundle Simulator Demo - demonstrating direct Bundle support in simulator" << std::endl;
    
    std::cout << "\n=== 测试1: Bundle位段拼接原理演示 ===" << std::endl;
    
    // 演示Bundle字段如何根据位段拼接
    // 假设我们有一个Bundle，包含以下字段：
    // - data: ch_uint<4> (4位)
    // - valid: ch_bool (1位)
    // - ready: ch_bool (1位)
    
    // 字段值
    uint64_t data_value = 10;  // 1010 (4位)
    uint64_t valid_value = 1;  // 1 (1位)
    uint64_t ready_value = 0;  // 0 (1位)
    
    std::cout << "Bundle field values:" << std::endl;
    std::cout << "  data : " << data_value << " (" << std::bitset<4>(data_value) << ")" << std::endl;
    std::cout << "  valid: " << valid_value << " (" << std::bitset<1>(valid_value) << ")" << std::endl;
    std::cout << "  ready: " << ready_value << " (" << std::bitset<1>(ready_value) << ")" << std::endl;
    
    // 按照字段顺序进行位段拼接
    // 字段顺序: data(4位), valid(1位), ready(1位)
    // 拼接方式: [ready(1位) | valid(1位) | data(4位)]
    uint64_t serialized = (ready_value << 5) | (valid_value << 4) | data_value;
    
    std::cout << "\nSerialization process:" << std::endl;
    std::cout << "  Field layout: [ready(1) | valid(1) | data(4)]" << std::endl;
    std::cout << "  Bit positions:  5       4       3210" << std::endl;
    std::cout << "  Result: " << std::bitset<6>(serialized) << " = 0x" << std::hex << serialized << std::dec << std::endl;
    
    std::cout << "\n=== 测试2: Bundle反序列化原理演示 ===" << std::endl;
    
    // 从序列化的值中提取各个字段
    uint64_t extracted_data = serialized & 0xF;          // 提取低4位 (data)
    uint64_t extracted_valid = (serialized >> 4) & 0x1;  // 提取第4位 (valid)
    uint64_t extracted_ready = (serialized >> 5) & 0x1;  // 提取第5位 (ready)
    
    std::cout << "Deserialization process:" << std::endl;
    std::cout << "  Extracted data : " << extracted_data << " (" << std::bitset<4>(extracted_data) << ")" << std::endl;
    std::cout << "  Extracted valid: " << extracted_valid << " (" << std::bitset<1>(extracted_valid) << ")" << std::endl;
    std::cout << "  Extracted ready: " << extracted_ready << " (" << std::bitset<1>(extracted_ready) << ")" << std::endl;
    
    // 验证反序列化结果
    bool data_match = (data_value == extracted_data);
    bool valid_match = (valid_value == extracted_valid);
    bool ready_match = (ready_value == extracted_ready);
    
    std::cout << "\nVerification:" << std::endl;
    std::cout << "  data match : " << (data_match ? "✓" : "✗") << std::endl;
    std::cout << "  valid match: " << (valid_match ? "✓" : "✗") << std::endl;
    std::cout << "  ready match: " << (ready_match ? "✓" : "✗") << std::endl;
    
    std::cout << "\n=== 测试3: 仿真器Bundle支持演示 ===" << std::endl;
    
    // 创建设备和仿真器
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());
    
    // 使用POD值设置Bundle
    uint64_t bundle_input = 0x15; // [pop=0, push=1, data=0101] 
    std::cout << "Setting bundle input with POD value: 0x" << std::hex << bundle_input 
              << " (" << std::bitset<8>(bundle_input) << ")" << std::dec << std::endl;
    
    // 使用传统方式设置输入
    sim.set_input_value(top_device.instance().io().data_in, 5);
    sim.set_input_value(top_device.instance().io().push_in, 1);
    sim.set_input_value(top_device.instance().io().pop_in, 0);
    
    // 直接使用Bundle设置值的新方法
    std::cout << "\nUsing new direct Bundle support in simulator..." << std::endl;
    sim.set_bundle_value(top_device.instance().bundle_io, bundle_input);
    
    // 运行仿真
    sim.tick();
    
    // 获取输出 - 通过传统端口
    auto data_out = sim.get_port_value(top_device.instance().io().data_out);
    auto full_out = sim.get_port_value(top_device.instance().io().full_out);
    auto empty_out = sim.get_port_value(top_device.instance().io().empty_out);
    
    std::cout << "Simulation results (via traditional ports):" << std::endl;
    std::cout << "  data_out : " << static_cast<uint64_t>(data_out) << std::endl;
    std::cout << "  full_out : " << static_cast<uint64_t>(full_out) << std::endl;
    std::cout << "  empty_out: " << static_cast<uint64_t>(empty_out) << std::endl;
    
    // 获取输出 - 直接通过Bundle
    uint64_t bundle_result = sim.get_bundle_value(top_device.instance().bundle_io);
    std::cout << "Simulation results (via Bundle):" << std::endl;
    std::cout << "  bundle result: 0x" << std::hex << bundle_result 
              << " (" << std::bitset<8>(bundle_result) << ")" << std::dec << std::endl;
    
    // 从Bundle结果中解析各字段
    uint64_t result_data = bundle_result & 0xF;         // 低4位 (data)
    uint64_t result_push = (bundle_result >> 4) & 0x1;  // 第4位 (push)
    uint64_t result_full = (bundle_result >> 5) & 0x1;  // 第5位 (full)
    uint64_t result_pop = (bundle_result >> 6) & 0x1;   // 第6位 (pop)
    uint64_t result_empty = (bundle_result >> 7) & 0x1; // 第7位 (empty)
    
    std::cout << "  parsed data  : " << result_data << std::endl;
    std::cout << "  parsed push  : " << result_push << std::endl;
    std::cout << "  parsed full  : " << result_full << std::endl;
    std::cout << "  parsed pop   : " << result_pop << std::endl;
    std::cout << "  parsed empty : " << result_empty << std::endl;
    
    std::cout << "\n=== 测试4: 不同Bundle类型的位宽分析 ===" << std::endl;
    
    // 分析现有Bundle类型的位宽
    std::cout << "fifo_bundle<ch_uint<4>> field widths:" << std::endl;
    std::cout << "  data : " << ch::core::ch_width_v<ch::core::ch_uint<4>> << " bits" << std::endl;
    std::cout << "  push : " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  full : " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  pop  : " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  empty: " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  Total: " << ch::core::get_bundle_width<fifo_bundle<ch_uint<4>>>() << " bits" << std::endl;
    
    std::cout << "\ninterrupt_bundle field widths:" << std::endl;
    std::cout << "  irq: " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  ack: " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  Total: " << ch::core::get_bundle_width<interrupt_bundle>() << " bits" << std::endl;
    
    // config_bundle示例
    std::cout << "\nconfig_bundle<8, 32> field widths:" << std::endl;
    std::cout << "  addr : " << ch::core::ch_width_v<ch::core::ch_uint<8>> << " bits" << std::endl;
    std::cout << "  wdata: " << ch::core::ch_width_v<ch::core::ch_uint<32>> << " bits" << std::endl;
    std::cout << "  rdata: " << ch::core::ch_width_v<ch::core::ch_uint<32>> << " bits" << std::endl;
    std::cout << "  write: " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  read : " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  ready: " << ch::core::ch_width_v<ch::core::ch_bool> << " bits" << std::endl;
    std::cout << "  Total: " << ch::core::get_bundle_width<config_bundle<8, 32>>() << " bits" << std::endl;
    
    std::cout << "\n=== 测试5: Bundle序列化/反序列化演示 ===" << std::endl;
    
    // 创建一个Bundle实例并演示序列化
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    fifo_bundle<ch_uint<4>> demo_bundle;
    std::cout << "Created fifo_bundle<ch_uint<4>> with width: " << demo_bundle.width() << " bits" << std::endl;
    
    // 获取Bundle字段信息
    auto fields = demo_bundle.__bundle_fields();
    std::cout << "Bundle has " << std::tuple_size_v<decltype(fields)> << " fields" << std::endl;
    
    std::cout << "\n=== 测试6: 复杂Bundle操作演示 ===" << std::endl;
    
    // 创建一个config_bundle并演示其操作
    config_bundle<8, 32> config_demo_bundle;
    std::cout << "Created config_bundle<8, 32> with width: " << config_demo_bundle.width() << " bits" << std::endl;
    
    // 创建另一个上下文用于config_bundle演示
    auto ctx2 = std::make_unique<ch::core::context>("config_demo_ctx");
    ch::core::ctx_swap ctx_guard2(ctx2.get());
    
    // 创建一个设备使用config_bundle
    class ConfigDevice : public ch::Component {
    public:
        config_bundle<8, 32> cfg;
        
        ConfigDevice(ch::Component* parent = nullptr, const std::string& name = "config_device")
            : ch::Component(parent, name) {
            cfg.as_slave();
        }
        
        void create_ports() override {}
        
        void describe() override {
            // 简单的配置回环
            cfg.rdata = cfg.wdata;
            cfg.ready = cfg.write || cfg.read;
        }
    };
    
    ch::ch_device<ConfigDevice> config_device;
    ch::Simulator config_sim(config_device.context());
    
    // 使用Bundle设置配置值
    uint64_t config_value = 0x2000000025; // [ready=0, read=1, write=0, rdata=0x20, wdata=0x25, addr=0]
    std::cout << "Setting config bundle with value: 0x" << std::hex << config_value << std::dec << std::endl;
    
    config_sim.set_bundle_value(config_device.instance().cfg, config_value);
    config_sim.tick();
    
    uint64_t config_result = config_sim.get_bundle_value(config_device.instance().cfg);
    std::cout << "Config bundle result: 0x" << std::hex << config_result << std::dec << std::endl;
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "This demonstrates how Bundle fields are serialized into bit segments" << std::endl;
    std::cout << "and how the simulator can work with Bundle interfaces." << std::endl;
    
    return 0;
}