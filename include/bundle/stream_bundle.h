#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {
/**
 * @brief Stream bundle - 带反压的数据流接口
 * 包含 payload, valid, ready 信号
 */
template <typename T> struct Stream : public bundle_base<Stream<T>> {
    using Self = Stream<T>;
    T payload;     // 数据载荷
    ch_bool valid; // 有效信号
    ch_bool ready; // 就绪信号

    // Stream() = default;
    // explicit Stream(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master() override {
        this->role_ = bundle_role::master;
        // Master: 输出数据和有效信号，接收就绪信号
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->role_ = bundle_role::slave;
        // Slave: 接收数据和有效信号，输出就绪信号
        this->make_input(payload, valid);
        this->make_output(ready);
    }

    // 获取bundle角色
    bundle_role get_role() const { return this->role_; }

    // 递归连接功能，用于处理嵌套bundle
    template <typename OtherStream>
    void do_recursive_connect(const OtherStream &src) {
        // 根据当前角色确定连接方向
        if (this->get_role() == bundle_role::master) {
            // 如果是主设备，输出payload和valid，输入ready
            if constexpr (is_bundle_v<T>) {
                bundle_field_traits<T>::connect(src.payload, this->payload);
            } else {
                this->payload = src.payload;
            }
            this->valid = src.valid;
            src.ready = this->ready; // 反向连接
        } else {
            // 如果是从设备，输入payload和valid，输出ready
            if constexpr (is_bundle_v<T>) {
                bundle_field_traits<T>::connect(src.payload, this->payload);
            } else {
                this->payload = src.payload;
            }
            this->valid = src.valid;
            this->ready = src.ready;
        }
    }
};
} // namespace ch