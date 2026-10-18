/**
 * @file i_cache_complete.h
 * @brief 兼容性 shim — ICacheComplete 是 ICacheSync (1-cycle sread) 的别名
 *
 * 历史:
 * - 原始 *_complete.h (2026-04-10) 是 dead-on-arrival (DOA, ADR-037)
 * - 2026-10-19 cache-complete-rewrite 重写为 ch_mem-based (commit 91a0833)
 * - 2026-10-19 ADR-041 拆分为 Sync/Async 双版本, ICacheComplete 指向 ICacheSync (RV32I L1 默认)
 *
 * 新代码请直接用 ICacheSync (1-cycle, RV32I L1 imem) 或 ICacheAsync (0-cycle, TCM-style).
 * 本文件保留以保持向后兼容 (旧 ICacheComplete 引用方不受影响).
 *
 * 作者: build-agent (cache-complete-rewrite Session B, ADR-041)
 * 日期: 2026-10-19
 */
#pragma once

#include "i_cache_sync.h"

namespace chlib {

// ICacheComplete = ICacheSync (默认 1-cycle sread for RV32I L1)
// 旧名称兼容 shim, 推荐新代码用 ICacheSync
using ICacheComplete = ICacheSync;

} // namespace chlib