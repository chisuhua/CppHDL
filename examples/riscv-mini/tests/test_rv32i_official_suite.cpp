/**
 * @file test_rv32i_official_suite.cpp
 * @brief **核级判据**：官方 riscv-tests `rv32ui-p-*`（40 个）全过
 *
 * ## 判据是什么
 *
 * 跑**上游** riscv-tests 的物理内存（`p`）测试集（**测试体一字未改**），
 * 逐个判定「官方 env 写 `tohost` 的值」：
 *
 * | `tohost` | 含义 |
 * |---|---|
 * | `1` | **PASS** |
 * | `TESTNUM \| 1`（≥ 3） | **FAIL**（`TESTNUM` = 被判失败的用例号） |
 *
 * ⚠️ **期望值来自上游 env 的协议**，不是本仓自拟 ⇒ ⇒ 判定口径与上游
 *   host 侧（riscv-tests 的 `run.py`）**同源**。
 *
 * ## 镜像从哪来
 *
 * `examples/riscv-mini/tests/riscv-tests/images/<name>/{itcm.mem,dtcm.mem}`
 * = 上游 riscv-tests 编译产物的 `rv32i-fw-dump --format mem-split` 双区域字镜像。
 * 来源 / 改造 / 排除项 / 复现方法 = **同目录的 `PROVENANCE.md`**。
 *
 * ## 怎么观察 `tohost`
 *
 * `tohost` 在 D-TCM 的**首字**（`0x8001_0000`）⇒ ⇒ 核**自己**写它。
 * ⇒ ⇒ 本判据观察核的**既有根端口**上的 **store 提交事件**
 * （`obs_data_write_en` / `obs_data_addr` / `obs_data_write_data`）：
 * 地址 == `0x8001_0000` 的那次写 ⇒ 就是 env 写 `tohost` ⇒ 值即判定依据。
 *
 * ⚠️ **不新增任何端口** ⇒ ⇒ 核的 53 端口契约表与 C6 快照**零改动**。
 * ⚠️ **不读 D-TCM 内容**（D-TCM 在核内部，无读出口）⇒ ⇒ 用「写事件」代替
 *   「读 tohost」⇒ ⇒ 这也顺带覆盖了 **store 路径**（D-L1 写穿 → D-TCM）。
 *
 * ## 为什么放在**核级**
 *
 * 官方测试要跑在**真正的核**上（取指走 I-TCM、数据走 D-TCM）⇒ ⇒ 判据落在
 * `ch_device<Rv32iCoreTop, itcm, dtcm>`（核 IP 顶层）⇒ ⇒ 与
 * `test_rv32i_stall_integrity` 同一个 DUT 形态。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../../../tests/catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "simulator.h"
#include "rv32i_core_top.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

// ⚠️ **D-TCM 的首字** = `tohost`（链接布局的固定地址，见 PROVENANCE.md §2.2）
constexpr std::uint64_t kTohostAddr = 0x80010000ull;

// ⚠️ **单测试的拍数上限**：实测 40 个测试合计约 1.3 万拍（平均 ≈ 330）
//   ⇒ ⇒ 4000 拍是平均值的 12 倍余量；超限 = **判失败**（fail-closed）
//   ⇒ ⇒ **不是** skip（skip 会让「跑不完」看起来像「通过」）。
constexpr std::size_t kMaxCycles = 4000;

std::filesystem::path images_root() {
    return std::filesystem::path(CPPHDL_SOURCE_DIR) / "examples" / "riscv-mini" /
           "tests" / "riscv-tests" / "images";
}

/// 读一个 `mem-split` 镜像：每行 1 个 32 位字（小端 8 位十六进制、无前缀）
std::vector<std::uint32_t> load_image(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in) return {};   // 文件缺失 ⇒ 空镜像 ⇒ 判据在下面显式失败
    std::vector<std::uint32_t> words;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        words.push_back(static_cast<std::uint32_t>(std::stoul(line, nullptr, 16)));
    }
    return words;
}

struct Outcome {
    bool saw_tohost = false;
    std::uint64_t tohost_value = 0;
    std::uint64_t cycles = 0;
    std::uint64_t retired = 0;
};

Outcome run_images(const std::vector<std::uint32_t>& itcm,
                   const std::vector<std::uint32_t>& dtcm) {
    Outcome out;
    if (itcm.empty() || dtcm.empty()) return out;   // 镜像缺失 ⇒ saw_tohost=false ⇒ 判失败

    using Dev = ch::ch_device<Rv32iCoreTop, const std::vector<std::uint32_t>&,
                               const std::vector<std::uint32_t>&>;
    auto dev = std::make_unique<Dev>(itcm, dtcm);
    ch::Simulator sim(dev->context());
    auto& io = dev->instance().io();

    // ⚠️ **核级没有 MMIO 从设备** ⇒ 8 根 `axil_*` 输入**显式 tie 0**
    //   （未驱动 ⇒ 读到未定义值 ⇒ 期望值可能「碰巧」通过 ⇒ 恒绿判据）
    sim.set_input_value(io.ext_rst, 0u);
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);
    sim.set_input_value(io.axil_awready, 0u);
    sim.set_input_value(io.axil_wready, 0u);
    sim.set_input_value(io.axil_bresp, 0u);
    sim.set_input_value(io.axil_bvalid, 0u);
    sim.set_input_value(io.axil_arready, 0u);
    sim.set_input_value(io.axil_rdata, 0u);
    sim.set_input_value(io.axil_rresp, 0u);
    sim.set_input_value(io.axil_rvalid, 0u);

    for (std::size_t c = 0; c < kMaxCycles; ++c) {
        sim.tick(1);
        ++out.cycles;

        // store 提交事件（**单拍脉冲**：写使能 && 总线就绪 —— 与 `obs_perf_instructions` 同口径）
        const auto we = static_cast<std::uint64_t>(sim.get_port_value(io.obs_data_write_en));
        if (we == 0) continue;
        const auto addr = static_cast<std::uint64_t>(sim.get_port_value(io.obs_data_addr));
        if (addr != kTohostAddr) continue;   // 不是 tohost 的写 ⇒ 忽略（测试体自己的 store）
        const auto data = static_cast<std::uint64_t>(sim.get_port_value(io.obs_data_write_data));
        out.saw_tohost = true;
        out.tohost_value = data & 0xFFFFFFFFull;
        break;
    }
    out.retired = static_cast<std::uint64_t>(sim.get_port_value(io.obs_perf_instructions));
    return out;
}

Outcome run_official(const std::string& name) {
    const auto dir = images_root() / name;
    return run_images(load_image(dir / "itcm.mem"), load_image(dir / "dtcm.mem"));
}

// ── 40 个官方测试（= 上游 isa/rv32ui/Makefrag 的 rv32ui_sc_tests 减去 2 个排除项）──
// ⚠️ 排除项与理由 = `riscv-tests/PROVENANCE.md` §3（`fence_i` = 无 Zifencei 扩展；
//   `ma_data` = **非对齐** load/store ⇒ 本核车道选择忽略低地址位）⇒ ⇒ 排除是
//   **登记过的能力对齐**，不是静默缩小。
//
// ⚠️⚠️ **为什么分成 4 组、每组一个独立二进制**（不是「一个二进制 + 4 个 ctest 过滤参数」）：
//   · 单次跑完 40 个 ≈ **7 分钟**（解释器后端 ≈ 36 拍/秒；该核的图 ≈ 11 万节点）
//     ⇒ ⇒ 超过仓内的**单测 300 s 上限**（ctest `TIMEOUT 300` + 护栏
//     `verify_example_test_wiring.sh` C3 的硬编码 300 s）。
//   · 护栏 C3 是按「**ctest 条目名 = 可执行文件名**」找二进制并**不带参数**运行
//     ⇒ ⇒ 「同一二进制 + 不同 ctest 过滤参数」会让护栏跑到**默认集合**
//     ⇒ ⇒ ⇒ 那就与 ctest 条目**不等价**（静默缩面）⇒ ⇒ MUST 是**独立二进制**。
//   · 4 组 × 10 个 ⇒ 每组 ≈ 105 s（300 s 上限的 1/3 ⇒ 余量充足），
//     且 CI 的 `ctest -j$(nproc)` 让 4 组**并行** ⇒ 墙钟 ≈ 单组时长。
#ifndef RV32I_OFFICIAL_GROUP
#error "MUST 用 -DRV32I_OFFICIAL_GROUP=<1..4> 编译（见本文件「为什么分成 4 组」注释）"
#endif

#if RV32I_OFFICIAL_GROUP == 1
#define RV32I_OFFICIAL_TESTS(X)                                               \
    X(add) X(addi) X(and) X(andi) X(auipc) X(beq) X(bge) X(bgeu) X(blt)        \
    X(bltu)
#elif RV32I_OFFICIAL_GROUP == 2
#define RV32I_OFFICIAL_TESTS(X)                                               \
    X(bne) X(jal) X(jalr) X(lb) X(lbu) X(lh) X(lhu) X(lw) X(ld_st) X(lui)
#elif RV32I_OFFICIAL_GROUP == 3
#define RV32I_OFFICIAL_TESTS(X)                                               \
    X(or) X(ori) X(sb) X(sh) X(sw) X(st_ld) X(sll) X(slli) X(slt) X(slti)
#elif RV32I_OFFICIAL_GROUP == 4
#define RV32I_OFFICIAL_TESTS(X)                                               \
    X(sltiu) X(sltu) X(sra) X(srai) X(srl) X(srli) X(sub) X(xor) X(xori)       \
    X(simple)
#else
#error "RV32I_OFFICIAL_GROUP 只能是 1..4"
#endif

#define RV32I_OFFICIAL_CASE(NAME)                                              \
    TEST_CASE("official riscv-tests: rv32ui-p-" #NAME,                        \
              "[riscv-mini][riscv-tests][official]") {                         \
        const auto out = run_official("rv32ui-p-" #NAME);                      \
        INFO("cycles=" << out.cycles << " retired=" << out.retired             \
                      << " tohost=" << out.tohost_value);                      \
        REQUIRE(out.saw_tohost);        /* 必须在拍数上限内写完 tohost */       \
        REQUIRE(out.tohost_value == 1ull);   /* 上游协议：1 = PASS */          \
    }

