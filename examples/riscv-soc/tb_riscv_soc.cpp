/**
 * RV32I SoC 级产品 —— **激励驱动 + 发射入口**（ADR-077 D1 · change `riscv-soc-skeleton-2026-11`）
 *
 * ⚠️ **设计定义在 `src/rv32i_soc_top.h`**（`Rv32iSocTop` + 2 处 `ch::ch_layer<T>`）；
 *    本文件三个模式：
 *      · **默认** = 解释器路径 + 手推断言 + `RESULT:` 协议行
 *      · **`--emit-rtl <dir>`** = 发射（恒加 `--hierarchical`）
 *      · **`--check-products <dir>`** = 发射 + 4 项产品级自检
 *
 * ⚠️ **期望值 MUST 手推**（禁抄仿真输出）：程序与写回/存储结果**同源于**核级产品
 *    （`.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` 的「小程序」表）
 *    ⇒ ⇒ 本文件**不新造**任何期望 ⇒ 手推台账零重复。
 *
 * ⚠️ **SoC 级看不到核的 `dbg_*` / `obs_*`**（ADR-072 D1：后代 `ch_out` 不是顶层端口）
 *    ⇒ ⇒ 本文件的观测面 = **根自有**的两个计数（`store_count` / `writeback_count`）。
 *
 * ⚠️ **本文件只留「值」**（期望 module 集 / 端口数 / 激励面 / 观测面 / 阈值）；
 *    规则侧（4 项判据 + 编排）在库层 `ch::rtl_emit::check_products`。
 */

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/product_check.h"
#include "rv32i_soc_top.h"
#include "soc_product_face.h"   // 产品面常量（module 集 / 端口数 · 单一真相）
#include "simulator.h"

#include "cpu/riscv/rv32i_asm_encode.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using riscv::ADD;
using riscv::ADDI;
using riscv::JAL;
using riscv::LUI;
using riscv::LW;
using riscv::NOP;
using riscv::SW;

// ---------------------------------------------------------------------------
// **12 字**小程序（**手推** · S4a 起与核级产品**分叉**）
//
// ⚠️ **为什么 MUST 分叉**（结构性 · 不是偏好）：核级 TB（`examples/riscv-mini`）
//    的 AXI-Lite 输入全 tie 0 ⇒ ⇒ 核级**没有** MMIO 从设备 ⇒ ⇒ 核级程序做 MMIO
//    访问会**挂死** ⇒ ⇒ MMIO 访问只能出现在 SoC 级程序里。
//
// | 字 | 指令 | 语义 |
// |---|---|---|
// | 0 | `addi x1, x0, 42` | 写回 x1 = 42 |
// | 1 | `nop` | 余量 |
// | 2 | `addi x2, x0, 11` | 写回 x2 = 11 |
// | 3 | `nop` | 余量 |
// | 4 | `add x3, x1, x2` | 写回 x3 = **53** |
// | 5 | `sw x3, 0(x0)` | store @ 字节地址 **0** ⇒ **走 D-L1**（write-through 单拍） |
// | 6 | `nop` | 余量 |
// | 7 | `lui x4, 0x80102` | 写回 x4 = **0x8010_2000** = `kGpio.base` |
// | 8 | `lw x6, 4(x4)` | **MMIO 读** @ `0x8010_2004`（DATA_IN）⇒ 写回 x6 |
// | 9 | `addi x5, x0, 0xA5` | 写回 x5 = **0xA5** |
// | 10 | `sw x5, 0(x4)` | **MMIO 写** @ `0x8010_2000`（DATA_OUT）· 数据 0xA5 |
// | 11 | `jal x0, 0` | 自环（`rd = x0` ⇒ **不写回**） |
//
// ⇒ ⇒ `store_count == 2`（**两条路径各一次**）· `writeback_count == 6`
//     · `gpio_out == 0xA5`（MMIO **写通路的值**）
// ---------------------------------------------------------------------------

constexpr std::size_t kProgWords = 12;

