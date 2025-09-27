// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "core/context.h"
#include "types.h"
#include "io.h"
#include "reg.h"
#include <unordered_map>
#include <vector>
#include <iostream>

namespace ch {

class Simulator {
public:
    explicit Simulator(ch::core::context* ctx);

    void tick();
    uint64_t get_value(const ch::core::ch_logic_out<ch::core::ch_uint<4>>& port) const;
    uint64_t get_value_by_name(const std::string& name) const;

private:
    void eval_combinational();
    void update_registers();

    ch::core::context* ctx_;
    std::vector<ch::core::lnodeimpl*> eval_list_;
    std::unordered_map<ch::core::lnodeimpl*, sdata_type> data_map_;
};

} // namespace ch

#endif // SIMULATOR_H
