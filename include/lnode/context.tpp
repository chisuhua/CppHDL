#ifndef CH_LNODE_CONTEXT_H
#define CH_LNODE_CONTEXT_H

#include "context.h"
#include "logger.h"

namespace ch::core {
// 统一的节点创建方法，添加错误检查
//
// ⚠️ **节点名唯一性检查 MUST 在本函数的 `try` 之外**（`core-ctx-node-name-uniqueness`）：
//    本函数的 `catch (const std::exception &)` 会**吞掉**异常并返回 `nullptr`
//    ⇒ ⇒ 若把 `check_node_name_unique()` 放进 `try`，它的「抛异常中止」会退化成
//    **静默失败** ⇒ 违反 fail-fast 契约（MUST NOT 只打日志 / MUST NOT 静默返回空）。
//    ⇒ ⇒ 故拆成三段：① 构造（`try` 内 · 只捕获构造失败）② 唯一性检查（`try` 外）
//    ③ 入表（`try` 内 · 只捕获分配失败）。
template <typename T, typename... Args>
inline T *context::create_node(Args &&...args) {
    CHDBG_FUNC();
    CHREQUIRE(this != nullptr, "Context cannot be null");

    uint32_t new_id = next_node_id();
    std::unique_ptr<T> node;
    try {
        node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this);
    } catch (const std::bad_alloc &) {
        CHERROR("Failed to allocate memory for node creation");
        return nullptr;
    } catch (const std::exception &e) {
        CHERROR("Node creation failed: %s", e.what());
        return nullptr;
    }

    T *raw_ptr = node.get();

    // ② 唯一性检查（**非 const 期命中冲突 ⇒ 抛** ⇒ 异常直接冒泡给调用方）
    if (node_name_policy_ == NodeNamePolicy::kRejectDuplicates) {
        if (!check_node_name_unique(raw_ptr->name(), raw_ptr->sloc())) {
            // 降级路径的唯一可达分支（静态析构期）⇒ **失败结果** = 不把节点放进 ctx
            return nullptr;
        }
    }

    try {
        node_storage_.push_back(std::move(node));
    } catch (const std::bad_alloc &) {
        CHERROR("Failed to allocate memory for node storage");
        return nullptr;
    }

    if (debug_context_lifetime()) {
        CHINFO("Created node ID %u (%s) of %s in context 0x%llx", new_id,
               raw_ptr->name().c_str(), raw_ptr->to_string().c_str(),
               (unsigned long long)this);
    }
    return raw_ptr;
}

} // namespace ch::core
#endif
