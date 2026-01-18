// samples/handshake_final.cpp
#include "component.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/io.h"
#include "core/uint.h"
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;

// HandShake Bundle - 使用无方向的类型
template <typename T> struct HandShake : public bundle_base<HandShake<T>> {
    using Self = HandShake<T>;
    // 使用无方向的中性成员
    T payload;
    ch_bool valid;
    ch_bool ready;

    HandShake() = default;
    HandShake(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS(Self, payload, valid, ready)

    void as_master() override {
        // Master: 输出payload和valid，输入ready
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        // Slave: 输入payload和valid，输出ready
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

// 简单的测试
int main() {
    std::cout << "=== HandShake Bundle Test (Final) ===" << std::endl;

    // 创建测试上下文
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    try {
        // 测试1: 基本创建
        HandShake<ch_uint<8>> master_bundle;
        std::cout << "✅ Master bundle created" << std::endl;

        // 测试2: 元数据
        auto fields = master_bundle.__bundle_fields();
        std::cout << "✅ Bundle has "
                  << std::tuple_size_v<decltype(fields)> << " fields"
                  << std::endl;

        // 测试3: flip功能
        auto slave_bundle = master_bundle.flip();
        std::cout << "✅ Bundle flipped successfully" << std::endl;

        // 测试4: 连接功能
        HandShake<ch_uint<8>> src_bundle;
        HandShake<ch_uint<8>> dst_bundle;
        ch::core::connect(src_bundle, dst_bundle);
        std::cout << "✅ Bundle connect successfully" << std::endl;

        // 测试5: Master/Slave工厂函数
        auto master_view = ch::core::master(HandShake<ch_uint<8>>{});
        auto slave_view = ch::core::slave(HandShake<ch_uint<8>>{});
        std::cout << "✅ Master/Slave factory functions work" << std::endl;

        std::cout << "✅ All Bundle tests passed!" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
