#ifndef CH_LNODE_CONTEXT_H
#define CH_LNODE_CONTEXT_H

#include "context.h"
#include "logger.h"

namespace ch::core {
// 统一的节点创建方法，添加错误检查
template <typename T, typename... Args>
inline T *context::create_node(Args &&...args) {
    CHDBG_FUNC();
    CHREQUIRE(this != nullptr, "Context cannot be null");

    try {
        uint32_t new_id = next_node_id();
        auto node =
            std::make_unique<T>(new_id, std::forward<Args>(args)..., this);
        T *raw_ptr = node.get();

        node_storage_.push_back(std::move(node));

        if (debug_context_lifetime()) {
            CHINFO("Created node ID %u (%s) of %s in context 0x%llx", new_id,
                   raw_ptr->name().c_str(), raw_ptr->to_string().c_str(),
                   (unsigned long long)this);
        }
        return raw_ptr;
    } catch (const std::bad_alloc &) {
        CHERROR("Failed to allocate memory for node creation");
        return nullptr;
    } catch (const std::exception &e) {
        CHERROR("Node creation failed: %s", e.what());
        return nullptr;
    }
}

} // namespace ch::core
#endif