/// 程序末尾之后 MUST 填 NOP（`ch_mem` 初值 0 的 opcode = 未知 ⇒ 流水线冻结）。
constexpr std::size_t kFillWords = 64;

/// 观测拍数（与 `.sv` TB 的 `repeat` 一致）。
///
/// ⚠️ **S4a**：12 字程序含 **2 次 MMIO 事务**（读 1 次 + 写 1 次）⇒ ⇒ 每次多约
/// 3–4 拍（AW/W/B 或 AR/R 的逐拍握手）⇒ ⇒ 余量从 60 提到 **100**。
constexpr std::size_t kCycles = 100;

/// 尾段起点：此后计数 MUST 不再增长（`jal` 自环已稳定）。
constexpr std::size_t kTailStart = 85;

/// **手算**编码常量（独立于库编码器的交叉校验证据）。
constexpr uint32_t kHandEncoded[kProgWords] = {
    0x02A00093u, // 0  addi x1, x0, 42
    0x00000013u, // 1  nop
    0x00B00113u, // 2  addi x2, x0, 11
    0x00000013u, // 3  nop
    0x002081B3u, // 4  add  x3, x1, x2
    0x00302023u, // 5  sw   x3, 0(x0)        ← 字节地址 0 ⇒ **走 D-L1**
    0x00000013u, // 6  nop
    0x80102237u, // 7  lui  x4, 0x80102      ⇒ x4 = 0x8010_2000 = kGpio.base
    0x00422303u, // 8  lw   x6, 4(x4)         ← **MMIO 读**（DATA_IN）
    0x0A500293u, // 9  addi x5, x0, 0xA5
    0x00522023u, // 10 sw   x5, 0(x4)         ← **MMIO 写**（DATA_OUT = 0xA5）
    0x0000006Fu, // 11 jal  x0, 0             （rd = x0 ⇒ 不写回）
};

/// 程序字（取自库编码器 ⇒ 全仓唯一一份编码器）。
constexpr uint32_t kProgram[kProgWords] = {
    ADDI(1, 0, 42), NOP, ADDI(2, 0, 11), NOP, ADD(3, 1, 2), SW(3, 0, 0),
    NOP, LUI(4, 0x80102), LW(6, 4, 4), ADDI(5, 0, 0xA5), SW(5, 4, 0), JAL(0, 0),
};

// 手算常量 × 库编码器 的交叉校验（编译期 ⇒ 恒生效 ⇒ 零运行期成本）。
static_assert(kProgram[0] == kHandEncoded[0], "字 0 编码与手算常量不符");
static_assert(kProgram[1] == kHandEncoded[1], "字 1 编码与手算常量不符");
static_assert(kProgram[2] == kHandEncoded[2], "字 2 编码与手算常量不符");
static_assert(kProgram[3] == kHandEncoded[3], "字 3 编码与手算常量不符");
static_assert(kProgram[4] == kHandEncoded[4], "字 4 编码与手算常量不符");
static_assert(kProgram[5] == kHandEncoded[5], "字 5 编码与手算常量不符");
static_assert(kProgram[6] == kHandEncoded[6], "字 6 编码与手算常量不符");
static_assert(kProgram[7] == kHandEncoded[7], "字 7 编码与手算常量不符");
static_assert(kProgram[8] == kHandEncoded[8], "字 8 编码与手算常量不符");
static_assert(kProgram[9] == kHandEncoded[9], "字 9 编码与手算常量不符");
static_assert(kProgram[10] == kHandEncoded[10], "字 10 编码与手算常量不符");
static_assert(kProgram[11] == kHandEncoded[11], "字 11 编码与手算常量不符");

