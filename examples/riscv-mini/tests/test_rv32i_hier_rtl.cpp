// examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp
//
// **RV32I 的层次化 RTL 交付物**（诉求 ④）的产物级 + 等价性判据。
//
// 架构依据：
//   · **ADR-070 D1**（发射器判据修正 H-A / H-B）—— 本文件是它的**交付级**判据
//   · **ADR-070 D2**（成层粒度 = 实例级 `ch_layer`，7 处 · `rv32i_pipeline.h`）
//   · **ADR-060 §2.5**（lint 门口径：禁 `-Wall` · 禁 `-Wno-fatal` · 禁 `-Wno-<单项>`）
//   · improvement `rv32i-hierarchical-example-2026-11` 的 R21-C
//
// 判据（逐条对应 R21-C）：
//   ① **交付粒度**发射：`ok == true` 且 module 数 > 1（7 个 stage + `top`）
//   ② **确定性**：同一设计发射两次 ⇒ 逐字节相同
//   ③ **单连接不变式**：每个 pin 在实例语句里**恰好出现一次**
//      （H-B 漏做 ⇒ 分析层旧行与新行并存 ⇒ Verilator `Duplicate pin connection`）
//   ④ **顶层端口表 == 扁平产物**（逐字逐序 · ADR-060 §2.2 G3）
//   ⑤ **逐拍等价**（扁平 ≡ 层次化 · 同一段固件）—— 用公共 helper `ch::hier_eq`
//   ⑥ **手推期望值**（★ **禁抄仿真输出**）：见 `kExpectedWriteback` 的推导注释
//   ⑦ **判别性自证**：篡改层次化产物的一条 `assign` ⇒ 等价性**转红**
//
// ⚠️ **lint 门不在这里**：它属产物级护栏 ⇒ 载体 = `scripts/verify_rv32i_hier_rtl.sh`
//    （测试里 fork 外部 lint 会让失败原因与 TIMEOUT 归因混乱）。
// ⚠️ **本文件重量级**：层次化 + 扁平两份 Verilator 模型 ⇒ ⇒ `verilator_e2e` 标签 +
//    `verilator_backend` 资源锁 + `TIMEOUT`（见 `examples/riscv-mini/CMakeLists.txt`）。

#include <sys/stat.h>
#include <unistd.h>

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/manifest.h"
#include "rv32i_core_top.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;

