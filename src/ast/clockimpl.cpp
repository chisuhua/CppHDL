#include "clockimpl.h"
#include "core/context.h"
#include "instr_clock.h"
#include <sstream>

namespace ch {
namespace core {

lnodeimpl *clockimpl::clone(context *new_ctx,
                            const clone_map &cloned_nodes) const {
    auto new_node = new_ctx->create_clock(init_value_, is_posedge_, is_negedge_,
                                          name_, sloc_);
    return new_node;
}

std::unique_ptr<ch::instr_base>
clockimpl::create_instruction(ch::data_map_t &data_map) const {
    // 创建时钟指令实例
    auto clock_instr = std::make_unique<ch::instr_clock>(
        &data_map[id()], is_posedge_, is_negedge_);
    return clock_instr;
}

} // namespace core
} // namespace ch