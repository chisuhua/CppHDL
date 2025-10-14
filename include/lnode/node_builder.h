// include/lnode/node_builder.h
#ifndef CH_LNODE_NODE_BUILDER_H
#define CH_LNODE_NODE_BUILDER_H

namespace ch { namespace core {

// 优化级别枚举 - 非核心实现细节
enum class optimization_level {
    none,        // 无优化
    normal,      // 正常优化
    aggressive   // 激进优化
};

// 构建统计信息结构 - 非核心功能
struct build_statistics {
    uint32_t literals_built = 0;
    uint32_t inputs_built = 0;
    uint32_t outputs_built = 0;
    uint32_t registers_built = 0;
    uint32_t operations_built = 0;
    uint32_t total_nodes_built = 0;
    
    void reset() {
        literals_built = 0;
        inputs_built = 0;
        outputs_built = 0;
        registers_built = 0;
        operations_built = 0;
        total_nodes_built = 0;
    }
};

}} // namespace ch::core

#endif // CH_LNODE_NODE_BUILDER_H