/// **手推**计数期望值（**事件次数**口径 ⇒ 与事务时长无关）
///
/// · `store_count = 2`：word 5（D-L1 路径）+ word 10（MMIO 旁路）——
///   ⇒ ⇒ **两条路径各一次** ⇒ ⇒ 分流判据写反 / 只接一条路径**必红**
/// · `writeback_count = 6`：x1 / x2 / x3 / x4 / x6 / x5
///   ⇒ ⇒ `jal x0`（word 11）的 `rd = x0` ⇒ **不写回**
///   ⇒ ⇒ `lw x6`（word 8）**写回**（load 也写回 rd）
constexpr uint64_t kExpectedStoreCount = 2;
constexpr uint64_t kExpectedWritebackCount = 6;

/// MMIO 写的**手推值**：word 9 的 `0xA5` 经 word 10 写进 GPIO 的 DATA_OUT（reg0）
constexpr uint64_t kExpectedGpioOut = 0xA5;

/// 判据计数器（失败进 stderr · 结论行进 stdout）。
class Checker {
public:
    void check(bool ok, const std::string &what) {
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

// ── 值侧 ①：冻结的产物 module 期望集（ADR-077 V2 · **手推**）──────────────
//
// ⚠️ 禁从 `ch_layer` 调用点派生 —— 否则「漏一个成层声明」会**同时**改变期望
//    ⇒ ⇒ 判据恒绿。
// ⚠️ 与 `.rddf/checks/soc-rtl-expectations.md#riscv-soc` 登记的集合 MUST 一致。
// ⚠️ 产品面常量**搬到了** `src/soc_product_face.h`（2026-11-24）：把上游组件的
//    **类型名**写在本文件里会触发 `verify_rst_n_wiring.sh` 的粗网判据（误判 —— 本
//    文件并不实例化它们）。⇒ ⇒ 判据值仍**手推**、仍只有一份（该头）。
const std::vector<std::string> &kExpectedModules = riscv_soc::product_modules();

/// 顶层端口数 / 最小变化观测量数 ⇒ **单一真相**在 `src/soc_product_face.h`
constexpr std::size_t kExpectedTopPorts = riscv_soc::kProductTopPorts;
constexpr std::size_t kMinVaryingObservations = riscv_soc::kMinVaryingObservations;

// ── 平台胶水：verilator 发现 + 执行（照 `tb_axi_soc_example.cpp`）───────────
std::string find_verilator() {
    if (const char *env = std::getenv("VERILATOR"); env && *env) return env;
    const auto usable = [](const std::string &p) {
        return !p.empty() && std::filesystem::exists(p);
    };
    const std::string src_local =
        std::string(CPPHDL_SOURCE_DIR) + "/build/verilator-install/bin/verilator";
    if (usable(src_local)) return src_local;
    const std::string local = "build/verilator-install/bin/verilator";
    if (usable(local)) return local;
    if (const char *path = std::getenv("PATH")) {
        std::string p(path);
        std::size_t pos = 0;
        while ((pos = p.find(':', pos)) != std::string::npos) {
            const std::string cand = p.substr(0, pos) + "/verilator";
            if (usable(cand)) return cand;
            pos += 1;
        }
    }
    return {};
}

bool run_lint(const std::string &verilator, const std::string &top_module,
              const std::string &verilog_path) {
#if defined(_WIN32)
    (void)verilator;
    (void)top_module;
    (void)verilog_path;
    return false; // fail-closed：无 POSIX 进程 API ⇒ 按失败计
#else
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::execl(verilator.c_str(), verilator.c_str(), "--lint-only", "--top-module",
                top_module.c_str(), verilog_path.c_str(), static_cast<char *>(nullptr));
        ::_exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<riscv_soc::Rv32iSocTop>(nullptr, riscv_soc::kSocRootName);
}

// ---------------------------------------------------------------------------
// **方案 a**：ELF → init_data 向量 → 核取指（SoC 级）
//
// ⚠️ **SoC 级的观测面只有两个计数**（ADR-072 D1：根端口面 = 框架 2 + 根自有 6）
// ⇒ ⇒ SoC 级**看不到**核的 `dbg_*` / `obs_*` ⇒ ⇒ **无法**在此断言
//    「`.data` 经 `lw` 读回」⇒ ⇒ 那条判据由**核级**（`examples/riscv-mini`）承担
// ⇒ ⇒ SoC 级能证明的是「**固件真的在 SoC 里跑起来了**」（两个计数增长）。
// ---------------------------------------------------------------------------

/// 读一个字镜像文件（每行一个 32 位十六进制字）⇒ ⇒ **fail-closed**
std::vector<uint32_t> read_mem_image(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "FATAL：读不到固件镜像：" << path << "\n";
        std::exit(2);
    }
    std::vector<uint32_t> words;
    std::string line;
    std::size_t lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;
        if (line.size() > 2 && line[0] == '/' && line[1] == '/') continue;
        uint32_t value = 0;
        try {
            value = static_cast<uint32_t>(std::stoul(line, nullptr, 16));
        } catch (...) {
            std::cerr << "FATAL：固件镜像第 " << lineno << " 行不是合法十六进制："
                      << path << "\n";
            std::exit(2);
        }
        words.push_back(value);
    }
    return words;
}

