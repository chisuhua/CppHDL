#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_serialization.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {
/**
 * @brief ch_stream bundle - 带反压的数据流接口
 * 包含 payload, valid, ready 信号
 */
template <typename T> struct ch_stream : public bundle_base<ch_stream<T>> {
    using Self = ch_stream<T>;
    T payload;     // 数据载荷
    ch_bool valid; // 有效信号
    ch_bool ready; // 就绪信号

    ch_stream() = default;
    // explicit ch_stream(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master_direction() {
        // Master: 输出数据和有效信号，接收就绪信号
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // Slave: 接收数据和有效信号，输出就绪信号
        this->make_input(payload, valid);
        this->make_output(ready);
    }

    [[nodiscard]] ch_bool fire() const { return this->valid && this->ready; }
};
} // namespace ch