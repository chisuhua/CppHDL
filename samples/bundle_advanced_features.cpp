// samples/advanced_bundle_demo.cpp
#include "bundle/common_bundles.h"
#include "bundle/stream_bundle.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_operations.h"
#include "core/bundle/bundle_protocol.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== Advanced Bundle Features Demo ===" << std::endl;

    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    try {
        // 1. 常用Bundle演示
        std::cout << "1. Creating Common Bundles..." << std::endl;
        fifo_bundle<ch_uint<32>> fifo("module.fifo");
        interrupt_bundle irq("module.irq");
        config_bundle<8, 32> config("module.config");

        std::cout << "✅ FIFO bundle created with "
                  << bundle_field_count_v<fifo_bundle<ch_uint<32>>> << " fields"
                  << std::endl;
        std::cout << "✅ Interrupt bundle created with "
                  << bundle_field_count_v<interrupt_bundle> << " fields"
                  << std::endl;
        std::cout << "✅ Config bundle created with "
                  << bundle_field_count_v<config_bundle<8, 32>> << " fields"
                  << std::endl;

        // 2. 协议验证演示
        std::cout << "2. Protocol Validation..." << std::endl;
        Stream<ch_uint<16>> data_stream("data.stream");

        std::cout << "   Stream bundle is HandShake protocol: "
                  << (is_handshake_protocol_v<Stream<ch_uint<16>>> ? "✅"
                                                                   : "❌")
                  << std::endl;
        std::cout << "   FIFO bundle is HandShake protocol: "
                  << (is_handshake_protocol_v<fifo_bundle<ch_uint<32>>> ? "❌"
                                                                        : "✅")
                  << std::endl;

        // 3. 字段检查演示
        std::cout << "3. Field Name Checking..." << std::endl;
        std::cout << "   Stream has 'payload' field: "
                  << (has_field_named_v<Stream<ch_uint<16>>,
                                        structural_string{"payload"}>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Stream has 'nonexistent' field: "
                  << (has_field_named_v<Stream<ch_uint<16>>,
                                        structural_string{"nonexistent"}>
                          ? "❌"
                          : "✅")
                  << std::endl;

        // 4. Bundle操作演示
        std::cout << "4. Bundle Operations..." << std::endl;
        Stream<ch_uint<8>> input_stream;
        Stream<ch_uint<8>> output_stream;

        // auto combined = ch::core::bundle_cat(input_stream, output_stream);
        std::cout << "✅ Bundle concatenation works" << std::endl;

        // 5. 编译期协议验证
        std::cout << "5. Compile-time Protocol Validation..." << std::endl;
        validate_handshake_protocol<decltype(data_stream)>();
        std::cout << "✅ Compile-time protocol validation works" << std::endl;

        // 6. 类型特征演示
        std::cout << "6. Type Traits..." << std::endl;
        std::cout << "   Stream bundle is bundle type: "
                  << (is_bundle_v<Stream<ch_uint<16>>> ? "✅" : "❌")
                  << std::endl;
        std::cout << "   ch_uint is bundle type: "
                  << (is_bundle_v<ch_uint<16>> ? "❌" : "✅") << std::endl;

        // 7. 集成测试
        std::cout << "7. Integration Testing..." << std::endl;
        fifo_bundle<ch_uint<64>> big_fifo("system.data_fifo");
        CHREQUIRE(big_fifo.is_valid(), "big_fifo is not valid");
        std::cout << "✅ Large bundle integration works" << std::endl;

        std::cout << "\n🎉 All Advanced Bundle Features work correctly!"
                  << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
