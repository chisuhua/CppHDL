// examples/spinalhdl-ported/quadrature_encoder/quadrature_encoder_top.h
//
// 正交编码器示例的**顶层组件**（三角色分离）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）。
// ⚠️ **端口面（7 根 · 根自有）**：`enc_a` · `enc_b` · `enc_z` · `position[15:0]` ·
//   `direction` · `a_sync` · `b_sync` ⇒ ⇒ 加框架 2 = 产物端口表 9 行。
// ⚠️ **语义（spec-derived = 正交编码 4 倍频）**：
//   · 同步：A/B/Z 各两级（INIT **true**）
//   · 4 倍频：A/B 的**每个边沿**都计数（方向由「另一相的相位」决定）
//   · `direction = a_sync2 ^ b_sync2`（1=CW）
//   · `enc_z` 上升沿 ⇒ `position <= 0`

#pragma once

#include "ch.hpp"
#include "component.h"
#include "module.h"

#include <string>

namespace quadrature_encoder_example {

using namespace ch::core;

/// **根组件名**（单一真相源）
inline constexpr const char *kQuadRootName = "top";

template <unsigned COUNTER_WIDTH = 16> class QuadratureEncoder : public ch::Component {
  public:
    __io(ch_in<ch_bool> enc_a{"enc_a"};
         ch_in<ch_bool> enc_b{"enc_b"};
         ch_in<ch_bool> enc_z{"enc_z"};
         ch_out<ch_uint<COUNTER_WIDTH>> position{"position"};
         ch_out<ch_bool> direction{"direction"};
         ch_out<ch_bool> a_sync{"a_sync"};
         ch_out<ch_bool> b_sync{"b_sync"})

    QuadratureEncoder(ch::Component *parent = nullptr, const std::string &name = "quad_enc")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_bool> a_sync1(ch_bool(true));
        ch_reg<ch_bool> a_sync2(ch_bool(true));
        ch_reg<ch_bool> b_sync1(ch_bool(true));
        ch_reg<ch_bool> b_sync2(ch_bool(true));

        a_sync1->next = io().enc_a;
        a_sync2->next = a_sync1;
        b_sync1->next = io().enc_b;
        b_sync2->next = b_sync1;

        io().a_sync = a_sync2;
        io().b_sync = b_sync2;

        auto a_rising = !a_sync2 & a_sync1;
        auto a_falling = a_sync2 & !a_sync1;
        auto b_rising = !b_sync2 & b_sync1;
        auto b_falling = b_sync2 & !b_sync1;

        auto is_cw = (a_sync2 & !b_sync2) | (!a_sync2 & b_sync2);

        auto count_up = (a_rising & !b_sync2) | (a_falling & b_sync2) |
                        (b_rising & a_sync2) | (b_falling & !a_sync2);
        auto count_down = (a_rising & b_sync2) | (a_falling & !b_sync2) |
                          (b_rising & !a_sync2) | (b_falling & a_sync2);

        ch_reg<ch_uint<COUNTER_WIDTH>> pos_reg(0_d);
        ch_reg<ch_bool> z_sync1(ch_bool(false));
        ch_reg<ch_bool> z_sync2(ch_bool(false));
        z_sync1->next = io().enc_z;
        z_sync2->next = z_sync1;
        auto z_rising = !z_sync2 & z_sync1;

        auto new_pos_up = pos_reg + ch_uint<COUNTER_WIDTH>(1_d);
        auto new_pos_down = pos_reg - ch_uint<COUNTER_WIDTH>(1_d);

        pos_reg->next = select(z_rising, ch_uint<COUNTER_WIDTH>(0_d),
                               select(count_up, new_pos_up,
                                      select(count_down, new_pos_down, pos_reg)));

        io().position = pos_reg;
        io().direction = is_cw;
    }
};

/// 顶层组件（**具名端口**）
class QuadEncTop : public ch::Component {
  public:
    __io(ch_in<ch_bool> enc_a{"enc_a"};
         ch_in<ch_bool> enc_b{"enc_b"};
         ch_in<ch_bool> enc_z{"enc_z"};
         ch_out<ch_uint<16>> position{"position"};
         ch_out<ch_bool> direction{"direction"};
         ch_out<ch_bool> a_sync{"a_sync"};
         ch_out<ch_bool> b_sync{"b_sync"})

    QuadEncTop(ch::Component *parent = nullptr, const std::string &name = "quad_enc_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<QuadratureEncoder<16>> enc{"enc"};
        enc.io().enc_a <<= io().enc_a;
        enc.io().enc_b <<= io().enc_b;
        enc.io().enc_z <<= io().enc_z;
        io().position <<= enc.io().position;
        io().direction <<= enc.io().direction;
        io().a_sync <<= enc.io().a_sync;
        io().b_sync <<= enc.io().b_sync;
    }
};

} // namespace quadrature_encoder_example