/// 读一个区域的镜像（`<dir>/<name>.mem`）⇒ ⇒ 缺文件 ⇒ ⇒ 空镜像（**不**是错误）
std::vector<uint32_t> read_region_image(const std::string &dir,
                                         const std::string &name) {
    std::ifstream probe(dir + "/" + name + ".mem");
    if (!probe) return {};
    return read_mem_image(dir + "/" + name + ".mem");
}

/// `--firmware <dir>` 的**验证模式**（解释器路径 · SoC 级）
int run_firmware_verification(const std::string &dir) {
    Checker ck;

    const std::vector<uint32_t> itcm = read_region_image(dir, "itcm");
    const std::vector<uint32_t> dtcm = read_region_image(dir, "dtcm");
    if (itcm.empty()) {
        std::cerr << "FATAL：固件镜像为空（" << dir << "/itcm.mem）\n";
        return 2;
    }

    // ⚠️ **装载走构造参数** ⇒ ⇒ **零 `prog_*` 写** ⇒ ⇒ 根端口面一字未动
    using FwDev = ch::ch_device<riscv_soc::Rv32iSocTop, const std::vector<uint32_t> &,
                                 const std::vector<uint32_t> &>;
    auto dev = std::make_unique<FwDev>(itcm, dtcm);
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ **`prog_*` 保持 0** ⇒ ⇒ 装载期**不存在** ⇒ ⇒ 计数从 0 起算
    sim.set_input_value(io.ext_rst, 0u);
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);
    // S4a：GPIO 的输入面（根端口）MUST 显式驱动（未驱动 ⇒ 读到未定义值）
    sim.set_input_value(io.gpio_in, 0u);

    uint64_t last_store = 0;
    uint64_t last_wb = 0;
    bool monotonic = true;
    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);
        const uint64_t store = static_cast<uint64_t>(sim.get_port_value(io.store_count));
        const uint64_t wb = static_cast<uint64_t>(sim.get_port_value(io.writeback_count));
        if (store < last_store || wb < last_wb) monotonic = false;
        last_store = store;
        last_wb = wb;
    }

    // ── G1：两个计数单调不减（impl-contract）──
    ck.check(monotonic, "G1 两个计数单调不减");
    // ── G2：写回计数 > 0 ⇒ ⇒ 固件真的在 SoC 里执行了 ──
    ck.check(last_wb > 0u, "G2 writeback_count > 0（固件真的在跑）");

    std::cout << "SOC-FIRMWARE-TRACE: store_count=" << last_store
              << " writeback_count=" << last_wb << "\n";
    std::cout << "RESULT: checks=" << ck.checks() << " fails=" << ck.fails() << "\n";
    return ck.fails() == 0 ? 0 : 1;
}

