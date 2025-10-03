// src/core/lnodeimpl.cpp
/*
#define CH_LNODE_ENUM(m) \
  m(none) \
  m(lit) \
  m(proxy) \
  m(input) \
  m(output) \
  m(op) \
  m(reg) \
  m(mem)
*/

#include "lnodeimpl.h"

#include <array>
#include <string_view>

namespace ch { namespace core {


#define CH_LNODE_NAME(n) #n,
static constexpr std::size_t LNODE_TYPE_NAMES_SIZE = ch_lnode_type_count();
static constexpr std::array<std::string_view, LNODE_TYPE_NAMES_SIZE> lnode_type_names = {{
    CH_LNODE_ENUM(CH_LNODE_NAME)
}};

#undef CH_LNODE_NAME

const char* to_string(lnodetype type) {
    auto index = static_cast<std::size_t>(type);
    if (index < lnode_type_names.size()) {
        return lnode_type_names[index].data();
    }
    return "unknown";
}

}} // namespace ch::core
