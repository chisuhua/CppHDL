// include/io/axi_lite_bundle.h
#ifndef CH_IO_AXI_LITE_BUNDLE_H
#define CH_IO_AXI_LITE_BUNDLE_H

#include "ch.hpp"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {

// AXI-Lite写地址通道Bundle
template <uint32_t ADDR_WIDTH>
struct axi_lite_aw_channel
    : public bundle_base<axi_lite_aw_channel<ADDR_WIDTH>> {
    using Self = axi_lite_aw_channel<ADDR_WIDTH>;
    ch_uint<ADDR_WIDTH> addr; // 地址
    ch_uint<3> prot;          // 保护类型

    axi_lite_aw_channel() = default;
    // explicit axi_lite_aw_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(addr, prot)

    void as_master_direction() {
        // Master: 输出地址和保护类型
        this->make_output(addr, prot);
    }

    void as_slave_direction() {
        // Slave: 输入地址和保护类型
        this->make_input(addr, prot);
    }
};

// AXI-Lite写数据通道Bundle
template <uint32_t DATA_WIDTH>
struct axi_lite_w_channel : public bundle_base<axi_lite_w_channel<DATA_WIDTH>> {
    using Self = axi_lite_w_channel<DATA_WIDTH>;
    ch_uint<DATA_WIDTH> data;     // 写数据
    ch_uint<DATA_WIDTH / 8> strb; // 字节选通
    ch_uint<DATA_WIDTH / 8> keep; // 数据有效

    axi_lite_w_channel() = default;
    // explicit axi_lite_w_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(data, strb, keep)

    void as_master_direction() {
        // Master: 输出数据和控制信号
        this->make_output(data, strb, keep);
    }

    void as_slave_direction() {
        // Slave: 输入数据和控制信号
        this->make_input(data, strb, keep);
    }
};

// AXI-Lite写响应通道Bundle
struct axi_lite_b_channel : public bundle_base<axi_lite_b_channel> {
    using Self = axi_lite_b_channel;
    ch_uint<2> resp; // 响应信号

    axi_lite_b_channel() = default;
    // explicit axi_lite_b_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(resp)

    void as_master_direction() {
        // Master: 输入响应信号
        this->make_input(resp);
    }

    void as_slave_direction() {
        // Slave: 输出响应信号
        this->make_output(resp);
    }
};

// AXI-Lite读地址通道Bundle
template <uint32_t ADDR_WIDTH>
struct axi_lite_ar_channel
    : public bundle_base<axi_lite_ar_channel<ADDR_WIDTH>> {
    using Self = axi_lite_ar_channel<ADDR_WIDTH>;
    ch_uint<ADDR_WIDTH> addr; // 地址
    ch_uint<3> prot;          // 保护类型

    axi_lite_ar_channel() = default;
    // explicit axi_lite_ar_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(addr, prot)

    void as_master_direction() {
        // Master: 输出地址和保护类型
        this->make_output(addr, prot);
    }

    void as_slave_direction() {
        // Slave: 输入地址和保护类型
        this->make_input(addr, prot);
    }
};

// AXI-Lite读数据通道Bundle
template <uint32_t DATA_WIDTH>
struct axi_lite_r_channel : public bundle_base<axi_lite_r_channel<DATA_WIDTH>> {
    using Self = axi_lite_r_channel<DATA_WIDTH>;
    ch_uint<DATA_WIDTH> data; // 读数据
    ch_uint<2> resp;          // 响应信号

    axi_lite_r_channel() = default;
    // explicit axi_lite_r_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(data, resp)

    void as_master_direction() {
        // Master: 输入数据和响应
        this->make_input(data, resp);
    }

    void as_slave_direction() {
        // Slave: 输出数据和响应
        this->make_output(data, resp);
    }
};

// AXI-Lite写接口Bundle
template <uint32_t ADDR_WIDTH, uint32_t DATA_WIDTH>
struct axi_lite_write_interface
    : public bundle_base<axi_lite_write_interface<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_lite_write_interface<ADDR_WIDTH, DATA_WIDTH>;
    axi_lite_aw_channel<ADDR_WIDTH> aw; // 写地址通道
    axi_lite_w_channel<DATA_WIDTH> w;   // 写数据通道
    axi_lite_b_channel b;               // 写响应通道

    axi_lite_write_interface() = default;
    // explicit axi_lite_write_interface(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(aw, w, b)

    void as_master_direction() {
        // Master: 控制写操作
        aw.as_master();
        w.as_master();
        b.as_slave();
    }

    void as_slave_direction() {
        // Slave: 响应写操作
        aw.as_slave();
        w.as_slave();
        b.as_master();
    }
};

// AXI-Lite读接口Bundle
template <uint32_t ADDR_WIDTH, uint32_t DATA_WIDTH>
struct axi_lite_read_interface
    : public bundle_base<axi_lite_read_interface<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_lite_read_interface<ADDR_WIDTH, DATA_WIDTH>;
    axi_lite_ar_channel<ADDR_WIDTH> ar; // 读地址通道
    axi_lite_r_channel<DATA_WIDTH> r;   // 读数据通道

    axi_lite_read_interface() = default;
    // explicit axi_lite_read_interface(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(ar, r)

    void as_master_direction() {
        // Master: 控制读操作
        ar.as_master();
        r.as_slave();
    }

    void as_slave_direction() {
        // Slave: 响应读操作
        ar.as_slave();
        r.as_master();
    }
};

// AXI-Lite完整接口Bundle
template <uint32_t ADDR_WIDTH, uint32_t DATA_WIDTH>
struct axi_lite_bundle
    : public bundle_base<axi_lite_bundle<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_lite_bundle<ADDR_WIDTH, DATA_WIDTH>;
    axi_lite_write_interface<ADDR_WIDTH, DATA_WIDTH> write;
    axi_lite_read_interface<ADDR_WIDTH, DATA_WIDTH> read;

    axi_lite_bundle() = default;
    // explicit axi_lite_bundle(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(write, read)

    void as_master_direction() {
        // Master: 控制整个接口
        write.as_master();
        read.as_master();
    }

    void as_slave_direction() {
        // Slave: 响应整个接口
        write.as_slave();
        read.as_slave();
    }
};

} // namespace ch

#endif // CH_IO_AXI_LITE_BUNDLE_H