// ── 值侧 ②：双形态逐拍等价的激励面 / 观测面 ───────────────────────────────
//
// ⚠️ 输入面 MUST 覆盖根的**全部** 4 个 `ch_in`（缺一个 ⇒ 该端口在比对里读到未定义值）。
std::vector<ch::hier_eq::DrivenInput> make_inputs(riscv_soc::Rv32iSocTop &root) {
    std::vector<ch::hier_eq::DrivenInput> in;
    // ⚠️ MUST 用 `set_input_value`（`<<=` 会把端口变成内部驱动 ⇒ 假分歧）
    in.push_back({"top.ext_rst", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().ext_rst, v & 1);
                  }});
    in.push_back({"top.prog_we", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_we, v & 1);
                  }});
    in.push_back({"top.prog_addr", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_addr, v & 0xFFFFFFFFu);
                  }});
    in.push_back({"top.prog_data", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_data, v & 0xFFFFFFFFu);
                  }});
    return in;
}

/// 顶层观测面（**只能**取根的 `ch_out` —— 后代 `ch_out` 在层次化顶层不可观测）。
std::vector<ch::hier_eq::Observation> make_observations(riscv_soc::Rv32iSocTop &root) {
    std::vector<ch::hier_eq::Observation> obs;
    obs.push_back(ch::hier_eq::make_observation(
        "top.store_count", root.io().store_count, [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().store_count));
        }));
    obs.push_back(ch::hier_eq::make_observation(
        "top.writeback_count", root.io().writeback_count,
        [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().writeback_count));
        }));
    return obs;
}

/// 激励序列：装载 7 字 → 填 NOP → 释放复位若干拍（合计 ≥10 拍且各拍值不同）。
///
/// ⚠️ 顺序 MUST 与 `make_inputs()` 一致：ext_rst · prog_we · prog_addr · prog_data
std::vector<std::vector<std::uint64_t>> make_stimulus() {
    std::vector<std::vector<std::uint64_t>> stim;
    // 装载期：ext_rst = 1（核被复位 ⇒ 计数器不被装载动作污染）
    for (std::size_t i = 0; i < kProgWords; ++i) {
        stim.push_back({1, 1, static_cast<std::uint64_t>(i),
                        static_cast<std::uint64_t>(kProgram[i])});
    }
    // 填充 NOP（地址继续递增 ⇒ 激励值持续变化）
    for (std::size_t i = kProgWords; i < 16; ++i) {
        stim.push_back({1, 1, static_cast<std::uint64_t>(i), static_cast<std::uint64_t>(NOP)});
    }
    // 释放复位 + 跑够拍数（计数在此段变化）
    for (std::size_t i = 0; i < 24; ++i) {
        stim.push_back({0, 0, 0, 0});
    }
    return stim;
}

ch::rtl_emit::ProductCheckSpec make_spec(const std::string &out_dir) {
    ch::rtl_emit::ProductCheckSpec spec;
    spec.verilog_path = out_dir + "/top.v";
    spec.work_dir = out_dir;
    spec.expected_modules = kExpectedModules;
    spec.expected_top_ports = kExpectedTopPorts;
    spec.min_varying_observations = kMinVaryingObservations;
    spec.lint_top_module = ch::HierarchyEmitOptions{}.top_module_name;
    spec.lint = [](const std::string &verilog_path) {
        const std::string verilator = find_verilator();
        if (verilator.empty()) return false; // fail-closed：找不到 linter ⇒ 按失败计
        return run_lint(verilator, ch::HierarchyEmitOptions{}.top_module_name, verilog_path);
    };
    return spec;
}

ch::rtl_emit::ProductCheckHooks make_hooks() {
    ch::rtl_emit::ProductCheckHooks hooks;
    hooks.make_inputs = [](ch::Component &root) {
        return make_inputs(static_cast<riscv_soc::Rv32iSocTop &>(root));
    };
    hooks.make_observations = [](ch::Component &root) {
        return make_observations(static_cast<riscv_soc::Rv32iSocTop &>(root));
    };
    hooks.make_stimulus = []() { return make_stimulus(); };
    return hooks;
}

// ---------------------------------------------------------------------------
// 默认模式：解释器路径 + 手推断言（A1–A6）
// ---------------------------------------------------------------------------

