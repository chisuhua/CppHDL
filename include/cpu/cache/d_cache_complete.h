/**
 * @file d_cache_complete.h
 * @brief 兼容性 shim — DCacheComplete 是 DCacheSync (1-cycle sread) 的别名
 *
 * 历史:
 * - 原始 *_complete.h (2026-04-10) 是 dead-on-arrival (DOA, ADR-037)
 * - 2026-10-19 cache-complete-rewrite 重写为 ch_mem-based (commit 91a0833)
 * - 2026-10-19 ADR-041 拆分为 Sync/Async 双版本, DCacheComplete 指向 DCacheSync (RV32I L1 默认)
 *
 * 新代码请直接用 DCacheSync (1-cycle, RV32I L1 dmem) 或 DCacheAsync (0-cycle, TCM-style).
 * 本文件保留以保持向后兼容.
 *
 * 作者: build-agent (cache-complete-rewrite Session B, ADR-041)
 * 日期: 2026-10-19
 */
#pragma once

#include "d_cache_sync.h"

namespace chlib {

// DCacheComplete = DCacheSync (默认 1-cycle sread for RV32I L1)
// 旧名称兼容 shim, 推荐新代码用 DCacheSync
using DCacheComplete = DCacheSync;

} // namespace chlib