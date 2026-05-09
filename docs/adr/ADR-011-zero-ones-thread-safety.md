# ADR-011: `zero()`/`ones()` 常量缓存线程安全策略

**状态**: ✅ 已采纳  
**日期**: 2026-05-07  
**决策人**: Sisyphus + 用户  

---

## 1. 背景

`ch::core::constants::zero()` 和 `ch::core::constants::ones()` 是两个工厂函数，用于快速获取指定位宽的全 0 / 全 1 常量 `sdata_type`。它们使用 `static` 缓存来避免重复构造：

- **`zero()`** — `include/core/types.h:175`（header-only inline）
- **`ones()`** — `src/utils/types.cpp:445`（非 inline）

当前状态：

```cpp
// zero() - types.h
inline const sdata_type &zero(uint32_t width = 1) {
    static /*thread_local*/ std::unordered_map<uint32_t, sdata_type> zero_cache;
    auto it = zero_cache.find(width);
    if (it == zero_cache.end()) {
        it = zero_cache.emplace(width, sdata_type(0, width)).first;
    }
    return it->second;
}

// ones() - types.cpp
const sdata_type &ones(uint32_t width) {
    static /*thread_local*/ std::unordered_map<uint32_t, sdata_type> ones_cache;
    // ...emplace 逻辑...
}
```

`thread_local` 被注释掉，导致两个函数使用**普通 `static` 共享缓存，无任何同步保护**。

---

## 2. 风险分析

### 2.1 数据竞争

- 两个函数可能被**多线程并行调用**（JIT 编译线程 + 仿真主线程）
- `std::unordered_map::emplace()` 在并发写入时是**数据竞争**（UB）
- 由于返回 `const &`，初始化后的只读访问是安全的（`find()` 只读操作是并发安全的）
- 风险仅发生在**首次对不同 width 的并发插入**

### 2.2 `sdata_type` 开销分析

- **width ≤ 64**: `sdata_type` 内部存储为 `uint64_t`，构造是极轻量的（无动态分配）
- **width > 64**: 涉及 `std::bitset` 或动态分配，构造有可度量开销

### 2.3 调用频率

- `zero()`/`ones()` 是 hot path — 每个 `ch_uint<N>` 默认构造、每个常量节点创建都可能调用
- `zero()` 的调用频次远高于 `ones()`

---

## 3. 方案分析

| 选项 | 方案 | 优点 | 缺点 |
|------|------|------|------|
| **A** | 恢复 `thread_local` | 零锁、完全无竞争 | 每个线程独立缓存 → 重复构造 O(n_threads×width)；`zero()` 在 header 中需移至 .cpp |
| **B** | 加 `mutex` 保护共享缓存 | 实现简单，复用缓存 | `zero()` 是 hot path，锁竞争影响性能 |
| **C** | 移除缓存，每次重新构造 | 零共享状态，最简单 | 高频调用场景产生重复分配开销 |
| **D** | **混合策略：小 width 无缓存 + 大 width thread_local** | 兼顾性能和线程安全 | 实现复杂度中等 |

### 3.1 选项 A：恢复 `thread_local`

**优点**：
- 完全无锁，无可竞争状态
- `ones()` 在 .cpp 中实现，加 `thread_local` 无额外负担

**缺点**：
- `zero()` 在 header 中 inline — 虽然 `static thread_local` 在 inline 函数中 C++17 合法，但每个线程首次调用时创建新的空 map
- N 个线程 × M 个 width 会产生 N×M 个 `sdata_type` 对象，存在浪费
- 测试中可能大量单线程场景，`thread_local` 的优势未被利用

### 3.2 选项 B：加 `mutex` 保护

**优点**：
- 实现改动最小：在现有 `static` 缓存基础上包一层 `std::mutex`
- 全局唯一缓存，无重复构造

**缺点**：
- `zero()` 在 hot path 上，即使 `std::lock_guard` 开销在争用下也会放大
- 对于 ≤64 bit 场景，加锁的代价可能超过构造本身