/// **A8 的载体**：独立的 SoC 实例 + 独立 4 字程序（**不污染** A1–A7 的两个计数）
///
/// 手推：`x1`（lui）· `x2`（lw）· `x3`（addi）各写回一次 ⇒ **3**
/// （`jal x0` 的 rd = x0 ⇒ 不写回）
/// ⇒ ⇒ **DECERR 未返回 ⇒ `lw` 永久停顿 ⇒ 写回冻结在 1**（只有 `lui`）⇒ ⇒ 必红。
bool dec_err_program_continues() {
    constexpr std::size_t kWords = 4;
    const uint32_t prog[kWords] = {
        LUI(1, 0x80100),  // x1 = 0x8010_0000 = kClint.base（**S4a 未接 CLINT**）
        LW(2, 1, 0),      // 读该地址 ⇒ MUST 得到 DECERR（而非挂死）
        ADDI(3, 0, 7),    // 若上一条挂死 ⇒ 这条永不写回
        JAL(0, 0),        // 自环
    };
    auto dev = std::make_unique<ch::ch_device<riscv_soc::Rv32iSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();
    // 装载期保持 ext_rst=1 ⇒ 计数器不被装载污染
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    sim.set_input_value(io.gpio_in, 0u);
    for (std::size_t i = 0; i < kFillWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data,
                            i < kWords ? static_cast<uint64_t>(prog[i])
                                       : static_cast<uint64_t>(NOP));
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);
    sim.set_input_value(io.ext_rst, 0u);

    uint64_t wb = 0;
    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);
        wb = static_cast<uint64_t>(sim.get_port_value(io.writeback_count));
    }
    return wb == 3u;
}

/// **A9 的载体**：MMIO **读值**回路（critic BLOCKER 的靶子 · 2026-11-24）
///
/// ## 为什么要它
///
/// `data_read_data` 在 **M2 级组合消费**（`rv32i_pipeline.h:345` 的
/// `load_lane.raw_word`），而那拍的 `data_addr` 已是**后继**指令的地址
/// ⇒ ⇒ 若 mux 的选择线直接用**当拍** `addr_in_mmio` ⇒ ⇒ 读到**另一条路径**的
/// **保持寄存器**（陈旧值）⇒ ⇒ 静默错值。
///
/// ## 程序（**手推**）· 5 字
///
/// | word | 指令 | 编码 | 作用 |
/// |---|---|---|---|
/// | 0 | `lui x4, 0x80102` | `0x80102237` | x4 = 0x8010_2000 |
/// | 1 | `lw x6, 4(x4)` | `0x00422303` | **MMIO 读** DATA_IN（根端口 `gpio_in` = **0x5A**） |
/// | 2 | `addi x7, x0, 1` | `0x00100393` | **非 MMIO 指令**（其 EX/MEM 地址 = 1 ∉ MMIO ⇒ 逼出错拍） |
/// | 3 | `sw x6, 0(x4)` | `0x00622023` | 把读回值写进 DATA_OUT |
/// | 4 | `jal x0, 0` | `0x0000006F` | 自环 |
///
/// ⇒ ⇒ 期望 `gpio_out == **0x5A**`（读值真的走了 MMIO 通路）。
/// ⇒ ⇒ ⚠️ 若读值取自 `dcache.rdata`（本场景**无** D-L1 读 ⇒ 恒 0）⇒ ⇒ 0 ≠ 0x5A ⇒ **必红**。
bool read_data_path_probe() {
    constexpr std::size_t kWords = 5;
    const uint32_t prog[kWords] = {
        LUI(4, 0x80102),   // x4 = 0x8010_2000 = kGpio.base
        LW(6, 4, 4),       // MMIO 读 DATA_IN
        ADDI(7, 0, 1),     // 非 MMIO 后继（逼出「用错周期地址」的缺陷）
        SW(6, 4, 0),       // 写回 DATA_OUT
        JAL(0, 0),
    };
    auto dev = std::make_unique<ch::ch_device<riscv_soc::Rv32iSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    sim.set_input_value(io.gpio_in, 0x5Au);   // DATA_IN 的手推值
    for (std::size_t i = 0; i < kFillWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data,
                            i < kWords ? static_cast<uint64_t>(prog[i])
                                       : static_cast<uint64_t>(NOP));
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);
    sim.set_input_value(io.ext_rst, 0u);

    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);
    }
    return static_cast<uint64_t>(sim.get_port_value(io.gpio_out)) == 0x5Au;
}