RV32I_OFFICIAL_TESTS(RV32I_OFFICIAL_CASE)

// ============================================================================
// 负向对照：运行器 MUST 能区分「PASS（1）」与「FAIL（TESTNUM|1）」
// ============================================================================
// ⚠️ **为什么需要它**：门禁的正向判据是「`tohost == 1` ⇒ PASS」⇒ ⇒ 若运行器
//   **恒**看到 1（例如地址过滤写错 ⇒ 总读到某个恒 1 的值），全部 40 个正向
//   用例会**恒绿** ⇒ ⇒ 那是**恒绿判据**。
//   ⇒ ⇒ 本对照构造一个**手写**镜像：写 `3`（= `TESTNUM|1`，TESTNUM = 2 的 FAIL 形态）
//   ⇒ ⇒ MUST 被运行器**如实**读到 3（而不是 1）⇒ ⇒ 证明读的是**真的 tohost 值**。
//
// 程序（工具链汇编 + objdump 核对）：
//   0x00 lui  x5, 0x80010  → x5 = 0x8001_0000（tohost 的地址）
//   0x04 addi x6, x0, 3    → x6 = 3
//   0x08 sw   x6, 0(x5)   → 把 3 写进 tohost（= FAIL 值形态）
//   0x0c jal  x0, 0       → 停机
TEST_CASE("official riscv-tests: 负向对照（tohost 的 FAIL 值 MUST 被如实读到）",
          "[riscv-mini][riscv-tests][official]") {
    const std::vector<std::uint32_t> itcm = {
        0x800102B7u, 0x00300313u, 0x0062A023u, 0x0000006Fu};
    const std::vector<std::uint32_t> dtcm = {0u};

    const auto out = run_images(itcm, dtcm);
    INFO("cycles=" << out.cycles << " retired=" << out.retired
                   << " tohost=" << out.tohost_value);
    REQUIRE(out.saw_tohost);
    REQUIRE(out.tohost_value == 3ull);   // 手推：写进去的就是 3
}

}  // namespace
