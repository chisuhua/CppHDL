/**
 * @file i_cache_prefetch.h
 * @brief I-Cache wrapper —— 直通式：把内层 cache 的 AXI 主端**转发**到顶层
 *
 * ⚠️ **2026-10-28 行为订正**（change `riscv-mini-cache-wrapper-live-logic-2026-10`）：
 *   本文件此前用**普通 C++ `if`** 包裹 `ch_bool` 条件来发射 RTL ——
 *   条件形如「`cache_miss` 与 `io().axi_ready` 同时为真」，分支体内驱动
 *   `io().axi_addr` / `io().axi_valid`。
 *   `ch_bool` 有 `explicit operator bool()` ⇒ C++ `if` 拿到的是
 *   「**当前节点是否非零**」，而 `describe()` 在**建图期**执行
 *   ⇒ 该条件建图期恒为**假** ⇒ **分支体永不发射**
 *   ⇒ `axi_addr` / `axi_valid` **恒为未驱动**（读到 0）
 *   ⇒ wrapper 的 AXI 主端**是死的**（只能证明「不 SIGSEGV」）。
 *   ⇒ 现在改为**无条件**的组合赋值（条件体现在表达式的**取值**里）
 *     ⇒ 每个 `ch_out` 都被驱动 ⇒ 输出**随输入变化**。
 *
 *   ⚠️ 本注释**刻意不逐字引用**那个 `if` 形态 ⇒ 主 spec `cpu-cache-subsystem`
 *      的 grep 护栏（`[^_]if\s*\(.*io\(\)\.`）会把**注释里**的字面形态
 *      也判为命中 ⇒ 逐字引用会让护栏**恒红**（护栏的已知边界，
 *      见该 change 的 design.md §8）。
 *
 * **当前语义（直通式）**：
 * - CPU 请求（`addr` / `req`）**直通**内层 `ICacheSync`；
 * - 数据 / 就绪 / 未命中（`data` / `ready` / `miss`）**转发**内层；
 * - AXI 主端（`axi_addr` / `axi_valid`）**转发**内层
 *   ⇒ 内层 `ICacheSync` **本身**就是完整的 AXI 主端（`i_cache_sync.h`）
 *   ⇒ wrapper 层**不**重推导 ⇒ AXI 的「**VALID 不得等 READY**」纪律天然成立；
 * - 内层 cache 的 **5 个输入端口全部被显式驱动**
 *   （未驱动端口读到的是**未定义值** ⇒ `miss` 恒为「冷 miss」
 *   ⇒ hit / miss **不可区分**）。
 *
 * ⚠️ **未实现 / 已知边界（如实登记；backlog 见该 change 的 design.md §7）**：
 * - **预取尚未实现**：`prefetch_data` / `prefetch_addr` / `prefetch_valid`
 *   三个缓冲区目前是**死数据**（`PrefetchConfig` 只描述尺寸）⇒ 真预取 = 独立立项；
 * - 内层的 `miss` 来自 `sread`（**1 拍**注册读）而 `axi_addr` 取**当拍**地址
 *   ⇒ 二者存在 **1 拍错位**（属**内层 cache 自身**的问题
 *   ⇒ 转发**不**加剧、也**不**修它）。
 *
 * 作者：DevMate
 * 日期：2026-04-10（2026-10-28 行为订正）
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "i_cache_complete.h"
#include "module.h"

namespace chlib {

using namespace ch::core;

/**
 * @brief I-Cache 预取配置
 *
 * ⚠️ 2026-10-28：**只有尺寸常量** —— 预取功能本身**尚未实现**
 *    （缓冲区目前是死数据）⇒ 真预取 = 独立立项。
 */
struct PrefetchConfig {
    static constexpr unsigned PREFETCH_BUFFER_SIZE = 2;  // 预取缓冲条目数
    static constexpr unsigned MAX_PENDING_REQUESTS = 2;  // 最大未响应请求数
};

/**
 * @brief I-Cache 预取控制器
 */
