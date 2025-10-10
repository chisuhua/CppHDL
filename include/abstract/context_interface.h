// include/abstract/context_interface.h
#ifndef CONTEXT_INTERFACE_H
#define CONTEXT_INTERFACE_H

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace ch { namespace core {
    class lnodeimpl;
}}

namespace ch { namespace abstract {

class context_interface {
public:
    virtual ~context_interface() = default;
    
    virtual uint32_t next_node_id() = 0;
    virtual std::vector<ch::core::lnodeimpl*> get_eval_list() const = 0;
    virtual void set_as_current_context() = 0;
    virtual ch::core::lnodeimpl* get_node_by_id(uint32_t id) const = 0;
};

}} // namespace ch::abstract

#endif // CONTEXT_INTERFACE_H