namespace {

std::string make_temp_dir(const char *suffix) {
    char tmpl[] = "/tmp/cpphdl_hier_rtl_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) {
        return {};
    }
    std::string base(dir);
    std::string path = base + suffix;
    ::mkdir(path.c_str(), 0755);
    return path;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file(const std::string &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::size_t count_occurrences(const std::string &haystack, const std::string &needle) {
    std::size_t n = 0;
    for (auto at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + needle.size())) {
        ++n;
    }
    return n;
}

/// 某个 module 的**头部**（`module <name> (` … `);`）—— 用于「顶层端口表」比对
std::string module_header(const std::string &all, const std::string &name) {
    const std::string head = "module " + name + " (";
    const auto at = all.find(head);
    if (at == std::string::npos) {
        return {};
    }
    const auto end = all.find("\n);", at);
    if (end == std::string::npos) {
        return {};
    }
    return all.substr(at, end + 3 - at);
}

/// `module ` 的出现次数（= 产物里的 module 数）
std::size_t module_count(const std::string &all) {
    return count_occurrences(all, "\nmodule ");
}

// ─────────────────────────────────────────────────────────────────────────────
// **手推固件**（R21-C ⑥：期望值 MUST 手推 · 禁抄仿真输出）
//
// 指令语义（RV32I）：
//   word 0: `addi x1, x0, 10`  ⇒ x1 = 0 + 10      = **10**
//   word 1: `addi x2, x0, 3`   ⇒ x2 = 0 + 3       = **3**
//   word 2: `add  x3, x1, x2`  ⇒ x3 = 10 + 3      = **13**
//   word 3: `sw   x3, 0x100(x0)` ⇒ mem[0x100] = **13**
//   word 4: `lw   x4, 0x100(x0)` ⇒ x4 = mem[0x100] = **13**（只有 store 真生效才是 13）
//   word 5: `add  x5, x4, x1`  ⇒ x5 = 13 + 10     = **23**（只有 load 真读到 13 才是 23）
//
// ⇒ ⇒ 写回序列（按 rd）= {(1,10), (2,3), (3,13), (4,13), (5,23)}
// ⇒ ⇒ **判别性**：x5 = 23 同时要求 ALU、store、load、寄存器堆**全部**正确
//      ⇒ ⇒ 恒 NOP 的流水线 / 未装载的 imem 都**不可能**产出它。
// ─────────────────────────────────────────────────────────────────────────────
constexpr std::uint32_t kNop = 0x00000013u;

const std::vector<std::uint32_t> &hand_firmware() {
    static const std::vector<std::uint32_t> fw = {
        0x00A00093u,  // addi x1, x0, 10
        0x00300113u,  // addi x2, x0, 3
        0x002081B3u,  // add  x3, x1, x2
        0x10302023u,  // sw   x3, 0x100(x0)
        0x10002203u,  // lw   x4, 0x100(x0)   ⇒ rd = bits[11:7] = 4
        0x001202B3u,  // add  x5, x4, x1
    };
    return fw;
}

/// 手推的写回序列（`(rd, value)` · 按程序序）
const std::vector<std::pair<std::uint32_t, std::uint32_t>> &hand_writebacks() {
    static const std::vector<std::pair<std::uint32_t, std::uint32_t>> wb = {
        {1u, 10u}, {2u, 3u}, {3u, 13u}, {4u, 13u}, {5u, 23u}};
    return wb;
}

/// 装载期的**尾部填充**：把 imem 填满 NOP ⇒ ⇒ 程序跑出固件后不会撞上未初始化字。
constexpr std::size_t kTailFillWords = 64;
/// 装载完成后再跑的拍数（★ 手推覆盖：6 条指令 + 流水线 5 级 ⇒ 20 拍足够，
/// 这里给 80 拍以便尾部 NOP 也被走一遍）
constexpr unsigned kRunCycles = 80;

}  // namespace

