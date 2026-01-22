// include/io/common_bundles.h
#ifndef CH_IO_COMMON_BUNDLES_H
#define CH_IO_COMMON_BUNDLES_H

#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

namespace ch::core {

// FIFO接口Bundle
template <typename T> struct fifo_bundle : public bundle_base<fifo_bundle<T>> {
    using Self = fifo_bundle<T>;
    T data;
    ch_bool push;
    ch_bool full;
    ch_bool pop;
    ch_bool empty;

    fifo_bundle() = default;
    fifo_bundle(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(data, push, full, pop, empty)

    void as_master() override {
        this->make_output(data, push, pop);
        this->make_input(full, empty);
    }

    void as_slave() override {
        this->make_input(data, push, pop);
        this->make_output(full, empty);
    }
};

// 中断Bundle
struct interrupt_bundle : public bundle_base<interrupt_bundle> {
    using Self = interrupt_bundle;
    ch_bool irq;
    ch_bool ack;

    interrupt_bundle() = default;
    interrupt_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(irq, ack)

    void as_master() override {
        this->make_output(irq);
        this->make_input(ack);
    }

    void as_slave() override {
        this->make_input(irq);
        this->make_output(ack);
    }
};

// 配置Bundle
template <size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct config_bundle
    : public bundle_base<config_bundle<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = config_bundle<ADDR_WIDTH, DATA_WIDTH>;
    ch_uint<ADDR_WIDTH> addr;
    ch_uint<DATA_WIDTH> wdata;
    ch_uint<DATA_WIDTH> rdata;
    ch_bool write;
    ch_bool read;
    ch_bool ready;

    config_bundle() = default;
    config_bundle(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(addr, wdata, rdata, write, read, ready)

    void as_master() override {
        this->make_output(addr, wdata, write, read);
        this->make_input(rdata, ready);
    }

    void as_slave() override {
        this->make_input(addr, wdata, write, read);
        this->make_output(rdata, ready);
    }
};

} // namespace ch::core

#endif // CH_IO_COMMON_BUNDLES_H