### 3.3 选项 C：移除缓存

**优点**：
- 代码最简化，零共享状态，天然线程安全

**缺点**：
- 每次调用都构造 `sdata_type`，即使对于频繁使用的小 width（1, 8, 16, 32, 64）
- `ones()` 对 >64 bit 有 O(width) 的置位循环，每次重复计算不划算

### 3.4 选项 D：混合策略（推荐 ✅）

**核心思路**：按位宽分界，对不同规模采用不同策略。

| 位宽范围 | 策略 | 原因 |
|---------|------|------|
| **≤ 64 bit** | 无独立缓存，使用预分配的小型固定数组直接返回引用 | `sdata_type` 内部仅 `uint64_t`，构造成本低于缓存查找 |
| **> 64 bit** | `thread_local std::unordered_map` 缓存 | 宽位向量构造有动态分配开销，`thread_local` 避免锁和重复构造 |

**优点**：
- ≤64 bit：零锁、零竞争、零额外分配（固定数组）
- >64 bit：`thread_local` 提供完全线程隔离，无锁竞争
- 边界清晰，分界点 64 bit 与 `sdata_type` 内部表示的自然边界一致

**缺点**：
- ≤64 bit 的固定数组最多 65 个 `sdata_type` 对象，每个约几十字节，共几 KB 内存占用
- 需要 `std::call_once` 或类似机制进行延迟初始化

---

## 4. 决议（Q1）

**决策**：选 **D** — 混合策略（小 width ≤64 无独立缓存 + 大 width thread_local 缓存）。

**理由**：
1. ≤64 bit 的 `sdata_type` 构造极轻量（仅一个 `uint64_t`），不值得缓存或加锁保护
2. >64 bit 涉及动态分配，`thread_local` 提供零锁线程安全且避免重复构造
3. 64 作为分界点与 `sdata_type` 内部表示边界一致，直觉清晰
4. 兼顾了 hot path 性能（小 width 无锁）和大 width 资源复用

**实现要点**：

**`zero()`**（`types.h` inline）：
```cpp
inline const sdata_type &zero(uint32_t width = 1) {
    if (width <= 64) {
        // 小型固定数组，最多 65 个条目，call_once 延迟初始化
        static sdata_type small_zero[65];
        static std::once_flag small_zero_init[65];
        std::call_once(small_zero_init[width], [w = width] {
            small_zero[w] = sdata_type(0, w);
        });
        return small_zero[width];
    }
    // 宽位向量：thread_local 缓存
    static thread_local std::unordered_map<uint32_t, sdata_type> zero_cache;
    auto it = zero_cache.find(width);
    if (it == zero_cache.end()) {
        it = zero_cache.emplace(width, sdata_type(0, width)).first;
    }
    return it->second;
}
```

**`ones()`**（`types.cpp` 非 inline）：
```cpp
const sdata_type &ones(uint32_t width) {
    if (width <= 64) {
        static sdata_type small_ones[65];
        static std::once_flag small_ones_init[65];
        std::call_once(small_ones_init[width], [w = width] {
            sdata_type s(w);
            s = (w == 64) ? UINT64_MAX : ((1ULL << w) - 1);
            small_ones[w] = std::move(s);
        });
        return small_ones[width];
    }
    static thread_local std::unordered_map<uint32_t, sdata_type> ones_cache;
    // ...现有 emplace 逻辑不变...
}
```

**注意事项**：
- `std::once_flag` 不可复制/移动，但可以在 `static` 数组中默认构造，合法
- `std::call_once` 在已触发的 flag 上仅做一次原子 load，开销极小
- 实现时注意 `ones()` 中 `(1ULL << 64)` 的 UB 需要特殊处理（已存在的 `width == 64` 特判）

---

## 5. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Q1 分析与决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/types.h:175-182` — `zero()` 定义（inline header）
- `src/utils/types.cpp:445-462` — `ones()` 定义（.cpp）
- `include/core/types.h:155-162` — `sdata_type` 成员定义
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #6