TEST_CASE("rv32i hierarchical rtl: deliverable product shape (granularity, determinism, ports)",
          "[rv32i][hierarchy][emit][product]") {
    const std::string dir = make_temp_dir("_shape");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<Rv32iCoreTop> dev;

    // ═══ ① 交付粒度发射（★ 不是 ForceLayer：那是探针/逃生门 ⇒ ADR-070 D4 条件②）═══
    const std::string p1 = dir + "/first.v";
    auto r1 = ch::toVerilogHierarchical(p1, dev.instance());
    INFO("emit error: " << r1.error);
    REQUIRE(r1.ok);
    REQUIRE(r1.files.size() == 1);

    const std::string t1 = read_file(p1);
    REQUIRE_FALSE(t1.empty());
    // 7 个 stage module + `top` ⇒ ⇒ 8（ADR-070 D2 的实例级 `ch_layer`）
    REQUIRE(module_count(t1) == 8);
    for (const char *m : {"IfStage", "IdStage", "ExStage", "MemStage", "WbStage", "HazardUnit"}) {
        INFO("missing module " << m);
        REQUIRE(t1.find(std::string("module ") + m + " (") != std::string::npos);
    }
    // `ForwardingMux<32>` 经 `sanitize_verilog_name` ⇒ `ForwardingMux_32_`
    REQUIRE(t1.find("module ForwardingMux_32_ (") != std::string::npos);
    // `load_lane` 按 ADR-070 的字面集合**不**成层（inline 进 top）
    REQUIRE(t1.find("module LoadLaneSelector") == std::string::npos);

    // ═══ ② 确定性（= R4 的最小版）：两次发射逐字节相同 ═══
    const std::string p2 = dir + "/second.v";
    auto r2 = ch::toVerilogHierarchical(p2, dev.instance());
    REQUIRE(r2.ok);
    REQUIRE(read_file(p2) == t1);

    // ═══ ③ 单连接不变式：每个 pin 在实例语句里恰好一次 ═══
    //    （H-B 漏做 ⇒ 分析层旧行 + 合成新行并存 ⇒ Verilator `Duplicate pin connection`）
    {
        std::size_t instances = 0;
        std::vector<std::string> dups;
        // 实例语句 = `<ModuleType> <inst> (\n  .pin(net),\n … );`
        // ⚠️ `module top (` 的**端口列表**同形 ⇒ 必须按 `m[1] != "module"` 排除
        const std::regex inst_re(R"((\w+)\s+(\w+)\s*\(\n((?:\s*\.\w+\([^()]*\),?\n)+)\s*\);)");
        const std::regex pin_re(R"(\.(\w+)\()");
        auto begin = t1.cbegin();
        std::smatch m;
        while (std::regex_search(begin, t1.cend(), m, inst_re)) {
            if (m[1].str() != "module") {
                ++instances;
                std::map<std::string, int> pins;
                const std::string body = m[3].str();
                for (std::sregex_iterator it(body.begin(), body.end(), pin_re), end; it != end;
                     ++it) {
                    ++pins[(*it)[1].str()];
                }
                for (const auto &kv : pins) {
                    if (kv.second != 1) {
                        dups.push_back("instance '" + m[2].str() + "': pin '" + kv.first +
                                       "' appears " + std::to_string(kv.second) + " times");
                    }
                }
            }
            begin = m[0].second;
        }
        INFO("duplicate pins: " << (dups.empty() ? "none" : dups.front()));
        REQUIRE(dups.empty());
        REQUIRE(instances == 7);   // 7 个 stage 实例都在 `top` 内
    }

    // ═══ ④ 顶层端口表 == 扁平端口表中「**根 module 的端口**」那一段（逐字逐序）═══
    //    ⚠️ **口径订正（实测 · 2026-11-15）**：ADR-060 §2.2 G3 说的「逐字逐序一致」约束的是
    //    **根端口**那一段 —— 扁平产物把**成层子组件的 `ch_out`** 也提成顶层端口
    //    （= 台账 id=23「扁平产物顶层暴露内部 `ch_out`」），层次化侧**不在**顶层
    //    （= `hierarchy_equivalence.h` 明写的「观测盲区」）
    //    ⇒ ⇒ 正确的可判定形态 = **层次化端口行 == 扁平端口行的前缀**。
    const std::string flat_path = dir + "/flat.v";
    ch::toVerilog(flat_path, dev.context());   // ⚠️ 扁平入口吃 **context***
    const std::string flat = read_file(flat_path);
    REQUIRE_FALSE(flat.empty());
    const std::string flat_header = module_header(flat, "top");
    const std::string hier_header = module_header(t1, "top");
    REQUIRE_FALSE(flat_header.empty());
    REQUIRE_FALSE(hier_header.empty());
    {
        auto header_port_lines = [](const std::string &header) {
            std::vector<std::string> out;
            std::istringstream ss(header);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("    ", 0) == 0) {   // 端口条目的缩进
                    out.push_back(line);
                }
            }
            return out;
        };
        const auto hier_ports = header_port_lines(hier_header);
        const auto flat_ports = header_port_lines(flat_header);
        REQUIRE_FALSE(hier_ports.empty());

        // ⚠️ **实测口径订阅正（2026-11-15）**：ADR-060 §2.2 G3 的依据句是「顶层端口表 MUST 与
        //    扁平产物**逐字逐序**一致」⇒ ⇒ 实测在「**部分成层** + inline 子组件带 `ch_out`」时
        //    只成立一半：
        //      · **逐字**：层次化侧每个端口条目都在扁平侧**逐字**存在（集合 ⊆）✓
        //      · **逐序**：只在**根 `io()` 端口段**成立 —— 其后「inline 子组件的 `ch_out`」
        //        与「成层子组件的 `ch_out`」的相对序两侧不同（扁平按 eval 序，层次化按
        //        owner 序）⇒ ⇒ 该差异**已登记**（见 change 的 design.md §「实测订正」）
        //    ⇒ ⇒ 判据按可判定的形态写：**根端口段逐字逐序** + **其余逐字（集合 ⊆）**。
        // ⚠️⚠️ **段归属 MUST 以 `PortProvenanceOrigin` 为准**（ADR-062 v1.7 · oracle F3）
        //    ⇒ ⇒ **MUST NOT** 以**名称前缀**推断（原实现用 `top_pipeline_` / `top_itcm_` /
        //    `top_dtcm_` 黑名单 ⇒ ⇒ 实例改名即**静默错分类** ⇒ fail-safe 转红但诊断误导）。
        //    ⇒ ⇒ 机械切法（实现自带的数据模型）：根 module 的 `Self` / `ClockReset` /
        //    `Bubbled` 三类 = **根段**；`InlineFolded` = ⊆ 组。
        // ⚠️ **ADR-072 D1-f** 之后：顶层端口**只有** `Self` / `ClockReset` 两类
        //    （「inline 折叠」与「冒泡」两类已退役）
        std::set<std::string> root_names;
        for (const auto &pp : r1.port_provenance) {
            if (pp.module != "top") continue;   // ★ 根 module（产物里的顶层）
            root_names.insert(pp.port);
        }
        REQUIRE_FALSE(root_names.empty());
        const auto port_name_of = [](const std::string &line) {
            std::string t = line;
            while (!t.empty() && (t.back() == ',' || std::isspace(
                     static_cast<unsigned char>(t.back())))) {
                t.pop_back();
            }
            const auto pos = t.find_last_of(" \t");
            return pos == std::string::npos ? t : t.substr(pos + 1);
        };
        const auto root_segment = [&](const std::vector<std::string> &lines) {
            std::vector<std::string> out;
            for (const auto &l : lines) {
                if (root_names.count(port_name_of(l))) {
                    // ⚠️ 归一化行尾逗号：两侧的**最后一个**端口分别带/不带逗号
                    //    （扁平侧后面还有别的端口）⇒ ⇒ 不归一化会把「同一端口」判成不同
                    std::string t = l;
                    while (!t.empty() &&
                           (t.back() == ',' ||
                            std::isspace(static_cast<unsigned char>(t.back())))) {
                        t.pop_back();
                    }
                    out.push_back(t);
                }
            }
            return out;
        };
        const auto hier_root = root_segment(hier_ports);
        const auto flat_root = root_segment(flat_ports);
        REQUIRE_FALSE(hier_root.empty());
        // ① **根端口段**：**逐字逐序**（clock → reset → 无 driver input → 根组件自身的 `ch_out`）
        //    ⚠️ 两侧**都**按 `root_names` 过滤 ⇒ ⇒ 比的是「同一组端口的**内容 + 顺序**」
        //    ⇒ ⇒ 不再依赖「根段是前缀」这个**结构性假设**。
        REQUIRE(hier_root == flat_root);
        // ② **其余**：逐字存在（集合 ⊆ · 扁平侧多出的 = 成层子组件的 `ch_out` ⇒ 观测盲区）
        const std::set<std::string> flat_set(flat_ports.begin(), flat_ports.end());
        for (std::size_t i = 0; i < hier_ports.size(); ++i) {
            if (root_names.count(port_name_of(hier_ports[i]))) continue;   // 根段已逐字逐序比过
            INFO("port #" << i << " = " << hier_ports[i]);
            REQUIRE(flat_set.count(hier_ports[i]) == 1);
        }
        // ③ 盲区**确实存在**（否则「子 ch_out 在层次化顶层不可观测」这条前提是空的）
        REQUIRE(flat_ports.size() > hier_ports.size());
        // ④ **收敛恒等式（ADR-072 D1-f）**：层次化顶层端口表 MUST **逐字逐序**等于
        //    manifest 的**产品端口表**（同一根组件 / 同一 ctx ⇒ 同一份定义）
        //    ⚠️ 旧的「层次化顶层 MUST 含**全部** inline 后代 `ch_out`」半句已**删除**
        //       （ADR-072 §2.8 R-5 改法③）⇒ ⇒ 后代 `ch_out` 一律不上顶层。
        const auto manifest =
            ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
        REQUIRE_FALSE(manifest.port_table.empty());
        REQUIRE(hier_ports.size() == manifest.port_table.size());
        for (std::size_t i = 0; i < hier_ports.size(); ++i) {
            INFO("port #" << i << " = " << hier_ports[i]);
            REQUIRE(port_name_of(hier_ports[i]) == manifest.port_table[i].name);
        }
    }

    // ═══ ④' **lint 门**（ADR-060 §2.5 / R21-C ②）═══
    //    `verilator --lint-only --top-module top <file>` · **禁** `-Wall` · **禁** `-Wno-fatal`
    //    · **禁** 任何 `-Wno-<单项>`（那是旁路）· 文件名 MUST == 顶层 module 名
    //    ⚠️ 实测（2026-11-15）：**零 `%Warning`** ⇒ ⇒ 本判据断言**零** Warning（不留白名单）
    {
        const std::string lint_dir = dir + "/lint";
        ::mkdir(lint_dir.c_str(), 0755);
        const std::string lint_v = lint_dir + "/top.v";
        write_file(lint_v, t1);
        const std::string cmd = "cd " + lint_dir +
                                " && verilator --lint-only --top-module top top.v > lint.log 2>&1";
        const int rc = std::system(cmd.c_str());
        const std::string lint_log = read_file(lint_dir + "/lint.log");
        INFO("lint log: " << lint_log.substr(0, 2000));
        REQUIRE(rc == 0);
        REQUIRE(lint_log.find("%Error") == std::string::npos);
        // ⚠️ 零 Warning 是**实测事实**（不是「允许清单为空」的默认值）⇒ ⇒ 一旦出现
        //    warning，要么修产物，要么在此处显式列出并给理由（MUST NOT 加 `-Wno-*`）
        REQUIRE(lint_log.find("%Warning") == std::string::npos);
        // 覆盖反证：lint 真的跑了（否则空 log 也能过）
        REQUIRE(lint_log.find("Verilator") != std::string::npos);
    }

    // ═══ ⑤ `rewired_pins` 是**结构化字段**（不是 CHWARN ⇒ 不污染 `warnings` 语义）═══
    //    ⚠️ 该路径对**每个**「子 output 被父体消费」的 pin 都会发生 ⇒ ⇒ 用 CHWARN 会让
    //    所有层次化发射常态告警。
    REQUIRE_FALSE(r1.rewired_pins.empty());
    for (const auto &pin : r1.rewired_pins) {
        INFO("rewired pin: " << pin);
        REQUIRE(pin.find(" -> ") != std::string::npos);
    }

    // ═══ ⑥ 多消费者 ≥ 3（幂等路径的覆盖）═══
    //    同一个子输出的合成网被**至少 3 条**边复用 ⇒ 走的是 `add_conn` / `register_parent_wire`
    //    的幂等分支（`hazard_unit` 的输出同时喂 `if_stage` / `id_stage` / 父体表达式）
    {
        std::map<std::string, int> fanout;
        for (const auto &w : r1.warnings) {
            const auto at = w.find("from ");
            if (at == std::string::npos) {
                continue;
            }
            const auto end = w.find(" (", at);
            if (end == std::string::npos) {
                continue;
            }
            ++fanout[w.substr(at + 5, end - at - 5)];
        }
        int best = 0;
        std::string best_name;
        for (const auto &kv : fanout) {
            if (kv.second > best) {
                best = kv.second;
                best_name = kv.first;
            }
        }
        INFO("max fan-out of one sub output = " << best << " (" << best_name << ")");
        REQUIRE(best >= 3);
    }

    // ═══ ⑦ proxy 端点（`reg_file.rs1/rs2` 作为父侧消费点）═══
    //    ⚠️ proxy 的**本地网名**带 `_data_proxy` 后缀（`print_proxy` 的 RHS 解析对象）
    //    ⇒ ⇒ 「proxy 端点实测成立」的可判形态 = **产物里这两个 proxy 网都在**
    //    （若 `print_proxy` 的 RHS 引用未声明信号 ⇒ 静态检查门会红 ⇒ 由 ④' 覆盖）
    REQUIRE(t1.find("top_pipeline_reg_file_rs1_data_proxy") != std::string::npos);
    REQUIRE(t1.find("top_pipeline_reg_file_rs2_data_proxy") != std::string::npos);
}

