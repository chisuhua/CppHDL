// src/hierarchy_chain.h
//
// 层次化发射的**链条机制**（ADR-060 v1.12 G0 · v1.13 G3-2 / G3-6）。
//
// 冒泡（分析层 · 自下而上）与第三轴 / 向上 pass-through（发射层 · 自上而下）是
// **方向相反、机制同构**的同一件事：沿祖先链**每级一个同名端口** + **逐跳实例映射**。
// ⇒ ⇒ MUST 共用本实现（两份实现必然漂移 —— G0 的原话）。
//
// ⚠️ 本头文件是**内部**实现细节（`src/` 下），不属于公开 API。
// ⚠️ 纯函数：只读 `parent_of` / `path`，不碰 IR、不碰 I/O。

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ch {
namespace detail {

/// 一跳：父 module 内的实例 `instance_name` 把 `parent_arg` 接到子 module 的 `child_port`。
struct PassChainHop {
    std::string child_module;
    std::string parent_module;
    std::string instance_name;
    std::string child_port;
    std::string parent_arg;
};

/// 把一条祖先链算成**逐跳映射**。
///
/// - `path` = 从**顶端 module 的直接子**到**底端 module**（顺序固定，不可颠倒）；
/// - 端口名在链上**逐级同名** = `port_name`；
/// - 顶端那一跳的父侧实参 = `top_arg`（顶端可以是已有信号，也可以是新建端口的同名）；
/// - 底端若本身是端口（`leaf_is_port`）⇒ 该跳接 `leaf_port_name`，否则接 `port_name`。
///
/// 返回空 = 路径有断点（某级在 `parent_of` 里没有父实例）⇒ 调用方 MUST fail-closed。
inline std::vector<PassChainHop>
plan_pass_chain(const std::vector<std::string> &path, const std::string &port_name,
                const std::string &top_arg, bool leaf_is_port,
                const std::string &leaf_port_name,
                const std::map<std::string, std::pair<std::string, std::string>> &parent_of) {
    std::vector<PassChainHop> hops;
    if (path.empty()) {
        return hops;
    }
    hops.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto it = parent_of.find(path[i]);
        if (it == parent_of.end()) {
            return {};   // 断点 ⇒ 调用方 fail-closed
        }
        const bool leaf = (i + 1 == path.size());
        hops.push_back(PassChainHop{path[i],
                                    it->second.first,
                                    it->second.second,
                                    (leaf && leaf_is_port) ? leaf_port_name : port_name,
                                    (i == 0) ? top_arg : port_name});
    }
    return hops;
}

}   // namespace detail
}   // namespace ch
