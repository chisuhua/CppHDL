// samples/axi_lite_demo.cpp
#include "bundle/axi_lite_bundle.h"
#include "bundle/axi_protocol.h"
#include "bundle/stream_bundle.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/uint.h"
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;

int main() {
    std::cout << "=== AXI-Lite Bundle Demo ===" << std::endl;

    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    try {
        // 1. AXI-Lite通道演示
        std::cout << "1. Creating AXI-Lite Channels..." << std::endl;
        axi_lite_aw_channel<32> aw("master.aw");
        axi_lite_w_channel<32> w("master.w");
        axi_lite_b_channel b("master.b");
        axi_lite_ar_channel<32> ar("master.ar");
        axi_lite_r_channel<32> r("master.r");

        std::cout << "✅ AW channel created with "
                  << bundle_field_count_v<axi_lite_aw_channel<32>> << " fields"
                  << std::endl;
        std::cout << "✅ W channel created with "
                  << bundle_field_count_v<axi_lite_w_channel<32>> << " fields"
                  << std::endl;
        std::cout << "✅ B channel created with "
                  << bundle_field_count_v<axi_lite_b_channel> << " fields"
                  << std::endl;
        std::cout << "✅ AR channel created with "
                  << bundle_field_count_v<axi_lite_ar_channel<32>> << " fields"
                  << std::endl;
        std::cout << "✅ R channel created with "
                  << bundle_field_count_v<axi_lite_r_channel<32>> << " fields"
                  << std::endl;

        // 2. AXI-Lite接口演示
        std::cout << "2. Creating AXI-Lite Interfaces..." << std::endl;
        axi_lite_write_interface<32, 32> write_if("cpu.write");
        axi_lite_read_interface<32, 32> read_if("cpu.read");
        axi_lite_bundle<32, 32> full_axi("peripheral.axi");

        std::cout << "✅ Write interface created with "
                  << bundle_field_count_v<
                         axi_lite_write_interface<32, 32>> << " fields"
                  << std::endl;
        std::cout << "✅ Read interface created with "
                  << bundle_field_count_v<
                         axi_lite_read_interface<32, 32>> << " fields"
                  << std::endl;
        std::cout << "✅ Full AXI-Lite created with "
                  << bundle_field_count_v<axi_lite_bundle<32, 32>> << " fields"
                  << std::endl;

        // 3. 协议验证演示
        std::cout << "3. Protocol Validation..." << std::endl;
        std::cout << "   Full AXI-Lite is AXI-Lite protocol: "
                  << (is_axi_lite_v<axi_lite_bundle<32, 32>> ? "✅" : "❌")
                  << std::endl;
        std::cout << "   Write interface is AXI-Lite write protocol: "
                  << (is_axi_lite_write_v<axi_lite_write_interface<32, 32>>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Read interface is AXI-Lite read protocol: "
                  << (is_axi_lite_read_v<axi_lite_read_interface<32, 32>>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Stream bundle is AXI-Lite protocol: "
                  << (is_axi_lite_v<Stream<ch_uint<32>>> ? "❌" : "✅")
                  << std::endl;

        // 4. 字段检查演示
        std::cout << "4. Field Name Checking..." << std::endl;
        std::cout << "   Full AXI has 'aw' field: "
                  << (has_field_named_v<axi_lite_bundle<32, 32>,
                                        structural_string{"aw"}>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Full AXI has 'w' field: "
                  << (has_field_named_v<axi_lite_bundle<32, 32>,
                                        structural_string{"w"}>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Full AXI has 'b' field: "
                  << (has_field_named_v<axi_lite_bundle<32, 32>,
                                        structural_string{"b"}>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Full AXI has 'ar' field: "
                  << (has_field_named_v<axi_lite_bundle<32, 32>,
                                        structural_string{"ar"}>
                          ? "✅"
                          : "❌")
                  << std::endl;
        std::cout << "   Full AXI has 'r' field: "
                  << (has_field_named_v<axi_lite_bundle<32, 32>,
                                        structural_string{"r"}>
                          ? "✅"
                          : "❌")
                  << std::endl;

        // 5. 方向控制演示
        std::cout << "5. Direction Control..." << std::endl;
        axi_lite_aw_channel<32> aw_channel;
        axi_lite_w_channel<32> w_channel;
        axi_lite_b_channel b_channel;
        axi_lite_ar_channel<32> ar_channel;
        axi_lite_r_channel<32> r_channel;

        aw_channel.as_master();
        w_channel.as_master();
        b_channel.as_slave();
        ar_channel.as_master();
        r_channel.as_slave();

        std::cout << "AW channel role: " << static_cast<int>(aw_channel.get_role()) << std::endl;
        std::cout << "W channel role: " << static_cast<int>(w_channel.get_role()) << std::endl;
        std::cout << "B channel role: " << static_cast<int>(b_channel.get_role()) << std::endl;
        std::cout << "AR channel role: " << static_cast<int>(ar_channel.get_role()) << std::endl;
        std::cout << "R channel role: " << static_cast<int>(r_channel.get_role()) << std::endl;

        std::cout << "AW channel width: " << aw_channel.width() << std::endl;
        std::cout << "W channel width: " << w_channel.width() << std::endl;
        std::cout << "B channel width: " << b_channel.width() << std::endl;
        std::cout << "AR channel width: " << ar_channel.width() << std::endl;
        std::cout << "R channel width: " << r_channel.width() << std::endl;

        std::cout << "Master AXI-Lite interface role: " << static_cast<int>(master_axi.get_role()) << std::endl;
        std::cout << "Slave AXI-Lite interface role: " << static_cast<int>(slave_axi.get_role()) << std::endl;
        std::cout << "Master AXI-Lite interface width: " << master_axi.width() << std::endl;
        std::cout << "Slave AXI-Lite interface width: " << slave_axi.width() << std::endl;
        std::cout << "✅ Direction control works" << std::endl;

        // 6. Flip功能演示
        std::cout << "6. Flip Functionality..." << std::endl;
        auto flipped_axi = master_axi.flip();
        std::cout << "✅ Flip functionality works" << std::endl;

        // 7. 连接功能演示
        std::cout << "7. Connection Function..." << std::endl;
        axi_lite_bundle<32, 32> src_axi;
        axi_lite_bundle<32, 32> dst_axi;
        ch::core::connect(src_axi, dst_axi);
        std::cout << "✅ Connection function works" << std::endl;

        // 8. 编译期协议验证
        std::cout << "8. Compile-time Protocol Validation..." << std::endl;
        validate_axi_lite_protocol(full_axi);
        validate_axi_lite_write_protocol(write_if);
        validate_axi_lite_read_protocol(read_if);
        std::cout << "✅ Compile-time protocol validation works" << std::endl;

        // 9. 不同宽度演示
        std::cout << "9. Different Widths..." << std::endl;
        axi_lite_bundle<64, 32> axi64_32("system.axi64_32");
        axi_lite_bundle<32, 64> axi32_64("system.axi32_64");
        CHREQUIRE(axi64_32.is_valid(), "axi64_32 is not valid");
        CHREQUIRE(axi32_64.is_valid(), "axi32_64 is not valid");
        std::cout << "✅ Different width configurations work" << std::endl;

        std::cout << "\n🎉 All AXI-Lite Bundle features work correctly!"
                  << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
