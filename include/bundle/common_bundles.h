#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

namespace ch {

using ch::core::ch_uint;
using ch::core::ch_bool;
using ch::core::bundle_base;

// FIFO Bundle - 用于表示FIFO接口
//
// ⚠️ **读数据通路（ADR-044）**：`rdata` 是**独立的**读数据字段 ——
//    `data` 是**写数据**（master OUT / slave IN），`rdata` 是**读数据**
//    （master IN / slave OUT）。⚠️ 二者**MUST NOT** 共用同一个字段
//    （共用 ⇒ slave 侧无处输出读到的数据 ⇒ 该协议的读通路不存在）。
//    ⚠️ `rdata` **追加在末尾** ⇒ 既有 5 个字段的位偏移**完全不变**。
template <typename T> struct fifo_bundle : public bundle_base<fifo_bundle<T>> {
    using Self = fifo_bundle<T>;
    T data;        // 数据（写）
    ch_bool push;  // 推入信号
    ch_bool full;  // 满信号
    ch_bool pop;   // 弹出信号
    ch_bool empty; // 空信号
    T rdata;       // 数据（读）—— 见 ADR-044

    fifo_bundle() = default;
    explicit fifo_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(data, push, full, pop, empty, rdata)

    void as_master_direction() {
        // Master = 发起方：驱动 push / pop 与写数据；读取 full / empty 状态。
        // ⚠️ `push` 此前被误放到 master 的**输入**侧（master IN / slave OUT）——
        // 与 `data`/`pop`/`full`/`empty` 的分配不一致：push 是 master 发出的命令，
        // 必须由 master 驱动。详见 OpenSpec change `bundle-master-slave-io-2026-10`。
        // ⚠️ `rdata`（读数据）是 master 的**输入**（由 slave 驱动）——
        // 详见 ADR-044。
        this->make_output(data, push, pop);
        this->make_input(full, empty);
        this->make_input(rdata);
    }

    void as_slave_direction() {
        // Slave = 响应方：接收 push / pop 与写数据；驱动 full / empty 状态。
        // ⚠️ `rdata`（读数据）是 slave 的**输出**（ADR-044）。
        this->make_input(data, push, pop);
        this->make_output(full, empty);
        this->make_output(rdata);
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