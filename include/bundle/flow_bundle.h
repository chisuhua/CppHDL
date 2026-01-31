#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {

/**
 * ch_flow - 无反压的数据流接口
 * 包含 payload, valid 信号（无ready信号）
 */
template <typename T> struct ch_flow : public bundle_base<ch_flow<T>> {
    using Self = ch_flow<T>;
    T payload;     // 数据载荷
    ch_bool valid; // 有效信号

    ch_flow() = default;
    // explicit ch_flow(const std::string &prefix) {
    // this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(payload, valid)

    void as_master_direction() {
        // Master: 输出数据和有效信号
        this->make_output(payload, valid);
    }

    void as_slave_direction() {
        // Slave: 接收数据和有效信号
        this->make_input(payload, valid);
    }
    
    // Check if flow is firing (valid is true)
    [[nodiscard]] ch_bool fire() const { return this->valid; }
    
    // Convert to stream (add ready signal)
    ch_stream<T> toStream() const {
        ch_stream<T> result;
        result.payload = this->payload;
        result.valid = this->valid;
        result.ready = true; // Default to ready
        return result;
    }
};

} // namespace ch