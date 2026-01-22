// samples/bundle_simulator_demo.cpp
#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 自定义Bundle示例
template<typename T>
struct SimBundle : public bundle_base<SimBundle<T>> {
    using Self = SimBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;

    SimBundle() = default;
    explicit SimBundle(const std::string& prefix = "sim_bundle") {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(data, enable, ack)

    void as_master_direction() {
        this->make_output(data, enable);
        this->make_input(ack);
    }

    void as_slave_direction() {
        this->make_input(data, enable);
        this->make_output(ack);
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<context>("bundle_simulator_demo");
    ctx_swap ctx_swapper(ctx.get());
    
    std::cout << "CppHDL Bundle Simulator Demo" << std::endl;
    std::cout << "============================" << std::endl;
    
    // 创建Bundle实例
    SimBundle<ch_uint<8>> bundle_master;
    SimBundle<ch_uint<8>> bundle_slave;
    
    // 设置角色
    bundle_master.as_master();
    bundle_slave.as_slave();
    
    // 设置名称前缀
    bundle_master.set_name_prefix("master");
    bundle_slave.set_name_prefix("slave");
    
    std::cout << "Bundle master role: " << static_cast<int>(bundle_master.get_role()) << std::endl;
    std::cout << "Bundle slave role: " << static_cast<int>(bundle_slave.get_role()) << std::endl;
    
    std::cout << "Bundle master width: " << bundle_master.width() << std::endl;
    std::cout << "Bundle slave width: " << bundle_slave.width() << std::endl;
    
    // 测试FIFO Bundle
    ch::fifo_bundle<ch_uint<8>> fifo_bundle;
    fifo_bundle.as_master();
    
    std::cout << "FIFO bundle role: " << static_cast<int>(fifo_bundle.get_role()) << std::endl;
    std::cout << "FIFO bundle width: " << fifo_bundle.width() << std::endl;
    
    // 创建仿真器
    ch::Simulator sim(ctx.get());
    
    std::cout << "Bundle Simulator Demo completed successfully!" << std::endl;
    
    return 0;
}