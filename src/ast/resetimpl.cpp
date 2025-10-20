// src/core/resetimpl.cpp
#include "ast/resetimpl.h"
#include "ast/instr_mem.h"
#include "core/context.h"
#include <algorithm>

namespace ch::core {

lnodeimpl* resetimpl::clone(context* new_ctx, const clone_map& cloned_nodes) const {
    return new_ctx->create_reset(init_value_, reset_type_, name_, sloc_);
}

} // namespace ch::core
