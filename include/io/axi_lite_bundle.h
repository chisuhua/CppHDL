// include/io/axi_lite_bundle.h
#ifndef CH_IO_AXI_LITE_BUNDLE_H
#define CH_IO_AXI_LITE_BUNDLE_H

#include "core/bundle_base.h"
#include "core/bundle_meta.h"
#include "core/uint.h"
#include "core/bool.h"

namespace ch::core {

// AXI-Lite写地址通道 (AW)
template<size_t ADDR_WIDTH>
struct axi_lite_aw_channel : public bundle_base<axi_lite_aw_channel<ADDR_WIDTH>> {
    ch_uint<ADDR_WIDTH> addr;    // 地址
    ch_uint<3> prot;             // 保护类型
    ch_bool valid;               // 有效信号
    ch_bool ready;               // 就绪信号

    axi_lite_aw_channel() = default;
    axi_lite_aw_channel(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(addr, prot, valid, ready)

    void as_master() override {
        this->make_output(addr, prot, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(addr, prot, valid);
        this->make_output(ready);
    }
};

// AXI-Lite写数据通道 (W)
template<size_t DATA_WIDTH>
struct axi_lite_w_channel : public bundle_base<axi_lite_w_channel<DATA_WIDTH>> {
    ch_uint<DATA_WIDTH> data;                          // 写数据
    ch_uint<(DATA_WIDTH + 7) / 8> strb;                // 字节使能
    ch_bool valid;                                     // 有效信号
    ch_bool ready;                                     // 就绪信号

    axi_lite_w_channel() = default;
    axi_lite_w_channel(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(data, strb, valid, ready)

private:
    void as_master() override {
        this->make_output(data, strb, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(data, strb, valid);
        this->make_output(ready);
    }
};

// AXI-Lite写响应通道 (B)
struct axi_lite_b_channel : public bundle_base<axi_lite_b_channel> {
    ch_uint<2> resp;             // 响应
    ch_bool valid;               // 有效信号
    ch_bool ready;               // 就绪信号

    axi_lite_b_channel() = default;
    axi_lite_b_channel(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(resp, valid, ready)

private:
    void as_master() override {
        this->make_input(resp, valid);
        this->make_output(ready);
    }

    void as_slave() override {
        this->make_output(resp, valid);
        this->make_input(ready);
    }
};

// AXI-Lite读地址通道 (AR)
template<size_t ADDR_WIDTH>
struct axi_lite_ar_channel : public bundle_base<axi_lite_ar_channel<ADDR_WIDTH>> {
    ch_uint<ADDR_WIDTH> addr;    // 地址
    ch_uint<3> prot;             // 保护类型
    ch_bool valid;               // 有效信号
    ch_bool ready;               // 就绪信号

    axi_lite_ar_channel() = default;
    axi_lite_ar_channel(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(addr, prot, valid, ready)

private:
    void as_master() override {
        this->make_output(addr, prot, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(addr, prot, valid);
        this->make_output(ready);
    }
};

// AXI-Lite读数据通道 (R)
template<size_t DATA_WIDTH>
struct axi_lite_r_channel : public bundle_base<axi_lite_r_channel<DATA_WIDTH>> {
    ch_uint<DATA_WIDTH> data;    // 读数据
    ch_uint<2> resp;             // 响应
    ch_bool valid;               // 有效信号
    ch_bool ready;               // 就绪信号

    axi_lite_r_channel() = default;
    axi_lite_r_channel(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(data, resp, valid, ready)

private:
    void as_master() override {
        this->make_input(data, resp, valid);
        this->make_output(ready);
    }

    void as_slave() override {
        this->make_output(data, resp, valid);
        this->make_input(ready);
    }
};

// 完整的AXI-Lite写通道Bundle
template<size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_lite_write_interface : public bundle_base<axi_lite_write_interface<ADDR_WIDTH, DATA_WIDTH>> {
    axi_lite_aw_channel<ADDR_WIDTH> aw;      // 写地址通道
    axi_lite_w_channel<DATA_WIDTH> w;        // 写数据通道
    axi_lite_b_channel b;                    // 写响应通道

    axi_lite_write_interface() = default;
    axi_lite_write_interface(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(aw, w, b)

private:
    void as_master() override {
        this->make_output(aw, w);
        this->make_input(b);
    }

    void as_slave() override {
        this->make_input(aw, w);
        this->make_output(b);
    }
};

// 完整的AXI-Lite读通道Bundle
template<size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_lite_read_interface : public bundle_base<axi_lite_read_interface<ADDR_WIDTH, DATA_WIDTH>> {
    axi_lite_ar_channel<ADDR_WIDTH> ar;      // 读地址通道
    axi_lite_r_channel<DATA_WIDTH> r;        // 读数据通道

    axi_lite_read_interface() = default;
    axi_lite_read_interface(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(ar, r)

private:
    void as_master() override {
        this->make_output(ar);
        this->make_input(r);
    }

    void as_slave() override {
        this->make_input(ar);
        this->make_output(r);
    }
};

// 完整的AXI-Lite接口Bundle
template<size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_lite_bundle : public bundle_base<axi_lite_bundle<ADDR_WIDTH, DATA_WIDTH>> {
    axi_lite_aw_channel<ADDR_WIDTH> aw;      // 写地址通道
    axi_lite_w_channel<DATA_WIDTH> w;        // 写数据通道
    axi_lite_b_channel b;                    // 写响应通道
    axi_lite_ar_channel<ADDR_WIDTH> ar;      // 读地址通道
    axi_lite_r_channel<DATA_WIDTH> r;        // 读数据通道

    axi_lite_bundle() = default;
    axi_lite_bundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(aw, w, b, ar, r)

private:
    void as_master() override {
        this->make_output(aw, w, ar);
        this->make_input(b, r);
    }

    void as_slave() override {
        this->make_input(aw, w, ar);
        this->make_output(b, r);
    }
};

} // namespace ch::core

#endif // CH_IO_AXI_LITE_BUNDLE_H
