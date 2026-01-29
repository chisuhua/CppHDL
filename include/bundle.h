// include/bundle.h
//
// CppHDL Bundle 系统的统一包含头
//
// 包含所有 Bundle 相关功能：
// - 基础 Bundle 类型
// - 标准 Bundle 协议（Stream, Flow）
// - 方案 3 的编译期反射支持
//

#ifndef CH_BUNDLE_H
#define CH_BUNDLE_H

// 基础设施
#include "core/bundle/bundle_meta.h"          // 字段元数据和宏
#include "core/bundle/bundle_traits.h"        // 类型特性和检测
#include "core/bundle/bundle_layout.h"        // 布局计算
#include "core/bundle/bundle_protocol.h"      // 协议检测
#include "core/bundle/bundle_operations.h"    // 操作符
#include "core/bundle/bundle_serialization.h" // 序列化
#include "core/bundle/bundle_base.h"          // Bundle 基类
#include "core/bundle/bundle_utils.h"         // 工具函数

// 方案 3：编译期反射支持
#include "core/bundle/bundle_port_proxy.h"           // 字段代理实现
#include "core/bundle/bundle_plan3_integration.h"    // 集成和便捷工具

// 标准 Bundle 类型
#include "bundle/stream_bundle.h"                    // Stream Bundle
#include "bundle/flow_bundle.h"                      // Flow Bundle
#include "bundle/common_bundles.h"                   // 常见 Bundle 类型

#endif // CH_BUNDLE_H
