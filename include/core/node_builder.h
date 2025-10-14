// include/core/node_builder.h
#ifndef CH_CORE_NODE_BUILDER_H
#define CH_CORE_NODE_BUILDER_H

#include "context.h"
#include "lnode.h"
#include "lnodeimpl.h"
#include "literal.h"
#include "types.h"
#include <source_location>
#include <memory>

// 直接包含非核心定义
#include "../lnode/node_builder.h"

namespace ch { namespace core {

// 节点构建器核心接口
class node_builder {
public:
    // 单例访问点
    static node_builder& instance() {
        static node_builder instance;
        return instance;
    }
    
    // 配置和统计接口
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    void set_optimization_level(optimization_level level) { opt_level_ = level; }
    void enable_caching(bool enable) { caching_enabled_ = enable; }
    void set_name_prefix(const std::string& prefix) { name_prefix_ = prefix; }
    void enable_statistics(bool enable) { statistics_enabled_ = enable; }
    const build_statistics& get_statistics() const { return *statistics_; }
    void reset_statistics() { statistics_->reset(); }
    
    // 核心构建接口
    template<typename T>
    lnodeimpl* build_literal(T value, 
                           const std::string& name = "literal",
                           const std::source_location& sloc = std::source_location::current());
    
    template<typename T>
    lnodeimpl* build_input(const std::string& name = "input",
                          const std::source_location& sloc = std::source_location::current());
    
    template<typename T>
    lnodeimpl* build_output(const std::string& name = "output",
                           const std::source_location& sloc = std::source_location::current());
    
    template<typename T>
    std::pair<regimpl*, proxyimpl*> build_register(
        lnodeimpl* init_val = nullptr,
        lnodeimpl* next_val = nullptr,
        const std::string& name = "reg",
        const std::source_location& sloc = std::source_location::current());
    
    template<typename T, typename U>
    lnodeimpl* build_operation(
        ch_op op,
        const lnode<T>& lhs,
        const lnode<U>& rhs,
        bool is_signed = false,
        const std::string& name = "op",
        const std::source_location& sloc = std::source_location::current());
    
    template<typename T>
    lnodeimpl* build_unary_operation(
        ch_op op,
        const lnode<T>& operand,
        const std::string& name = "unary_op",
        const std::source_location& sloc = std::source_location::current());

private:
    node_builder();
    
    // 私有成员 (移除不完整类型的 unique_ptr)
    bool debug_mode_;
    optimization_level opt_level_;
    bool caching_enabled_;
    bool statistics_enabled_;
    std::string name_prefix_;
    std::unique_ptr<build_statistics> statistics_;
    // 移除 node_cache 和 optimization_manager 相关成员
};

// 构造函数实现
inline node_builder::node_builder() 
    : debug_mode_(false)
    , opt_level_(optimization_level::normal)
    , caching_enabled_(false)
    , statistics_enabled_(false)
    , name_prefix_("")
    , statistics_(std::make_unique<build_statistics>()) {
}

}} // namespace ch::core

// 在文件末尾包含模板实现
#include "../lnode/node_builder.tpp"

#endif // CH_CORE_NODE_BUILDER_H
