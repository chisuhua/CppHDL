// samples/handshake_simple.cpp
#include "core/context.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/io.h"
#include <memory>
#include <iostream>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== Simple HandShake Test ===" << std::endl;
    
    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        // 测试基本IO端口创建
        ch_out<ch_uint<8>> payload("payload");
        ch_out<ch_bool> valid("valid");
        ch_in<ch_bool> ready("ready");
        
        std::cout << "✅ IO ports created successfully" << std::endl;
        std::cout << "   Payload name: " << payload.name() << std::endl;
        std::cout << "   Valid name: " << valid.name() << std::endl;
        std::cout << "   Ready name: " << ready.name() << std::endl;
        
        // 测试基本操作
        ch_uint<8> data(42_d);
        payload = data;
        valid = ch_bool(true);
        
        std::cout << "✅ Basic operations completed" << std::endl;
        std::cout << "✅ All simple tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
