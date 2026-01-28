// samples/nested_fragment_demo.cpp
#include "bundle/fragment.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace ch;

// 自定义嵌套Fragment示例
template <typename T>
struct NestedFragment : public bundle_base<NestedFragment<T>> {
    using Self = NestedFragment<T>;

    ch_fragment<T> fragment;
    ch_bool flag;

    NestedFragment() = default;
    // explicit NestedFragment(const std::string &prefix = "nested_frag") {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(fragment, flag)

    void as_master_direction() {
        this->make_output(flag);
        // 对fragment也应用master角色
        this->fragment.as_master();
    }

    void as_slave_direction() {
        this->make_input(flag);
        // 对fragment也应用slave角色
        this->fragment.as_slave();
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<context>("nested_bundle_demo");
    ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Nested Bundle Demo" << std::endl;
    std::cout << "=========================" << std::endl;

    // 创建嵌套Bundle实例
    NestedFragment<ch_uint<8>> nested_bundle_master;
    NestedFragment<ch_uint<8>> nested_bundle_slave;

    // 设置角色
    nested_bundle_master.as_master();
    nested_bundle_slave.as_slave();

    // 创建Fragment实例
    ch_fragment<ch_uint<16>> frag_master;
    ch_fragment<ch_uint<16>> frag_slave;

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