class ICachePrefetch : public ch::Component {
public:
    __io(
        // CPU 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_bool> req;               // 请求有效

        // 数据输出
        ch_out<ch_uint<32>> data;         // 读出指令
        ch_out<ch_bool> ready;            // 就绪信号
        ch_out<ch_bool> miss;             // 未命中信号

        // AXI 主设备接口
        ch_out<ch_uint<32>> axi_addr;     // AXI 读地址
        ch_out<ch_bool> axi_valid;        // AXI 请求有效
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_uint<32>> axi_data;      // AXI 读数据
        ch_in<ch_bool> axi_resp_valid;    // AXI 响应有效
    )

    ICachePrefetch(ch::Component* parent = nullptr,
                   const std::string& name = "icache_prefetch")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // 内层 I-Cache（ch_module 创建子 component, 自动管理 Component::current()）
        // =====================================================================
        ch::ch_module<ICacheComplete> icache_base{"icache_base"};
        auto& icache = icache_base.instance();

        // =====================================================================
        // ⚠️ 内层 cache 的输入 MUST **全部显式驱动**
        //
        // 未驱动的输入端口读到的是**未定义值**（根 `AGENTS.md §ANTI-PATTERNS`
        // 登记：实测 `strbe` 读**全 1**、`rst_n` 读 0）⇒ 依赖默认值会让测试
        // 「**碰巧**通过」而跨后端行为可能分叉。
        // ⇒ 此前这 **5 个端口全部未驱动** ⇒ 内层 `addr` 恒 0 ⇒ `miss` 恒为
        //   「冷 miss」⇒ hit / miss **不可区分**。
        // =====================================================================
        icache.io().addr           <<= io().addr;
        icache.io().req            <<= io().req;
        icache.io().axi_ready      <<= io().axi_ready;
        icache.io().axi_data       <<= io().axi_data;
        icache.io().axi_resp_valid <<= io().axi_resp_valid;

        // =====================================================================
        // 预取缓冲 (ch_mem-based per ADR-039 + cache-complete-doa-note §6.1 R1)
        // =====================================================================
        // C-style ch_reg<T>[N] 数组在 runtime init 时触发 SIGSEGV (Context 还未就绪)。
        // ch_mem<T, N> 默认值 0, sread/write 接口兼容, 不会 SIGSEGV。
        //
        // ⚠️ 2026-10-28：**预取功能尚未实现** ⇒ 这三个缓冲区目前是**死数据**
        //    （既无读端口也无写端口）⇒ 真预取 = 独立立项
        //    （见该 change 的 design.md §7 的 backlog）。
        ch_mem<ch_uint<32>, PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_data;
        ch_mem<ch_uint<32>, PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_addr;
        ch_mem<ch_uint<1>,   PrefetchConfig::PREFETCH_BUFFER_SIZE> prefetch_valid;
        // ch_mem 默认 0, 无需 runtime init loop
        (void)prefetch_data; (void)prefetch_addr; (void)prefetch_valid;

        // =====================================================================
        // 数据输出（**无条件**驱动）
        // =====================================================================
        // fix: ICacheSync/ICacheAsync 没有 `hit` 输出端口, 只有 `miss`
        //       (root AGENTS.md ANTI-PATTERNS)
        ch_bool cache_miss(icache.io().miss.impl());
        io().data  <<= icache.io().data;
        io().ready <<= icache.io().ready;
        io().miss  <<= cache_miss;

        // =====================================================================
        // AXI 主端：**转发**内层（**不**在 wrapper 层重推导）
        //
        // ⚠️ 为什么 MUST 转发而不是重算（3 条理由）：
        //   ① **AXI 合规**：内层的 `axi_valid` **不含** `axi_ready`
        //      （AXI 要求「VALID **不得**等 READY 才拉高」）⇒ wrapper 重推导
        //      就得把 READY 引进 VALID ⇒ **违反 AXI**；
        //   ② **无死逻辑**：wrapper 重推导 ⇒ 内层的 `axi_addr` / `axi_valid`
        //      **无人消费** ⇒ 违反 ZERO-DEBT「无死代码」；
        //   ③ **值同源**：内层的 `icache_get_block_addr`（`i_cache_sync.h`）
        //      = `addr & 0xFFFFFFF0` ⇒ 与 wrapper 原先的 `get_block_addr`
        //      **逐位相同** ⇒ 转发**不改变值**，只是消除重复实现。
        // =====================================================================
        io().axi_addr  <<= icache.io().axi_addr;
        io().axi_valid <<= icache.io().axi_valid;
    }
};

} // namespace chlib
