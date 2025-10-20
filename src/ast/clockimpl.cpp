// src/core/clockimpl.cpp
#include "ast/clockimpl.h"
#include "core/context.h"
#include <algorithm>

namespace ch::core {

lnodeimpl* clockimpl::clone(context* new_ctx, const clone_map& cloned_nodes) const {
    return new_ctx->create_clock(init_value_, is_posedge_, is_negedge_, name_, sloc_);
}

} // namespace ch::core
