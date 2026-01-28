
// samples/bundle_demo.cpp
#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 自定义Bundle示例
template <typename T>
struct CustomBundle : public bundle_base<CustomBundle<T>> {
    using Self = CustomBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;

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
    auto ctx = std::make_unique<context>("bundle_demo");
    ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Bundle Demo" << std::endl;
    std::cout << "==================" << std::endl;

    // 创建Bundle实例
    CustomBundle<ch_uint<8>> bundle_master;
    CustomBundle<ch_uint<8>> bundle_slave;

    // 设置角色
    bundle_master.as_master();
    bundle_slave.as_slave();

    // 设置名称前缀
    bundle_master.set_name_prefix("master");
    bundle_slave.set_name_prefix("slave");

    std::cout << "Bundle master role: "
              << static_cast<int>(bundle_master.get_role()) << std::endl;
    std::cout << "Bundle slave role: "
              << static_cast<int>(bundle_slave.get_role()) << std::endl;

    std::cout << "Bundle master width: " << bundle_master.width() << std::endl;
    std::cout << "Bundle slave width: " << bundle_slave.width() << std::endl;

    // 创建Stream Bundle
    ch_stream<ch_uint<16>> stream_master;
    ch_stream<ch_uint<16>> stream_slave;

    stream_master.as_master();
    stream_slave.as_slave();

    std::cout << "Stream master role: "
              << static_cast<int>(stream_master.get_role()) << std::endl;
    std::cout << "Stream slave role: "
              << static_cast<int>(stream_slave.get_role()) << std::endl;

    std::cout << "Stream master width: " << stream_master.width() << std::endl;
    std::cout << "Stream slave width: " << stream_slave.width() << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());

    std::cout << "Bundle Demo completed successfully!" << std::endl;

    return 0;
}
