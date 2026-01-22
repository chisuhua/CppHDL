// samples/improved_bundle_demo.cpp
#include "bundle/fragment.h"
#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 自定义嵌套Bundle示例
template <typename T>
struct NestedBundle : public bundle_base<NestedBundle<T>> {
    using Self = NestedBundle<T>;

    ch::Stream<T> stream;
    ch_bool flag;

    NestedBundle() = default;
    explicit NestedBundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(stream, flag)

    void as_master() override {
        this->role_ = bundle_role::master;
        this->make_output(flag);
        // 对stream也应用master角色
        this->stream.as_master();
    }

    void as_slave() override {
        this->role_ = bundle_role::slave;
        this->make_input(flag);
        // 对stream也应用slave角色
        this->stream.as_slave();
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<context>("nested_bundle_demo");
    ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Nested Bundle Demo" << std::endl;
    std::cout << "=========================" << std::endl;

    // 创建嵌套Bundle实例
    NestedBundle<ch_uint<8>> nested_bundle_master;
    NestedBundle<ch_uint<8>> nested_bundle_slave;

    // 设置角色
    nested_bundle_master.as_master();
    nested_bundle_slave.as_slave();

    // 创建Fragment实例
    Fragment<ch_uint<16>> frag_master;
    Fragment<ch_uint<16>> frag_slave;

    frag_master.as_master();
    frag_slave.as_slave();

    // 设置名称前缀
    nested_bundle_master.set_name_prefix("nested_master");
    nested_bundle_slave.set_name_prefix("nested_slave");

    frag_master.set_name_prefix("frag_master");
    frag_slave.set_name_prefix("frag_slave");

    std::cout << "Bundle roles:" << std::endl;
    std::cout << "Nested bundle master role: "
              << static_cast<int>(nested_bundle_master.get_role()) << std::endl;
    std::cout << "Nested bundle slave role: "
              << static_cast<int>(nested_bundle_slave.get_role()) << std::endl;
    std::cout << "Fragment master role: "
              << static_cast<int>(frag_master.get_role()) << std::endl;
    std::cout << "Fragment slave role: "
              << static_cast<int>(frag_slave.get_role()) << std::endl;

    std::cout << "\nBundle widths:" << std::endl;
    std::cout << "Nested bundle master width: " << nested_bundle_master.width()
              << std::endl;
    std::cout << "Nested bundle slave width: " << nested_bundle_slave.width()
              << std::endl;
    std::cout << "Fragment master width: " << frag_master.width() << std::endl;
    std::cout << "Fragment slave width: " << frag_slave.width() << std::endl;

    // 演示递归连接功能
    std::cout << "\nDemonstrating recursive connection:" << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());

    // 检查bundle是否有效
    std::cout << "Master bundle is valid: " << nested_bundle_master.is_valid()
              << std::endl;
    std::cout << "Slave bundle is valid: " << nested_bundle_slave.is_valid()
              << std::endl;

    std::cout << "\nNested Bundle Demo completed successfully!" << std::endl;

    return 0;
}