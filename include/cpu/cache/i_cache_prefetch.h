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
 * - AXI4-full 主端（`ar_*` 出向 + `r_*` 入出向）**逐根转发**内层
 *   ⇒ 内层 `ICacheSync` **本身**就是完整的 AXI 主端（`i_cache_sync.h`）
 *   ⇒ wrapper 层**不**重推导 ⇒ AXI 的「**VALID 不得等 READY**」纪律天然成立；
 * - 内层的 **7 个输入端口全部被显式驱动**
 *   （未驱动端口读到的是**未定义值** ⇒ `miss` 恒为「冷 miss」
 *   ⇒ hit / miss **不可区分**）。
 *
 * ⚠️ **2026-11-28 · S4c-1a（ADR-078 D7）**：内层 AXI 侧由 5 根简化握手升级为
 *   **AXI4-full 读通道**（`ar_*` 6 根 + `r_*` 5 根，**只读 ⇒ 无写通道**）
 *   ⇒ ⇒ 本 wrapper 的 **AXI 侧转发面同步扩**；**前端转发面（`addr`/`req`/
 *   `data`/`ready`/`miss`）逐字不变** ⇒ ⇒ 爆炸半径只落在 AXI 侧。
 *
 * ⚠️ **未实现 / 已知边界（如实登记；backlog 见该 change 的 design.md §7）**：
 * - **预取尚未实现** ⇒ ⚠️ **2026-11-09（phase-6 **P9** · Zero-Debt）**：原三个预取缓冲
 *   （`prefetch_data` / `prefetch_addr` / `prefetch_valid`）与 `PrefetchConfig`
 *   **已删除**（**零消费者**的死数据 ⇒ Zero-Debt「No dead code」）⇒ ⇒
 *   **本组件现在不含任何预取状态** ⇒ 真预取 = **独立立项**（届时重新引入配置面）。
 * - 内层的 `miss` 来自 `sread`（**1 拍**注册读）而 `ar_addr` 取**当拍**地址
 *   ⇒ 二者存在 **1 拍错位**（属**内层 cache 自身**的问题
 *   ⇒ 转发**不**加剧、也**不**修它）
 *   ⇒ ⇒ **调用方 MUST 在 `miss` / 填充期间保持 `addr` 不变**（契约见 capability
 *   `cpu-cache-subsystem` 的「`miss` 期间的地址稳定性」Requirement）
 *   ⇒ ⇒ 违反该约束 ⇒ 取回的是**新块**的数据（判别性证据见
 *   `examples/riscv-mini/tests/test_cache_smoke.cpp` 的 miss/addr 探针用例）。
 *
 * 作者：DevMate
 * 日期：2026-04-10（2026-10-28 行为订正 · 2026-11-28 AXI4-full 转发面）
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "i_cache_complete.h"
#include "module.h"

namespace chlib {

using namespace ch::core;

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

        // ---- AXI4 full 主口 · AR（**只读** ⇒ 无 AW/W/B 写通道）----
        ch_out<ch_uint<32>> ar_addr{"ar_addr"};
        ch_out<ch_uint<4>>  ar_len{"ar_len"};
        ch_out<ch_uint<3>>  ar_size{"ar_size"};
        ch_out<ch_uint<2>>  ar_burst{"ar_burst"};
        ch_out<ch_bool>     ar_valid{"ar_valid"};
        ch_in<ch_bool>      ar_ready{"ar_ready"};

        // ---- AXI4 full 主口 · R ----
        ch_in<ch_uint<32>>  r_data{"r_data"};
        ch_in<ch_uint<2>>   r_resp{"r_resp"};
        ch_in<ch_bool>      r_last{"r_last"};
        ch_in<ch_bool>      r_valid{"r_valid"};
        ch_out<ch_bool>     r_ready{"r_ready"};
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
        // ⇒ 此前这些端口全部未驱动 ⇒ 内层 `addr` 恒 0 ⇒ `miss` 恒为
        //   「冷 miss」⇒ hit / miss **不可区分**。
        // =====================================================================
        icache.io().addr    <<= io().addr;
        icache.io().req     <<= io().req;
        icache.io().ar_ready <<= io().ar_ready;
        icache.io().r_data  <<= io().r_data;
        icache.io().r_resp  <<= io().r_resp;
        icache.io().r_last  <<= io().r_last;
        icache.io().r_valid <<= io().r_valid;

        // =====================================================================
        // 预取缓冲：**已删除**（2026-11-09 · phase-6 P9 · Zero-Debt）
        // =====================================================================
        // 原先的 `prefetch_data` / `prefetch_addr` / `prefetch_valid` 三个
        // `ch_mem` 缓冲**既无读端口也无写端口** ⇒ 零消费者的死数据 ⇒ 已删
        // （删除登记见 `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的
        // 公开符号」章）⇒ ⇒ 真预取 = **独立立项**（届时重新引入缓冲与配置面）。
        // ⚠️ 本组件**不得**再挂 C-style `ch_reg<T>[N]` 数组
        //    （runtime init 触发 SIGSEGV ⇒ ADR-039 + cache-complete-doa-note §6.1 R1）。

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
        // AXI4-full 主端：**转发**内层（**不**在 wrapper 层重推导）
        //
        // ⚠️ 为什么 MUST 转发而不是重算（3 条理由）：
        //   ① **AXI 合规**：内层的 `ar_valid` **不含** `ar_ready`
        //      （AXI 要求「VALID **不得**等 READY 才拉高」）⇒ wrapper 重推导
        //      就得把 READY 引进 VALID ⇒ **违反 AXI**；
        //   ② **无死逻辑**：wrapper 重推导 ⇒ 内层的 `ar_addr` / `ar_valid`
        //      **无人消费** ⇒ 违反 ZERO-DEBT「无死代码」；
        //   ③ **值同源**：内层的 `ar_addr` = `addr & 0xFFFFFFF0`（`i_cache_sync.h`）
        //      ⇒ 转发**不改变值**，只是消除重复实现。
        // =====================================================================
        io().ar_addr  <<= icache.io().ar_addr;
        io().ar_len   <<= icache.io().ar_len;
        io().ar_size  <<= icache.io().ar_size;
        io().ar_burst <<= icache.io().ar_burst;
        io().ar_valid <<= icache.io().ar_valid;
        io().r_ready  <<= icache.io().r_ready;
    }
};

} // namespace chlib
