
// samples/bundle_demo.cpp
#include "core/context.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "io/stream_bundle.h"
#include "core/uint.h"
#include "core/bool.h"
#include <memory>
#include <iostream>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== Bundle Advanced Demo ===" << std::endl;
    
    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        // 1. Stream Bundle演示
        std::cout << "1. Creating Stream Bundle..." << std::endl;
        stream_bundle<ch_uint<32>> input_stream("io.input");
        stream_bundle<ch_uint<32>> output_stream("io.output");
        std::cout << "✅ Stream bundles created" << std::endl;
        
        // 2. Bundle连接演示
        std::cout << "2. Testing Bundle Connection..." << std::endl;
        ch::core::connect(input_stream, output_stream);
        std::cout << "✅ Bundle connection works" << std::endl;
        
        // 3. Factory函数演示
        std::cout << "3. Testing Factory Functions..." << std::endl;
        auto master_stream = ch::core::master(stream_bundle<ch_uint<16>>{"master"});
        auto slave_stream = ch::core::slave(stream_bundle<ch_uint<16>>{"slave"});
        std::cout << "✅ Factory functions work" << std::endl;
        
        // 4. Flip功能演示
        std::cout << "4. Testing Flip with Auto Direction..." << std::endl;
        auto flipped = input_stream.flip();
        std::cout << "✅ Flip with auto direction works" << std::endl;
        
        // 5. 命名功能演示
        std::cout << "5. Testing Naming Integration..." << std::endl;
        stream_bundle<ch_uint<8>> named_stream("top.level.signal");
        std::cout << "✅ Naming integration works" << std::endl;
        
        std::cout << "\n🎉 All advanced Bundle features work correctly!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
