#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {

// FIFO Bundle - 用于表示FIFO接口
template <typename T> struct fifo_bundle : public bundle_base<fifo_bundle<T>> {
    using Self = fifo_bundle<T>;
    T data;        // 数据
    ch_bool push;  // 推入信号
    ch_bool full;  // 满信号
    ch_bool pop;   // 弹出信号
    ch_bool empty; // 空信号

    fifo_bundle() = default;
    explicit fifo_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(data, push, full, pop, empty)

    void as_master_direction() {
        // Master: 输出数据和pop信号，输入push信号
        this->make_output(data, pop);
        this->make_input(push, full, empty);
    }

    void as_slave_direction() {
        // Slave: 输入数据和pop信号，输出push信号
        this->make_input(data, pop);
        this->make_output(push, full, empty);
    }
};

// 中断Bundle - 用于表示中断接口
struct interrupt_bundle : public bundle_base<interrupt_bundle> {
    using Self = interrupt_bundle;
    ch_bool irq; // 中断请求
    ch_bool ack; // 中断确认

    interrupt_bundle() = default;
    explicit interrupt_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(irq, ack)

    void as_master_direction() {
        // Master: 输出中断请求，输入确认
        this->make_output(irq);
        this->make_input(ack);
    }

    void as_slave_direction() {
        // Slave: 输入中断请求，输出确认
        this->make_input(irq);
        this->make_output(ack);
    }
};

// 配置Bundle - 用于表示配置接口
template <uint32_t ADDR_WIDTH, uint32_t DATA_WIDTH>
struct config_bundle
    : public bundle_base<config_bundle<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = config_bundle<ADDR_WIDTH, DATA_WIDTH>;
    ch_uint<ADDR_WIDTH> address; // 地址
    ch_uint<DATA_WIDTH> data;    // 数据
    ch_bool read;                // 读信号
    ch_bool write;               // 写信号
    ch_bool ready;               // 就绪信号

    config_bundle() = default;
    explicit config_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(address, data, read, write, ready)

    void as_master_direction() {
        // Master: 输出地址和控制信号，输入数据和就绪信号
        this->make_output(address, read, write);
        this->make_input(data, ready);
    }

    void as_slave_direction() {
        // Slave: 输入地址和控制信号，输出数据和就绪信号
        this->make_input(address, read, write);
        this->make_output(data, ready);
    }
};

} // namespace ch