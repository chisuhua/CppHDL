// examples/riscv-full-soc/tb_riscv_full_soc.cpp
//
// **full 产品线的 SoC 级产品单元**（ADR-078 · S4c-0 · **空壳**）
//
// ⚠️ **本批不发 RTL**（S4c-0 范围）⇒ ⇒ 本文件**不是**发射入口 ⇒ ⇒ 它在
//   `.rddf/checks/examples-rtl-sim-exclusions.tsv` 的**落选登记**里（理由 = 无 RTL 产物）。
//   ⇒ RTL 发射（`--emit-rtl` / `--check-products`）从 **S4c-1** 起。
//
// ⚠️ **它为什么存在**：新线 MUST **立即进入**运行护栏（`run_all_ported_tests.sh`）与
//   全部清单的扫描面 ⇒ ⇒ 「漏登记」当场红 ⇒ ⇒ 本文件是那把探针。
//
// ⚠️ **期望值手推**（禁抄仿真输出）：空壳只有 2 根**恒定**输出 ⇒ ⇒ 两条判据。

#include "ch.hpp"
#include "device.h"
#include "rv32i_full_soc_top.h"
#include "simulator.h"

#include <cstdint>
#include <iostream>

namespace {

/// 判据计数器（失败进 stderr · 结论行进 stdout · 与仓内 `RESULT:` 协议一致）
class Checker {
public:
    void check(bool ok, const char *what) {
        ++checks_;
        if (!ok) {
            ++fails_;
            std::cerr << "CHECK FAIL: " << what << "\n";
        }
    }
    std::size_t checks() const { return checks_; }
    std::size_t fails() const { return fails_; }

private:
    std::size_t checks_ = 0;
    std::size_t fails_ = 0;
};

} // namespace

int main() {
    using namespace ch;
    Checker ck;

    auto dev = std::make_unique<ch_device<riscv_full_soc::Rv32iFullSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    sim.tick(1);

    // ── A1：`line_id` == 产品线序号 2（手推：`kFullLineId = 2`）──
    ck.check(static_cast<uint64_t>(sim.get_port_value(io.line_id)) ==
                 static_cast<uint64_t>(riscv_full_soc::kFullLineId),
             "A1 line_id == 2（full 线的稳定标识）");

    // ── A2：`line_ready` 恒 true（空壳无输入 ⇒ 无复位态差异）──
    ck.check(static_cast<uint64_t>(sim.get_port_value(io.line_ready)) == 1u,
             "A2 line_ready == 1（空壳恒定）");

    // ── A3：跨拍恒定（负向对照：若空壳被误接状态 ⇒ 第二拍会不同）──
    const uint64_t id1 = static_cast<uint64_t>(sim.get_port_value(io.line_id));
    for (int i = 0; i < 7; ++i) {
        sim.tick(1);
    }
    ck.check(static_cast<uint64_t>(sim.get_port_value(io.line_id)) == id1,
             "A3 line_id 跨 8 拍恒定（空壳无状态）");

    std::cout << "RESULT: checks=" << ck.checks() << " fails=" << ck.fails() << "\n";
    return ck.fails() == 0 ? 0 : 1;
}
