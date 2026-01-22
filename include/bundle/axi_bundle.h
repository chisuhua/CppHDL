// include/io/axi_bundle.h
#ifndef CH_IO_AXI_BUNDLE_H
#define CH_IO_AXI_BUNDLE_H

#include "bundle/stream_bundle.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

namespace ch::core {

// AXI地址通道Bundle
template <size_t ADDR_WIDTH>
struct axi_addr_channel : public bundle_base<axi_addr_channel<ADDR_WIDTH>> {
    using Self = axi_addr_channel<ADDR_WIDTH>;
    ch_uint<ADDR_WIDTH> addr;
    ch_uint<3> prot;
    ch_bool valid;
    ch_bool ready;

    axi_addr_channel() = default;
    axi_addr_channel(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(addr, prot, valid, ready)

    void as_master() override {
        this->make_output(addr, prot, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(addr, prot, valid);
        this->make_output(ready);
    }
};

// AXI数据写通道Bundle
template <size_t DATA_WIDTH>
struct axi_write_data_channel
    : public bundle_base<axi_write_data_channel<DATA_WIDTH>> {
    using Self = axi_write_data_channel<DATA_WIDTH>;
    ch_uint<DATA_WIDTH> data;
    ch_uint<(DATA_WIDTH + 7) / 8> strb;
    ch_bool valid;
    ch_bool ready;

    axi_write_data_channel() = default;
    axi_write_data_channel(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(data, strb, valid, ready)

    void as_master() override {
        this->make_output(data, strb, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(data, strb, valid);
        this->make_output(ready);
    }
};

// AXI写响应通道Bundle
struct axi_write_resp_channel : public bundle_base<axi_write_resp_channel> {
    using Self = axi_write_resp_channel;
    ch_uint<2> resp;
    ch_bool valid;
    ch_bool ready;

    axi_write_resp_channel() = default;
    axi_write_resp_channel(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(resp, valid, ready)

    void as_master() override {
        this->make_input(resp, valid);
        this->make_output(ready);
    }

    void as_slave() override {
        this->make_output(resp, valid);
        this->make_input(ready);
    }
};

// 完整的AXI写通道Bundle (嵌套Bundle示例)
template <size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_write_channel
    : public bundle_base<axi_write_channel<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_write_channel;
    axi_addr_channel<ADDR_WIDTH> aw;      // 地址通道
    axi_write_data_channel<DATA_WIDTH> w; // 数据写通道
    axi_write_resp_channel b;             // 响应通道

    axi_write_channel() = default;
    axi_write_channel(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(aw, w, b)

    void as_master() override {
        this->make_output(aw, w);
        this->make_input(b);
    }

    void as_slave() override {
        this->make_input(aw, w);
        this->make_output(b);
    }
};

} // namespace ch::core

#endif // CH_IO_AXI_BUNDLE_H