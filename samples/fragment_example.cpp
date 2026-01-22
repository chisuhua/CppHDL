#include "chlib/fragment.h"
#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;
using namespace chlib;

// 自定义Fragment示例
template<typename T>
struct CustomFragment : public bundle_base<CustomFragment<T>> {
    using Self = CustomFragment<T>;
    T fragment;  // 片段数据
    ch_bool last;  // 标记是否为最后一个片段
    ch_bool valid; // 有效标志

    CustomFragment() = default;
    explicit CustomFragment(const std::string& prefix = "custom_fragment") {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(fragment, last, valid)

    void as_master_direction() {
        this->make_output(fragment, last, valid);
    }

    void as_slave_direction() {
        this->make_input(fragment, last, valid);
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<context>("fragment_example");
    ctx_swap ctx_swapper(ctx.get());
    
    std::cout << "CppHDL Fragment Example" << std::endl;
    std::cout << "=======================" << std::endl;
    
    // 创建Fragment实例
    FragmentBundle<ch_uint<16>> frag_master;
    FragmentBundle<ch_uint<16>> frag_slave;
    
    frag_master.as_master();
    frag_slave.as_slave();
    
    // 设置名称前缀
    frag_master.set_name_prefix("frag_master");
    frag_slave.set_name_prefix("frag_slave");
    
    std::cout << "Fragment master role: " << static_cast<int>(frag_master.get_role()) << std::endl;
    std::cout << "Fragment slave role: " << static_cast<int>(frag_slave.get_role()) << std::endl;
    
    std::cout << "Fragment master width: " << frag_master.width() << std::endl;
    std::cout << "Fragment slave width: " << frag_slave.width() << std::endl;
    
    // 创建自定义Fragment
    CustomFragment<ch_uint<8>> custom_frag;
    custom_frag.as_master();
    
    std::cout << "Custom fragment role: " << static_cast<int>(custom_frag.get_role()) << std::endl;
    std::cout << "Custom fragment width: " << custom_frag.width() << std::endl;

    // 创建仿真器
    ch::Simulator sim(ctx.get());
    
    std::cout << "Fragment Example completed successfully!" << std::endl;
    
    return 0;
}