int run_verification() {
    Checker ck;

    auto dev = std::make_unique<ch::ch_device<riscv_soc::Rv32iSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ── A1：复位/初始状态两计数为 0 ──
    ck.check(sim.get_port_value(io.store_count) == 0, "A1 store_count 初始 == 0");
    ck.check(sim.get_port_value(io.writeback_count) == 0, "A1 writeback_count 初始 == 0");

    // ── 装载：装载期 ext_rst = 1（核保持复位 ⇒ 装载动作不产生写回/store）──
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    bool load_phase_clean = true;
    for (std::size_t i = 0; i < kProgWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, kProgram[i]);
        sim.tick(1);
        if (sim.get_port_value(io.store_count) != 0 ||
            sim.get_port_value(io.writeback_count) != 0) {
            load_phase_clean = false;
        }
    }
    for (std::size_t i = kProgWords; i < kFillWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, NOP);
        sim.tick(1);
        if (sim.get_port_value(io.store_count) != 0 ||
            sim.get_port_value(io.writeback_count) != 0) {
            load_phase_clean = false;
        }
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);

    // ── A2：装载期两计数保持 0（`ext_rst` 接反 ⇒ 装载被当指令执行 ⇒ 必红）──
    ck.check(load_phase_clean, "A2 装载期两计数保持 0");

    // ── 释放复位 ──
    sim.set_input_value(io.ext_rst, 0u);

    // ── 逐拍观测（A3–A6）──
    bool monotonic = true;
    uint64_t last_store = 0;
    uint64_t last_wb = 0;
    uint64_t tail_store = 0;
    uint64_t tail_wb = 0;
    bool tail_sampled = false;
    uint64_t final_store = 0;
    uint64_t final_wb = 0;

    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);
        const uint64_t sc = static_cast<uint64_t>(sim.get_port_value(io.store_count));
        const uint64_t wc = static_cast<uint64_t>(sim.get_port_value(io.writeback_count));
        // A5：单调不减
        if (sc < last_store || wc < last_wb) {
            monotonic = false;
        }
        last_store = sc;
        last_wb = wc;
        // A6：尾段采样
        if (!tail_sampled && c + 1 >= kTailStart) {
            tail_store = sc;
            tail_wb = wc;
            tail_sampled = true;
        }
        final_store = sc;
        final_wb = wc;
    }

    // ── A3 / A4：手推终值（**事件次数**口径）──
    ck.check(final_store == kExpectedStoreCount,
             "A3 store_count == 2（D-L1 一条 + MMIO 一条 · 手推）");
    ck.check(final_wb == kExpectedWritebackCount,
             "A4 writeback_count == 6（x1/x2/x3/x4/x6/x5 · 手推）");

    // ── A5：单调不减 ──
    ck.check(monotonic, "A5 两计数单调不减");

    // ── A6：尾段不再增长（jal 自环已稳定）──
    ck.check(tail_sampled && final_store == tail_store && final_wb == tail_wb,
             "A6 尾段计数不再增长");

    // ── A7：MMIO **写通路的值**（AC1）──
    // ⚠️ 只判「事务完成」不够：地址错段 / 数据错位都会让计数照样到终值
    //    ⇒ ⇒ 必须判 **GPIO 的 DATA_OUT 根端口**。
    ck.check(static_cast<uint64_t>(sim.get_port_value(io.gpio_out)) == kExpectedGpioOut,
             "A7 gpio_out == 0xA5（MMIO 写的值 · 手推）");

    // ── A8：**未映射地址 ⇒ DECERR（不挂死）**（AC8）──
    ck.check(dec_err_program_continues(),
             "A8 未映射 0x8010_0000 ⇒ DECERR 且程序继续（写回 == 3）");

    // ── A9：MMIO **读值**回路（critic BLOCKER 的靶子）──
    ck.check(read_data_path_probe(),
             "A9 MMIO 读值经「非 MMIO 后继」后仍 == 0x5A（读数据 mux 与 M2 同拍）");

    std::cout << "RESULT: checks=" << ck.checks() << " fails=" << ck.fails() << "\n";
    return ck.fails() == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 模式判定：位置无关剥离两个模式标记（库层共用实现）──
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        rest.emplace_back(argv[i]);
    }
    std::vector<std::string> mode_flags;
    rest = rtl_emit::strip_mode_flags(rest, {"--emit-rtl", "--check-products"}, &mode_flags);

    // ── **`--firmware <dir>`**（方案 a：ELF → init_data → 核取指）──
    // ⚠️ **位置无关** · ⚠️ **fail-closed**（缺取值 ⇒ exit 2）
    std::string fw_dir;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] != "--firmware") continue;
        if (i + 1 >= rest.size() || rest[i + 1].empty() || rest[i + 1][0] == '-') {
            std::cerr << "FATAL：--firmware 缺少取值（固件镜像目录）\n";
            return 2;
        }
        if (!fw_dir.empty()) {
            std::cerr << "FATAL：--firmware 只能给一次\n";
            return 2;
        }
        fw_dir = rest[i + 1];
        rest.erase(rest.begin() + static_cast<std::ptrdiff_t>(i),
                   rest.begin() + static_cast<std::ptrdiff_t>(i) + 2);
        --i;
    }

    const bool emit_mode =
        std::find(mode_flags.begin(), mode_flags.end(), "--emit-rtl") != mode_flags.end();
    const bool check_mode =
        std::find(mode_flags.begin(), mode_flags.end(), "--check-products") != mode_flags.end();

    // ── 发射 / 自检模式 ──
    if (emit_mode || check_mode) {
        std::string out_dir;
        std::string error;
        if (!rtl_emit::require_out_dir(rest, &out_dir, &error)) {
            std::cerr << "FATAL: --emit-rtl / --check-products " << error << "\n";
            return 2;
        }
        // ⚠️ 统一加 `--hierarchical`：本设计有 2 处 `ch_layer` ⇒ 产层次化产物。
        bool has_hier = false;
        for (const auto &a : rest) {
            if (a == "--hierarchical") has_hier = true;
        }
        if (!has_hier) rest.emplace_back("--hierarchical");

        // ⚠️ **`--firmware <dir>` 优先**：装载**从文件**来的真实 ELF 镜像
        auto make_design = [fw_dir](ch::core::context *) {
            const std::vector<uint32_t> itcm = read_region_image(fw_dir, "itcm");
            const std::vector<uint32_t> dtcm = read_region_image(fw_dir, "dtcm");
            return std::make_unique<riscv_soc::Rv32iSocTop>(nullptr,
                                                             riscv_soc::kSocRootName,
                                                             itcm, dtcm);
        };
        if (!fw_dir.empty()) {
            const int rc = rtl_emit::run_emit_cli(rest, make_design);
            if (rc != 0 || !check_mode) return rc;
            return rtl_emit::check_products(make_spec(out_dir), make_hooks(), make_design);
        }

        const int rc = rtl_emit::run_emit_cli(rest, make_emit_design);
        if (rc != 0 || !check_mode) return rc;
        return rtl_emit::check_products(make_spec(out_dir), make_hooks(), make_emit_design);
    }

    // ── **`--firmware <dir>` 的验证模式**（解释器路径）──
    if (!fw_dir.empty()) {
        return run_firmware_verification(fw_dir);
    }

    // ── 默认模式 fail-closed：收到任何未知参数 ⇒ exit 2 ──
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    return run_verification();
}
