// samples/handshake_final.cpp
#include "component.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/io.h"
#include "core/uint.h"
#include "utils/logger.h"
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

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master_direction() {
        // Master: 输出payload和valid，输入ready
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
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
        HandShake<ch_uint<8>> slave_bundle;

        // 设置方向
        master_bundle.as_master();
        slave_bundle.as_slave();

        master_bundle.payload = 0x55_h;
        master_bundle.valid = true;
        slave_bundle.ready = true;

        std::cout << "Bundle created successfully!" << std::endl;
        std::cout << "Master role: " << (int)master_bundle.get_role()
                  << std::endl;
        std::cout << "Slave role: " << (int)slave_bundle.get_role()
                  << std::endl;

        // 测试2: 连接
        slave_bundle <<= master_bundle;
        std::cout << "Connection established successfully!" << std::endl;

        // 测试3: 验证连接
        CHCHECK(master_bundle.get_role() == bundle_role::master, "Error");
        CHCHECK(slave_bundle.get_role() == bundle_role::slave, "Error");
        std::cout << "Direction setting verified!" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}