// src/component.cpp
#include "component.h"
#include "core/context.h"

namespace ch {

thread_local Component* Component::current_ = nullptr;

Component::Component(Component* parent, const std::string& name)
    : parent_(parent)
    , name_(name.empty() ? "unnamed" : name)
{
    // 构造函数保持简单，只初始化基本成员
}

void Component::build(ch::core::context* external_ctx) {
    // 防止重复构建
    if (built_) {
        return;
    }
    built_ = true;

    // 确定目标上下文
    std::unique_ptr<ch::core::context> local_ctx;
    ch::core::context* target_ctx = nullptr;
    
    if (external_ctx) {
        // 使用外部提供的上下文
        target_ctx = external_ctx;
    } else {
        // 创建自己的上下文
        auto parent_ctx = parent_ ? parent_->context() : nullptr;
        local_ctx = std::make_unique<ch::core::context>(name_, parent_ctx);
        target_ctx = local_ctx.get();
        ctx_ = std::move(local_ctx);  // 转移所有权到成员变量
    }

    // 执行内部构建逻辑
    build_internal(target_ctx);
}

void Component::build_internal(ch::core::context* target_ctx) {
    // 使用 RAII 管理上下文切换，确保异常安全
    ch::core::ctx_swap ctx_guard(target_ctx);
    
    // 保存并设置当前组件
    Component* old_comp = current_;
    current_ = this;
    
    try {
        // 执行端口创建和行为描述
        create_ports();
        describe();
    } catch (...) {
        // 恢复状态并重新抛出异常
        current_ = old_comp;
        throw;
    }
    
    // 恢复之前的当前组件
    current_ = old_comp;
    // RAII 自动处理上下文恢复
}

Component* Component::add_child(std::unique_ptr<Component> child) {
    // 输入验证
    if (!child) {
        return nullptr;
    }
    
    // 检查是否已经存在同名子组件（可选）
    const std::string& child_name = child->name();
    for (const auto& existing_child : children_) {
        if (existing_child && existing_child->name() == child_name) {
            // 可以选择警告或抛出异常
            // std::cerr << "Warning: Child with name '" << child_name << "' already exists" << std::endl;
            break;
        }
    }
    
    // 添加子组件并返回原始指针
    auto* raw_ptr = child.get();
    children_.push_back(std::move(child));
    return raw_ptr;
}

} // namespace ch
