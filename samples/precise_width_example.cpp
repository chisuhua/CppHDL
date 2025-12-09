// samples/precise_width_example.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include "../include/core/literal.h"  // 添加这一行以支持字面量操作符如1_d
#include <iostream>

// 添加STATIC_REQUIRE宏定义
#define STATIC_REQUIRE(expr) static_assert(expr, #expr)

using namespace ch::core;

// 示例模块：展示精确宽度支持
template<unsigned N>
class PreciseWidthExample : public ch::Component {
public:
    __io(
        ch_in<ch_uint<N>> in_data;
        ch_out<ch_uint<N+1>> out_data;  // 输出宽度比输入大1
    )

    PreciseWidthExample(ch::Component* parent = nullptr, const std::string& name = "precise_width_example")
        : ch::Component(parent, name)
    {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 展示精确宽度操作
        auto incremented = io().in_data + 1_d;  // 结果宽度应该是N+1
        io().out_data = incremented;
    }
};

int main() {
    std::cout << "=== Precise Width Support Example ===" << std::endl;
    
    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        // 测试不同宽度的ch_uint
        std::cout << "1. Testing ch_uint<3> operations..." << std::endl;
        ch_uint<3> a(0b101, "a");  // 3位
        ch_uint<3> b(0b011, "b");  // 3位
        
        // 加法操作应该产生4位结果
        auto sum = a + b;
        STATIC_REQUIRE(ch_width_v<decltype(sum)> == 4);
        std::cout << "   Sum result width: " << ch_width_v<decltype(sum)> << " bits" << std::endl;
        std::cout << "   Expected width: 4 bits" << std::endl;
        std::cout << "   Test " << (ch_width_v<decltype(sum)> == 4 ? "PASSED" : "FAILED") << std::endl;
        
        std::cout << "\n2. Testing ch_uint<7> operations..." << std::endl;
        ch_uint<7> c(0b1010101, "c");  // 7位
        ch_uint<5> d(0b11010, "d");    // 5位
        
        // 加法操作应该产生8位结果 (max(7,5)+1)
        auto sum2 = c + d;
        STATIC_REQUIRE(ch_width_v<decltype(sum2)> == 8);
        std::cout << "   Sum result width: " << ch_width_v<decltype(sum2)> << " bits" << std::endl;
        std::cout << "   Expected width: 8 bits" << std::endl;
        std::cout << "   Test " << (ch_width_v<decltype(sum2)> == 8 ? "PASSED" : "FAILED") << std::endl;
        
        std::cout << "\n3. Testing concat operations..." << std::endl;
        ch_uint<3> e(0b101, "e");  // 3位
        ch_uint<5> f(0b11010, "f");  // 5位
        
        // 拼接操作应该产生8位结果
        auto concatenated = concat(e, f);
        STATIC_REQUIRE(ch_width_v<decltype(concatenated)> == 8);
        std::cout << "   Concat result width: " << ch_width_v<decltype(concatenated)> << " bits" << std::endl;
        std::cout << "   Expected width: 8 bits" << std::endl;
        std::cout << "   Test " << (ch_width_v<decltype(concatenated)> == 8 ? "PASSED" : "FAILED") << std::endl;
        
        std::cout << "\n4. Testing bits extraction..." << std::endl;
        ch_uint<12> g(0b101101011100, "g");  // 12位
        
        // 提取位[7:4]应该产生4位结果
        auto slice = bits<ch_uint<12>, 7, 4>(g);
        STATIC_REQUIRE(ch_width_v<decltype(slice)> == 4);
        std::cout << "   Bits extract result width: " << ch_width_v<decltype(slice)> << " bits" << std::endl;
        std::cout << "   Expected width: 4 bits" << std::endl;
        std::cout << "   Test " << (ch_width_v<decltype(slice)> == 4 ? "PASSED" : "FAILED") << std::endl;
        
        std::cout << "\n5. Testing module with precise widths..." << std::endl;
        ch_device<PreciseWidthExample<4>> device;
        Simulator simulator(device.context());
        
        // 设置输入值
        simulator.set_port_value(device.instance().io().in_data, 0b1010);  // 10 in decimal
        
        // 运行仿真
        simulator.tick();
        
        // 获取输出值
        auto output_value = simulator.get_port_value(device.instance().io().out_data);
        std::cout << "   Input value: " << 0b1010 << " (decimal)" << std::endl;
        std::cout << "   Output value: " << static_cast<uint64_t>(output_value) << " (decimal)" << std::endl;
        std::cout << "   Expected output: " << (0b1010 + 1) << " (decimal)" << std::endl;
        std::cout << "   Test " << (static_cast<uint64_t>(output_value) == (0b1010 + 1) ? "PASSED" : "FAILED") << std::endl;
        
        std::cout << "\n✅ All precise width tests completed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}