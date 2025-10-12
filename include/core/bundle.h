// include/core/bundle.h
#ifndef CORE_BUNDLE_H
#define CORE_BUNDLE_H

#include "io.h"
#include "logger.h"
#include <memory>

namespace ch { namespace core {

// === Bundle 基类 ===
class bundle {
public:
    virtual ~bundle() = default;
    
    // 翻转接口 - 每个 Bundle 都需要实现
    virtual std::unique_ptr<bundle> flip() const = 0;
    
    // 验证 Bundle 中所有端口是否有效
    virtual bool is_valid() const = 0;
};

}} // namespace ch::core

// 包含具体的 Bundle 实现
#include "../io/bundle.h"

#endif // CORE_BUNDLE_H
