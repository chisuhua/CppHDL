// include/core/node_builder.h
// include/core/node_builder.h
#ifndef CH_CORE_NODE_BUILDER_H
#define CH_CORE_NODE_BUILDER_H
#include "context.h"
#include "lnodeimpl.h"
#include "literal.h"
#include "types.h"
#include <source_location>
#include <memory>

namespace ch { namespace core {

// 前向声明 - 具体定义在实现文件中
struct build_statistics;
enum class optimization_level;

// 节点构建器核心接口
class node_builder {
public:
    // 单例访问点
    static node_builder& instance();
    
    // === 核心配置接口 ===
    void set_debug_mode(bool debug);
    void set_optimization_level(optimization_level level);
    void enable_caching(bool enable);
    void set_name_prefix(const std::string& prefix);
    
    // === 统计接口 ===
    void enable_statistics(bool enable);
    const build_statistics& get_statistics() const;
    void reset_statistics();
    
    // === 核心构建接口 ===
    // 常量节点构建
    template<typename T>
    lnodeimpl* build_literal(T value, 
                           const std::string& name = "literal",
                           const std::source_location& sloc = std::source_location::current());
    
    // 输入节点构建
    template<typename T>
    lnodeimpl* build_input(const std::string& name = "input",
                          const std::source_location& sloc = std::source_location::current());
    
    // 输出节点构建
    template<typename T>
    lnodeimpl* build_output(const std::string& name = "output",
                           const std::source_location& sloc = std::source_location::current());
    
    // 寄存器节点构建
    template<typename T>
    std::pair<regimpl*, proxyimpl*> build_register(
        lnodeimpl* init_val = nullptr,
        lnodeimpl* next_val = nullptr,
        const std::string& name = "reg",
        const std::source_location& sloc = std::source_location::current());
    
    // 二元操作节点构建
    template<typename T, typename U>
    lnodeimpl* build_operation(
        ch_op op,
        const lnode<T>& lhs,
        const lnode<U>& rhs,
        bool is_signed = false,
        const std::string& name = "op",
        const std::source_location& sloc = std::source_location::current());
    
    // 一元操作节点构建
    template<typename T>
    lnodeimpl* build_unary_operation(
        ch_op op,
        const lnode<T>& operand,
        const std::string& name = "unary_op",
        const std::source_location& sloc = std::source_location::current());

protected:
    // 受保护的构造函数，只允许内部使用
    node_builder();
    
    // 辅助方法声明
    static uint32_t calculate_result_size(ch_op op, uint32_t lhs_width, uint32_t rhs_width);
    std::string prefixed_name(const std::string& name) const;

private:
    // 核心状态成员变量声明
    bool debug_mode_;
    optimization_level opt_level_;
    bool caching_enabled_;
    bool statistics_enabled_;
    std::string name_prefix_;
    build_statistics statistics_;
    
    // 扩展功能成员变量声明
    std::unique_ptr<struct node_cache> cache_;
    std::unique_ptr<struct optimization_manager> optimizer_;
};

// 在文件末尾包含具体实现
#include "../lnode/node_builder.h"

}} // namespace ch::core
#endif // CH_CORE_NODE_BUILDER_H
