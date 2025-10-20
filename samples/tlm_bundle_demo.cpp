// samples/tlm_bundle_demo.cpp
#ifdef USE_SYSTEMC_TLM

#include "core/context.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_serialization.h"
#include "io/stream_bundle.h"
#include "tlm/tlm_bundle_converter.h"
#include <memory>
#include <iostream>
#include <systemc.h>
#include <tlm.h>

using namespace ch;
using namespace ch::core;
using namespace ch::tlm_integration;

// 简单的SystemC模块示例
SC_MODULE(SimpleInitiator) {
    tlm_utils::simple_initiator_socket<SimpleInitiator> socket;
    
    SC_CTOR(SimpleInitiator) : socket("socket") {
        SC_THREAD(run);
    }
    
    void run() {
        // 创建测试Bundle
        stream_bundle<ch_uint<32>> test_bundle;
        test_bundle.payload = ch_uint<32>(12345678);
        test_bundle.valid = ch_bool(true);
        
        std::cout << "=== TLM-Bundle Demo ===" << std::endl;
        std::cout << "Bundle width: " << test_bundle.width() << " bits" << std::endl;
        std::cout << "Bundle payload: " << static_cast<uint64_t>(test_bundle.payload) << std::endl;
        
        // 序列化测试
        auto bits = serialize(test_bundle);
        std::cout << "Serialized to " << bits.width << " bits" << std::endl;
        
        // TLM转换测试
        auto* payload = bundle_tlm_converter<stream_bundle<ch_uint<32>>>::bundle_to_tlm(test_bundle);
        std::cout << "TLM payload created with " << payload->get_data_length() << " bytes" << std::endl;
        
        // 反向转换测试
        auto recovered_bundle = bundle_tlm_converter<stream_bundle<ch_uint<32>>>::tlm_to_bundle(payload);
        std::cout << "Recovered payload: " << static_cast<uint64_t>(recovered_bundle.payload) << std::endl;
        
        // 清理
        bundle_tlm_converter<stream_bundle<ch_uint<32>>>::cleanup_tlm_payload(payload);
        
        std::cout << "✅ TLM-Bundle conversion demo completed!" << std::endl;
    }
};

// 主函数
int sc_main(int argc, char* argv[]) {
    // 创建CppHDL上下文
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        SimpleInitiator initiator("initiator");
        
        std::cout << "Starting TLM-Bundle demo..." << std::endl;
        sc_start(100, SC_NS);
        std::cout << "Demo completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

#else

// 如果没有SystemC TLM，提供简单的测试
#include "core/context.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_serialization.h"
#include "io/stream_bundle.h"
#include <memory>
#include <iostream>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== Bundle Serialization Demo (No TLM) ===" << std::endl;
    
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    try {
        // 测试Bundle序列化
        stream_bundle<ch_uint<32>> test_bundle;
        test_bundle.payload = ch_uint<32>(12345678_h);
        test_bundle.valid = ch_bool(true);
        
        std::cout << "Bundle width: " << test_bundle.width() << " bits" << std::endl;
        std::cout << "Bundle payload: " << static_cast<uint64_t>(test_bundle.payload) << std::endl;
        
        // 序列化测试
        auto bits = serialize(test_bundle);
        std::cout << "Serialized to ch_uint<" << bits.width << ">" << std::endl;
        
        // 反序列化测试
        auto recovered_bundle = deserialize<stream_bundle<ch_uint<32>>>(bits);
        std::cout << "Recovered payload: " << static_cast<uint64_t>(recovered_bundle.payload) << std::endl;
        
        std::cout << "✅ Bundle serialization demo completed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

#endif // USE_SYSTEMC_TLM
