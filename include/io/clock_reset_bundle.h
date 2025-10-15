
// include/io/clock_reset_bundle.h
#ifndef CH_IO_CLOCK_RESET_BUNDLE_H
#define CH_IO_CLOCK_RESET_BUNDLE_H

#include "core/bundle_base.h"
#include "core/bool.h"

namespace ch::core {

struct clock_reset_bundle : public bundle_base<clock_reset_bundle> {
    // 使用无方向的中性成员
    ch_bool clock;
    ch_bool reset;

    clock_reset_bundle() = default;
    clock_reset_bundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(clock, reset)

    void as_master() {
        // Master: 输出时钟和复位
        this->make_output(clock, reset);
    }

    void as_slave() {
        // Slave: 输入时钟和复位
        this->make_input(clock, reset);
    }
};

} // namespace ch::core

#endif // CH_IO_CLOCK_RESET_BUNDLE_H