TEST_CASE("rv32i hierarchical rtl: cycle-accurate equivalence with the flat product",
          "[rv32i][hierarchy][equivalence][verilator]") {
    const std::string dir = make_temp_dir("_eq");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<Rv32iCoreTop> dev;
    auto &io = dev.instance().io();

    auto ps = ch::hier_eq::emit(dev.instance(), dir);   // ★ 默认 = RespectDeclarations
    INFO("emit error: " << ps.error);
    REQUIRE(ps.ok);
    REQUIRE_FALSE(ps.ports.empty());

    // ── 受驱动的顶层输入（⚠️ `DrivenInput` MUST 用 `set_input_value(port, 纯整数)`）──
    std::vector<ch::hier_eq::DrivenInput> inputs;
    auto add_in = [&](const std::string &n, auto &port) {
        inputs.push_back({n, [&port](ch::Simulator &s, std::uint64_t v) {
                              s.set_input_value(port, v);
                          }});
    };
    add_in("ext_rst", io.ext_rst);
    add_in("prog_we", io.prog_we);
    add_in("prog_addr", io.prog_addr);
    add_in("prog_data", io.prog_data);

    // ── 观测面（手写 · 全部是**根** `ch_out` ⇒ 两侧顶层都可见）──
    std::vector<ch::hier_eq::Observation> obs;
    auto add_obs = [&](const std::string &n, auto &port) {
        obs.push_back(ch::hier_eq::make_observation(n, port, [&port](ch::Simulator &s) {
            return static_cast<std::uint64_t>(s.get_port_value(port));
        }));
    };
    add_obs("dbg_id_pc", io.dbg_id_pc);
    add_obs("dbg_id_valid", io.dbg_id_valid);
    add_obs("dbg_id_instr", io.dbg_id_instr);
    add_obs("dbg_wb_write_en", io.dbg_wb_write_en);
    add_obs("dbg_wb_rd_addr", io.dbg_wb_rd_addr);
    add_obs("dbg_wb_write_data", io.dbg_wb_write_data);
    add_obs("obs_data_addr", io.obs_data_addr);
    add_obs("obs_data_write_en", io.obs_data_write_en);
    add_obs("obs_perf_instructions", io.obs_perf_instructions);

    // ── 激励 = 装载（`ext_rst = 1` 期间写 imem）+ 跑动 ──
    const auto &fw = hand_firmware();
    std::vector<std::vector<std::uint64_t>> stim;
    for (std::size_t i = 0; i < fw.size(); ++i) {
        stim.push_back({1u, 1u, i, fw[i]});
    }
    for (std::size_t i = fw.size(); i < fw.size() + kTailFillWords; ++i) {
        stim.push_back({1u, 1u, i, kNop});
    }
    for (unsigned i = 0; i < kRunCycles; ++i) {
        stim.push_back({0u, 0u, 0u, 0u});
    }

    auto res = ch::hier_eq::compare(dev.context(), dir, ps.hier_path, ps.ports, inputs, obs, stim);
    INFO("compare error: " << res.error);
    REQUIRE(res.ok);
    INFO("first mismatch: " << res.first_mismatch);
    REQUIRE(res.equal);
    REQUIRE(res.mismatch_count == 0);
    REQUIRE(res.flat_trace.size() == stim.size());

    // ═══ ⑥ 手推期望值（★ **不抄** `flat_trace`：下面的常量来自 `hand_writebacks()`）═══
    //    列序 = `obs` 的声明序
    constexpr std::size_t kPc = 0, kValid = 1, kInstr = 2, kWbEn = 3, kWbRd = 4, kWbData = 5;
    constexpr std::size_t kDataAddr = 6, kDataWe = 7, kInstrCount = 8;

    // (a) **固件真的被装载**：ID 级按程序序吐出这 6 个字
    {
        std::vector<std::uint32_t> seen;
        for (const auto &row : res.flat_trace) {
            const auto instr = static_cast<std::uint32_t>(row[kInstr]);
            if (!seen.empty() && seen.back() == instr) {
                continue;   // 停顿拍：同一条指令保持多拍
            }
            if (instr != 0u) {
                seen.push_back(instr);
            }
            if (seen.size() > 8) {
                break;
            }
        }
        REQUIRE(seen.size() >= fw.size());
        for (std::size_t i = 0; i < fw.size(); ++i) {
            INFO("instruction #" << i);
            REQUIRE(seen[i] == fw[i]);
        }
    }

    // (b) **写回序列 == 手推**（rd 与值逐项）
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> got;
        for (const auto &row : res.flat_trace) {
            if (row[kWbEn] == 0u) {
                continue;
            }
            const auto rd = static_cast<std::uint32_t>(row[kWbRd]);
            const auto val = static_cast<std::uint32_t>(row[kWbData]);
            if (!got.empty() && got.back() == std::make_pair(rd, val)) {
                continue;   // 同一拍保持
            }
            got.emplace_back(rd, val);
        }
        // 前 5 项 MUST 就是手推序列（其后的 NOP 不写回 ⇒ 见下一条）
        REQUIRE(got.size() >= hand_writebacks().size());
        for (std::size_t i = 0; i < hand_writebacks().size(); ++i) {
            INFO("writeback #" << i);
            REQUIRE(got[i] == hand_writebacks()[i]);
        }
        // `x5 = 23` 是「ALU + store + load + 寄存器堆全对」才成立的值
        REQUIRE(std::find(got.begin(), got.end(), std::make_pair(5u, 23u)) != got.end());
    }

    // (c) **store 生效**：`obs_data_write_en = 1` 的拍里地址 == 0x100（手推：`sw x3, 0x100`）
    {
        bool saw_store = false;
        for (const auto &row : res.flat_trace) {
            if (row[kDataWe] == 0u) {
                continue;
            }
            saw_store = true;
            REQUIRE(static_cast<std::uint32_t>(row[kDataAddr]) == 0x100u);
        }
        REQUIRE(saw_store);
    }

    // (d) 退休计数单调递增且 ≥ 手推的 6 条
    {
        REQUIRE(!res.flat_trace.empty());
        REQUIRE(res.flat_trace.back()[kInstrCount] >= fw.size());
    }
}

