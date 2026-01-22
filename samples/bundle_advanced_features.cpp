// samples/advanced_bundle_demo.cpp
#include "bundle/common_bundles.h"
#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_operations.h"
#include "core/bundle/bundle_protocol.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include "simulator.h"
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;

// 自定义Bundle示例
template <typename T>
struct AdvancedBundle : public bundle_base<AdvancedBundle<T>> {
    using Self = AdvancedBundle<T>;
    T data;
    ch_bool enable;
    ch_bool ack;
    ch_bool extra_flag;

    CH_BUNDLE_FIELDS_T(data, enable, ack, extra_flag)

    void as_master_direction() {
        this->make_output(data, enable, extra_flag);
        this->make_input(ack);
    }

    void as_slave_direction() {
        this->make_input(data, enable, extra_flag);
        this->make_output(ack);
    }
};

int main() {
    std::cout << "CppHDL Bundle Advanced Features Demo" << std::endl;
    std::cout << "===================================" << std::endl;

    // 创建上下文
    auto ctx = std::make_unique<context>("bundle_advanced_features");
    ctx_swap ctx_swapper(ctx.get());

    std::cout << "Creating bundles and testing master/slave roles..."
              << std::endl;

    // 创建Bundle实例
    AdvancedBundle<ch_uint<16>> bundle_master;
    AdvancedBundle<ch_uint<16>> bundle_slave;

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

    // 测试FIFO Bundle
    ch::fifo_bundle<ch_uint<8>> fifo_bundle;
    fifo_bundle.as_master();

    std::cout << "FIFO bundle role: "
              << static_cast<int>(fifo_bundle.get_role()) << std::endl;
    std::cout << "FIFO bundle width: " << fifo_bundle.width() << std::endl;

    // 测试中断Bundle
    ch::interrupt_bundle interrupt_bundle;
    interrupt_bundle.as_master();

    std::cout << "Interrupt bundle role: "
              << static_cast<int>(interrupt_bundle.get_role()) << std::endl;
    std::cout << "Interrupt bundle width: " << interrupt_bundle.width()
              << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());

    std::cout << "Bundle Advanced Features Demo completed successfully!"
              << std::endl;

    return 0;
}
