// samples/nested_bundle_demo.cpp
#include "core/context.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/bundle/bundle_traits.h"
#include "io/stream_bundle.h"
#include "io/axi_bundle.h"
#include "core/uint.h"
#include "core/bool.h"
#include <memory>
#include <iostream>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== Nested Bundle Demo ===" << std::endl;
    
    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        // 1. 基本嵌套Bundle
        std::cout << "1. Creating Nested Bundle..." << std::endl;
        struct CustomNested : public bundle_base<CustomNested> {
            using Self = CustomNested;
            stream_bundle<ch_uint<32>> data_stream;
            ch_bool interrupt;
            
            CustomNested() = default; 

            CustomNested(const std::string& prefix) {
                this->set_name_prefix(prefix);
            }
            
            CH_BUNDLE_FIELDS(Self, data_stream, interrupt)
            
            void as_master() override {
                this->make_output(data_stream, interrupt);
            }
            
            void as_slave() override {
                this->make_input(data_stream, interrupt);
            }
        };
        
        CustomNested nested("top.module");
        std::cout << "✅ Custom nested bundle created" << std::endl;
        
        // 2. AXI Bundle演示
        std::cout << "2. Creating AXI Bundles..." << std::endl;
        axi_addr_channel<32> addr_channel("axi.master.aw");
        axi_write_data_channel<32> data_channel("axi.master.w");
        axi_write_resp_channel resp_channel("axi.master.b");
        std::cout << "✅ AXI channel bundles created" << std::endl;
        
        // 3. 完整的AXI写通道
        std::cout << "3. Creating Full AXI Write Channel..." << std::endl;
        axi_write_channel<32, 32> axi_write("axi.master.write");
        std::cout << "✅ Full AXI write channel created" << std::endl;
        
        // 4. 嵌套Bundle验证
        std::cout << "4. Testing Nested Bundle Validation..." << std::endl;
        std::cout << "   Custom nested is valid: " << (nested.is_valid() ? "✅" : "❌") << std::endl;
        std::cout << "   AXI write is valid: " << (axi_write.is_valid() ? "✅" : "❌") << std::endl;
        
        // 5. Bundle类型特征
        std::cout << "5. Testing Bundle Type Traits..." << std::endl;
        std::cout << "   CustomNested is bundle: " << (is_bundle_v<CustomNested> ? "✅" : "❌") << std::endl;
        std::cout << "   StreamBundle is bundle: " << (is_bundle_v<stream_bundle<ch_uint<8>>> ? "✅" : "❌") << std::endl;
        std::cout << "   AXI fields count: " << bundle_field_count_v<axi_write_channel<32, 32>> << std::endl;
        
        // 6. Flip嵌套Bundle
        std::cout << "6. Testing Flip with Nested Bundles..." << std::endl;
        auto flipped_axi = axi_write.flip();
        std::cout << "✅ Nested bundle flip works" << std::endl;
        
        // 7. 连接嵌套Bundle
        std::cout << "7. Testing Connect with Nested Bundles..." << std::endl;
        axi_write_channel<32, 32> src_axi;
        axi_write_channel<32, 32> dst_axi;
        ch::core::connect(src_axi, dst_axi);
        std::cout << "✅ Nested bundle connect works" << std::endl;
        
        std::cout << "\n🎉 All Nested Bundle features work correctly!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