TEST_CASE("rv32i hierarchical rtl: corrupting one assign turns the equivalence red",
          "[rv32i][hierarchy][equivalence][mutation]") {
    // ⑦ **判别性自证**：篡改层次化产物的**一条** `assign`（改写成常量 0）
    //    ⇒ ⇒ 逐拍等价 MUST 转红 ⇒ ⇒ 否则「全绿」可能来自观测面恒定量。
    const std::string dir = make_temp_dir("_mut");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<Rv32iCoreTop> dev;
    auto &io = dev.instance().io();

    auto ps = ch::hier_eq::emit(dev.instance(), dir);
    REQUIRE(ps.ok);

    // 篡改对象 = `top` 内**驱动被观测信号**的那条 `assign`（RHS ⇒ `'0`）。
    //   ⚠️ **为什么选被观测信号的驱动线**（而不是随便一条内部线）：
    //      本判据的目的是证明「`compare` **真的**在比较这份产物」（否则全绿可能来自
    //      空的 / 恒定的观测面）⇒ ⇒ 改**内部**线时无法保证它在 198 拍内影响到观测
    //      （实测两次都落进「改了对行为无影响」的线 ⇒ 判据**假绿**）
    //      ⇒ ⇒ 而「**内部通路**正确」由**等价性用例**（198 拍逐拍全等）+
    //      **手推写回序列**（`x5 = 23`）承担 ⇒ ⇒ 三者互补，互不替代。
    //   · MUST 是**单行**  · MUST 恰好出现**一次**
    const std::string original = read_file(ps.hier_path);
    std::string target_stmt;
    for (const char *sig : {"assign top_dbg_id_pc = ", "assign top_dbg_id_instr = ",
                            "assign top_dbg_wb_write_data = "}) {
        const auto at = original.find(sig);
        if (at == std::string::npos) {
            continue;
        }
        const auto bol = original.rfind('\n', at);
        const auto eol = original.find(";\n", at);
        if (bol == std::string::npos || eol == std::string::npos) {
            continue;
        }
        const std::string cand = original.substr(bol + 1, eol + 1 - (bol + 1));
        if (cand.find('\n') != std::string::npos) {
            continue;   // 多行语句 ⇒ 替换会造出语法错误
        }
        target_stmt = cand;
        break;
    }
    INFO("corrupted statement: " << target_stmt);
    REQUIRE_FALSE(target_stmt.empty());
    REQUIRE(count_occurrences(original, target_stmt) == 1);

    std::string corrupted = original;
    {
        const auto eq = target_stmt.find('=');
        REQUIRE(eq != std::string::npos);
        const std::string lhs = target_stmt.substr(0, eq);   // `    assign <name> `
        corrupted.replace(corrupted.find(target_stmt), target_stmt.size(), lhs + "= '0;");
    }
    const std::string corrupt_path = dir + "/corrupted.v";
    write_file(corrupt_path, corrupted);
    REQUIRE(read_file(corrupt_path) != original);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    auto add_in = [&](const std::string &n, auto &port) {
        inputs.push_back({n, [&port](ch::Simulator &s, std::uint64_t v) {
                              s.set_input_value(port, v);
                          }});
    };
    add_in("ext_rst", io.ext_rst);
    add_in("prog_we", io.prog_we);
    add_in("prog_addr", io.prog_addr);
    add_in("prog_data", io.prog_data);

    std::vector<ch::hier_eq::Observation> obs;
    auto add_obs = [&](const std::string &n, auto &port) {
        obs.push_back(ch::hier_eq::make_observation(n, port, [&port](ch::Simulator &s) {
            return static_cast<std::uint64_t>(s.get_port_value(port));
        }));
    };
    add_obs("dbg_id_pc", io.dbg_id_pc);
    add_obs("dbg_id_instr", io.dbg_id_instr);
    add_obs("dbg_wb_write_data", io.dbg_wb_write_data);
    add_obs("obs_data_addr", io.obs_data_addr);

    const auto &fw = hand_firmware();
    std::vector<std::vector<std::uint64_t>> stim;
    for (std::size_t i = 0; i < fw.size(); ++i) {
        stim.push_back({1u, 1u, i, fw[i]});
    }
    for (unsigned i = 0; i < kRunCycles; ++i) {
        stim.push_back({0u, 0u, 0u, 0u});
    }

    auto res =
        ch::hier_eq::compare(dev.context(), dir, corrupt_path, ps.ports, inputs, obs, stim);
    INFO("compare error: " << res.error);
    // 篡改后 MUST 跑起来（否则测的是「编译失败」而不是「行为分叉」）
    REQUIRE(res.ok);
    REQUIRE_FALSE(res.equal);
    REQUIRE(res.mismatch_count > 0);
}
