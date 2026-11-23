// tools/rv32i-fw-dump/main.cpp
//
// **RV32I 固件转储器 / ELF 装载器**（`firmware-load-chain` **L4 + L5**）
//
// 用法：
//     rv32i-fw-dump --in <fw.elf|fw.json> --format vh|mem|manifest --out <path>
//
// ## 输出格式
//
// | `--format` | 产物 | 消费者 |
// |---|---|---|
// | `mem`      | **字镜像**（每行一个 32 位字 · 十六进制） | `$readmemh` · **仅单区域固件**（多区域 ⇒ ⇒ **拒绝**，见下） |
// | `vh`       | SystemVerilog `initial` 块（`$readmemh`） | 独立 Verilog TB（**自带装载** ⇒ TB 不再手写固件）· **仅单区域固件** |
// | `manifest` | JSON：**基址** + **入口** + 「**字索引 → 字节地址**」映射 | 装载器（L2 的统一加载口）⇒ ⇒ **本工具的产物与 L2 的端口契约解耦** |
// | `mem-split`| **按区域拆分**的两个字镜像：`<dir>/itcm.mem` + `<dir>/dtcm.mem` | `ch_mem::init_data` 的初值镜像（**方案 a**：ELF → init_data ⇒ ⇒ 核取指） |
//
// ⚠️ **`mem-split` 的行号 = 区域内的字索引** ⇒ ⇒ 空洞 MUST **补零**
//    （否则索引与地址错位 ⇒ ⇒ 固件被装到**错误**的位置）。
//    ⇒ ⇒ 字节地址 → 区域字索引的换算**只在本文件一处**完成：
//    `(byte_addr - region.base) >> 2`。
//
// ⚠️ **`mem` / `vh` 遇多区域固件 ⇒ ⇒ exit 3**（**不**静默拼接）
//    ⇒ ⇒ 理由：单条无地址的字流无法表达「哪一段属于哪个区域」
//    ⇒ ⇒ 消费者 MUST NOT 靠猜（capability `rv32i-firmware-loading` 的
//    「转储器 MUST 产出**可寻址**产物」条款）⇒ ⇒ 多区域 MUST 用 `mem-split`。
//
// ## ⚠️ 纪律（沿用 `rtl-emit-product-tooling` **C1**）
//
// · 本二进制 **MUST NOT 链 `examples/**`** ⇒ ⇒ 只链 `cpphdl` 库 ⇒ ⇒
//   `examples/` 改了不会影响它 ⇒ ⇒ 它可以当**自测载体**。
// · 本工具 **只**链**库**（`rv32i_mem_map.h` ⇒ ⇒ 地址映射**单一真相**）。
//
// ## ELF 装载（**L5**）
//
// · 解析 **ELF32** header + `PT_LOAD` program header ⇒ ⇒ 取 `p_paddr` / `p_filesz` / `p_memsz`。
// · **`.bss`**（`p_memsz > p_filesz`）⇒ ⇒ **补零**（旧内容不留）。
// · ⚠️ **非零重定位 ⇒ ⇒ 报错**（**不是**静默忽略）⇒ ⇒ 判据 = **section header 里有
//   非空的 `SHT_RELA` / `SHT_REL`**（**不**解析符号表 ⇒ ⇒ 无需符号表即可判）。
// · ⚠️ **入口 MUST 等于 `kPcReset`**（= I-TCM 的 base）⇒ ⇒ 否则装出来的固件
//   **无法启动**（SoC 的 PC 复位值硬连线为 `kPcReset`）⇒ ⇒ **MUST 报错**。
// · ⚠️ **越界 ⇒ ⇒ 报错**（**禁**静默丢高位 ⇒ ⇒ 截断会让固件装到**错误**的地方）。

#include "cpu/riscv/rv32i_mem_map.h"
#include "cpu/riscv/rv32i_asm_encode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using riscv::kDtcm;
using riscv::kItcm;

constexpr int kExitOk = 0;
constexpr int kExitUsage = 2;   // 用法错误 / 输入不可读
constexpr int kExitData = 3;   // 输入数据不合法（越界 / 重定位 / 非法 ELF）

/// 固件的一个字（**字节**地址 + 32 位数据）
struct Word {
    uint64_t addr;   ///< **字节**地址
    uint32_t data;   ///< 字数据
};

struct Firmware {
    uint64_t entry = 0;              ///< 入口**字节**地址
    std::vector<Word> words;          ///< 全部字（按地址升序）
    std::vector<riscv::MemRegion> regions;  ///< 覆盖到的区域
};

/// 区域**是否已登记**（按 base 判重 ⇒ ⇒ 同一区域不被登记两次）
bool has_region(const std::vector<riscv::MemRegion> &regions, uint64_t base) {
    for (const auto &r : regions) {
        if (r.base == base) return true;
    }
    return false;
}

/// 固件**跨越了几个**受管区域（1 = 单区域 · 2 = I-TCM + D-TCM）
///
/// ⚠️ 判据取自**字**（不是 `fw.regions`）⇒ ⇒ 与 manifest 的 `regions[]` 登记
///    **解耦** ⇒ ⇒ 两者任一坏掉都不会让本判据失真。
std::size_t region_count(const Firmware &fw) {
    bool itcm = false;
    bool dtcm = false;
    for (const auto &w : fw.words) {
        if (riscv::in_region(kItcm, w.addr)) {
            itcm = true;
        } else if (riscv::in_region(kDtcm, w.addr)) {
            dtcm = true;
        }
    }
    return (itcm ? 1u : 0u) + (dtcm ? 1u : 0u);
}

void fail(const std::string &msg, int code) {
    std::cerr << "::error::" << msg << "\n";
    std::exit(code);
}

uint16_t rd16(const std::vector<uint8_t> &b, size_t off) {
    return static_cast<uint16_t>(b[off] | (b[off + 1] << 8));
}
uint32_t rd32(const std::vector<uint8_t> &b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

std::vector<uint8_t> read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fail("无法读取输入文件：" + path, kExitUsage);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

// ---------------------------------------------------------------------------
// ELF32 装载（**L5**）
// ---------------------------------------------------------------------------
Firmware load_elf(const std::vector<uint8_t> &b) {
    // ELF32 little-endian header（52 字节）
    constexpr size_t kEhdrSize = 52;
    constexpr size_t kPhdrSize = 32;
    if (b.size() < kEhdrSize) fail("ELF 文件太短（< 52 字节）", kExitData);
    if (!(b[0] == 0x7Fu && b[1] == 'E' && b[2] == 'L' && b[3] == 'F')) {
        fail("不是 ELF 文件（魔数不匹配）", kExitData);
    }
    if (b[4] != 1u) fail("只支持 ELF32（EI_CLASS != 1）", kExitData);
    if (b[5] != 1u) fail("只支持小端（EI_DATA != 1）", kExitData);
    const uint16_t e_type = rd16(b, 16);
    const uint16_t e_machine = rd16(b, 18);
    if (e_machine != 243u /* EM_RISCV */) {
        fail("不是 RISC-V ELF（e_machine != 243）", kExitData);
    }
    if (e_type != 2u /* ET_EXEC */) {
        fail("只支持 ET_EXEC（e_type != 2）", kExitData);
    }
    const uint32_t e_entry = rd32(b, 24);
    const uint32_t e_phoff = rd32(b, 28);
    const uint16_t e_phentsize = rd16(b, 42);
    const uint16_t e_phnum = rd16(b, 44);
    const uint32_t e_shoff = rd32(b, 32);
    const uint16_t e_shentsize = rd16(b, 46);
    const uint16_t e_shnum = rd16(b, 48);

    // ⚠️ **重定位检查**（**不**解析符号表 ⇒ ⇒ 无需符号表即可判）
    //
    // 判据 = **section header 里有非空的 `SHT_RELA`(4) / `SHT_REL`(9)**。
    // ⇒ ⇒ ELF32 的 `.rela` 在 **section** 里 ⇒ ⇒ program header **看不到**
    //    ⇒ ⇒ 本检查**必须**读 section header 才成立（此前是空循环 ⇒ 零判别力）。
    // ⇒ ⇒ `e_shnum == 0`（节头被 strip）⇒ ⇒ **无从查起** ⇒ ⇒ 接受（已知边界）。
    // ⇒ ⇒ 正常 `ld` 产出的静态 RV32I ELF **不含**任何 REL/RELA 节
    //    （链接器已把重定位固化进 `p_paddr`）⇒ ⇒ 无误报。
    if (e_shnum > 0) {
        constexpr size_t kShdrSize = 40;
        if (e_shentsize < kShdrSize) {
            fail("ELF section header 项大小非法（e_shentsize < 40）", kExitData);
        }
        for (uint16_t s = 0; s < e_shnum; ++s) {
            const size_t soff = e_shoff + static_cast<size_t>(s) * e_shentsize;
            if (soff + kShdrSize > b.size()) fail("section header 越界", kExitData);
            const uint32_t sh_type = rd32(b, soff + 4);
            const uint32_t sh_size = rd32(b, soff + 20);
            const bool is_reloc = (sh_type == 4u /* SHT_RELA */ ||
                                   sh_type == 9u /* SHT_REL */);
            if (is_reloc && sh_size > 0) {
                std::ostringstream os;
                os << "ELF 含非空的重定位表（section #" << s << " · sh_type="
                   << sh_type << " · sh_size=" << sh_size << "）⇒ ⇒ 拒绝装载"
                   << "（本工具**不做**重定位 ⇒ ⇒ 静默忽略会产出"
                      "「看起来装好了、实际地址全错」的固件）";
                fail(os.str(), kExitData);
            }
        }
    }

    // ⚠️ **入口 MUST 等于 `kPcReset`**（= I-TCM 的 base）⇒ ⇒ 校验在 `validate()`
    //    （ELF 与 JSON 两条装载路径**共用**同一处判据 ⇒ ⇒ 禁两处各写一份）。

    Firmware fw;
    fw.entry = e_entry;

    for (uint16_t i = 0; i < e_phnum; ++i) {
        const size_t off = e_phoff + static_cast<size_t>(i) * e_phentsize;
        if (off + kPhdrSize > b.size()) fail("program header 越界", kExitData);
        const uint32_t p_type = rd32(b, off + 0);
        if (p_type != 1u /* PT_LOAD */) continue;
        const uint32_t p_offset = rd32(b, off + 4);
        const uint32_t p_paddr = rd32(b, off + 12);
        const uint32_t p_filesz = rd32(b, off + 16);
        const uint32_t p_memsz = rd32(b, off + 20);
        if (p_memsz < p_filesz) fail("p_memsz < p_filesz（段声明非法）", kExitData);
        if (static_cast<size_t>(p_offset) + p_filesz > b.size()) {
            fail("PT_LOAD 的文件内容越界", kExitData);
        }
        // ⚠️ **重定位检查已在 section header 层完成**（见函数头部）
        //    ⇒ ⇒ 这里**不再**做逐字节扫描（此前是**空循环** ⇒ ⇒ 零判别力）。
        // 逐字写入（**含 `.bss` 补零**：p_memsz > p_filesz 的部分 = 0）
        for (uint32_t k = 0; k < p_memsz; k += 4) {
            uint32_t data = 0;
            if (k + 4 <= p_filesz) {
                data = rd32(b, p_offset + k);
            } else if (k < p_filesz) {
                // 尾部不足 4 字节 ⇒ ⇒ 零补齐（**禁**读越界）
                for (uint32_t j = 0; j < 4 && (k + j) < p_filesz; ++j) {
                    data |= static_cast<uint32_t>(b[p_offset + k + j]) << (8 * j);
                }
            }
            fw.words.push_back(Word{p_paddr + k, data});
        }
        // 记录覆盖到的区域（供 manifest / mem-split 用）
        //
        // ⚠️ **2026-11-23 修**：此前的 `covered` 判据是「`regions` 里是否**已有**
        //    ITCM **或** DTCM」⇒ ⇒ 第 1 个 `PT_LOAD` 之后 `regions` 非空
        //    ⇒ ⇒ **第 2 个不同区域永远进不去** ⇒ ⇒ manifest 的 `regions[]`
        //    **丢 D-TCM**（实测：真 ELF 的 `words[]` 含 DTCM 地址而 `regions[]`
        //    只含 ITCM ⇒ ⇒ **产物自相矛盾**）。
        //    ⇒ ⇒ 正确判据 = 「**本段**落在哪个区域 · 且该区域**尚未**登记」。
        if (riscv::in_region(kItcm, p_paddr)) {
            if (!has_region(fw.regions, kItcm.base)) fw.regions.push_back(kItcm);
        } else if (riscv::in_region(kDtcm, p_paddr)) {
            if (!has_region(fw.regions, kDtcm.base)) fw.regions.push_back(kDtcm);
        }
    }
    if (fw.words.empty()) fail("ELF 里没有 PT_LOAD 段", kExitData);
    return fw;
}

// ---------------------------------------------------------------------------
// JSON 固件描述装载（**简单**格式 · 供工具的自测载体用）
//
//   { "entry": 2147483648,
//     "words": [ {"addr": 2147483648, "data": 12345}, ... ] }
// ---------------------------------------------------------------------------
Firmware load_json(const std::vector<uint8_t> &raw) {
    const std::string text(raw.begin(), raw.end());
    Firmware fw;
    // 极简扫描（**不**引第三方 JSON 库 ⇒ ⇒ 工具保持零依赖）
    size_t ep = text.find("\"entry\"");
    if (ep != std::string::npos) {
        size_t colon = text.find(':', ep);
        if (colon != std::string::npos) {
            fw.entry = std::strtoull(text.c_str() + colon + 1, nullptr, 0);
        }
    }
    size_t wp = text.find("\"words\"");
    if (wp == std::string::npos) fail("JSON 里没有 \"words\"", kExitData);
    size_t pos = wp;
    while (true) {
        size_t a = text.find("\"addr\"", pos);
        if (a == std::string::npos) break;
        size_t d = text.find("\"data\"", a);
        if (d == std::string::npos) break;
        size_t ac = text.find(':', a);
        size_t dc = text.find(':', d);
        if (ac == std::string::npos || dc == std::string::npos) break;
        const uint64_t addr = std::strtoull(text.c_str() + ac + 1, nullptr, 0);
        const uint64_t data = std::strtoull(text.c_str() + dc + 1, nullptr, 0);
        fw.words.push_back(Word{addr, static_cast<uint32_t>(data)});
        pos = dc + 1;
    }
    if (fw.words.empty()) fail("JSON 里没有解析出任何字", kExitData);
    return fw;
}

// ---------------------------------------------------------------------------
// 校验：**每个字 MUST 落在某个区域内且不截断**（**禁**静默丢高位）
// ---------------------------------------------------------------------------
void validate(const Firmware &fw) {
    for (const auto &w : fw.words) {
        bool ok = riscv::in_region(kItcm, w.addr) || riscv::in_region(kDtcm, w.addr);
        if (!ok) {
            std::ostringstream os;
            os << "地址 0x" << std::hex << w.addr
               << " 落在 I-TCM / D-TCM **之外** ⇒ ⇒ 拒绝装载"
               << "（**禁**静默截断 —— 截断会把固件装到**错误**的位置）";
            fail(os.str(), kExitData);
        }
        const riscv::MemRegion &r =
            riscv::in_region(kItcm, w.addr) ? kItcm : kDtcm;
        if (!riscv::word_index_fits(r, w.addr)) {
            std::ostringstream os;
            os << "地址 0x" << std::hex << w.addr
               << " 的字索引超出 TCM 的索引位宽（" << std::dec
               << riscv::kTcmAddrWidth << " 位）⇒ ⇒ 拒绝装载（**禁**静默丢高位）";
            fail(os.str(), kExitData);
        }
    }

    // ⚠️ **入口 MUST 等于 `kPcReset`**（= I-TCM 的 base）
    //
    // ⇒ ⇒ SoC 的 PC 复位值**硬连线**为 `kPcReset`
    //    （`include/cpu/riscv/rv32i_mem_map.h` · capability `rv32i-firmware-loading`
    //    的「PC 复位值 MUST 等于 I-TCM 的 base」条款）
    // ⇒ ⇒ 入口落在别处 ⇒ ⇒ 核从 `kPcReset` 取指 ⇒ ⇒ 装出来的固件**无法启动**
    // ⇒ ⇒ 且 I-TCM 初值 0 = **未知 opcode** ⇒ ⇒ 流水线在首个未知指令处**冻结**
    // ⇒ ⇒ **零报错** ⇒ ⇒ 属「静默失配」⇒ ⇒ MUST 拒绝。
    //
    // ⚠️ 此处是**唯一**的入口判据 ⇒ ⇒ ELF 与 JSON 两条装载路径**共用**
    //    ⇒ ⇒ 禁在两处各写一份（两份判据 = 静默分叉）。
    if (fw.entry != riscv::kPcReset) {
        std::ostringstream os;
        os << "入口地址 0x" << std::hex << fw.entry << " ≠ I-TCM 的 base（0x"
           << riscv::kPcReset << "）⇒ ⇒ 拒绝装载"
           << "（SoC 的 PC 复位值 = I-TCM 的 base ⇒ ⇒ 入口不符 ⇒ ⇒ 固件**无法启动**）";
        fail(os.str(), kExitData);
    }
}

// ---------------------------------------------------------------------------
// 输出
// ---------------------------------------------------------------------------
void write_mem(const Firmware &fw, const std::string &path) {
    std::ofstream out(path);
    if (!out) fail("无法写出：" + path, kExitUsage);
    for (const auto &w : fw.words) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%08x", w.data);
        out << buf << "\n";
    }
}

/// ⚠ **`mem-split`**：按区域拆分的两个字镜像（**方案 a** · ELF → init_data）
///
/// · 产物 = `<dir>/itcm.mem` + `<dir>/dtcm.mem`
/// · **行号 = 区域内的字索引** ⇒ ⇒ 空洞 MUST **补零**
///   （否则索引与地址错位 ⇒ ⇒ 固件被装到**错误**的位置 ⇒ ⇒ 静默错）
/// · 字节地址 → 区域字索引的换算**只在本文件一处**完成：
///   `(byte_addr - region.base) >> 2`
/// · 某区域无字 ⇒ ⇒ 产出**空文件**（0 字节）⇒ ⇒ 消费者据此得到空初值镜像
void write_mem_split(const Firmware &fw, const std::string &dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        fail("无法创建输出目录：" + dir + "（" + ec.message() + "）", kExitUsage);
    }

    struct RegionOut {
        const riscv::MemRegion *region;
        const char *file_name;
    };
    const RegionOut outs[2] = {{&kItcm, "itcm.mem"}, {&kDtcm, "dtcm.mem"}};

    for (const auto &o : outs) {
        // 该区域的「字索引 → 数据」（稀疏 ⇒ ⇒ 空洞补零）
        std::map<uint64_t, uint32_t> image;
        for (const auto &w : fw.words) {
            if (!riscv::in_region(*o.region, w.addr)) continue;
            image[(w.addr - o.region->base) >> 2] = w.data;
        }
        const std::filesystem::path path =
            std::filesystem::path(dir) / o.file_name;
        std::ofstream out(path);
        if (!out) fail("无法写出：" + path.string(), kExitUsage);
        if (image.empty()) continue;  // 该区域无字 ⇒ ⇒ 空文件
        const uint64_t last = image.rbegin()->first;
        for (uint64_t i = 0; i <= last; ++i) {
            char buf[16];
            const uint32_t data = image.count(i) ? image.at(i) : 0u;
            std::snprintf(buf, sizeof(buf), "%08x", data);
            out << buf << "\n";
        }
    }
}

/// ⚠ **`mem` / `vh` 遇多区域固件 ⇒ ⇒ 拒绝**（**不**静默拼接）
///
/// ⇒ ⇒ 这两种格式的字流**无地址** ⇒ ⇒ 无法表达「哪一段属于哪个区域」
/// ⇒ ⇒ 消费者 MUST NOT 靠猜（capability `rv32i-firmware-loading` 的
///    「转储器 MUST 产出**可寻址**产物」条款）
/// ⇒ ⇒ 多区域 MUST 用 `--format mem-split`（按区域拆分）或 `--format manifest`
void require_single_region(const Firmware &fw, const std::string &format) {
    if (region_count(fw) <= 1) return;
    std::ostringstream os;
    os << "固件跨越 " << region_count(fw) << " 个区域（I-TCM + D-TCM）"
       << "⇒ ⇒ `--format " << format << "` **不**支持多区域"
       << "（该格式的字流**无地址** ⇒ ⇒ 消费者 MUST NOT 靠猜）"
       << "⇒ ⇒ 请用 `--format mem-split` 或 `--format manifest`";
    fail(os.str(), kExitData);
}

void write_vh(const Firmware &fw, const std::string &path) {
    std::ofstream out(path);
    if (!out) fail("无法写出：" + path, kExitUsage);
    out << "// 自动生成（rv32i-fw-dump）—— **勿手改**\n";
    out << "// 入口字节地址 = 0x" << std::hex << fw.entry << "\n";
    out << "initial begin\n";
    out << "    $readmemh(\"" << path << "\", <mem-array>);\n";
    out << "end\n";
    // 说明：`$readmemh` 需要**数组**名 ⇒ ⇒ 这里给出**可直接粘进 TB** 的形式
    out << "// ⚠️ 数组名由 TB 提供 ⇒ ⇒ 用法：\n";
    out << "//   1) 本文件与 .mem 并列产出\n";
    out << "//   2) TB 里 `initial $readmemh(\"<mem>.mem\", mem);`\n";
}

void write_manifest(const Firmware &fw, const std::string &path) {
    std::ofstream out(path);
    if (!out) fail("无法写出：" + path, kExitUsage);
    out << "{\n";
    out << "  \"schema\": \"rv32i-fw-manifest-v1\",\n";
    out << "  \"entry\": " << fw.entry << ",\n";
    out << "  \"regions\": [\n";
    std::vector<riscv::MemRegion> regs = fw.regions;
    if (regs.empty()) regs = {kItcm, kDtcm};
    for (size_t i = 0; i < regs.size(); ++i) {
        out << "    {\"base\": " << regs[i].base << ", \"size\": " << regs[i].size
            << "}" << (i + 1 < regs.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"words\": [\n";
    for (size_t i = 0; i < fw.words.size(); ++i) {
        out << "    {\"addr\": " << fw.words[i].addr
            << ", \"data\": " << fw.words[i].data << "}"
            << (i + 1 < fw.words.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void usage() {
    std::cerr << "用法: rv32i-fw-dump --in <fw.elf|fw.json> "
                 "--format vh|mem|manifest|mem-split --out <path>\n"
                 "  · mem-split 的 --out 是**目录** ⇒ ⇒ 产出 <dir>/itcm.mem + <dir>/dtcm.mem\n";
}

}  // namespace

int main(int argc, char **argv) {
    std::string in, format, out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char *what) -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(kExitUsage);
            }
            (void)what;
            return argv[++i];
        };
        if (a == "--in") {
            in = need("--in");
        } else if (a == "--format") {
            format = need("--format");
        } else if (a == "--out") {
            out = need("--out");
        } else if (a == "--help" || a == "-h") {
            usage();
            return kExitOk;
        } else {
            std::cerr << "::error::未知参数：" << a << "\n";
            usage();
            return kExitUsage;
        }
    }
    if (in.empty() || format.empty() || out.empty()) {
        usage();
        return kExitUsage;
    }

    const std::vector<uint8_t> raw = read_file(in);
    Firmware fw;
    if (raw.size() >= 4 && raw[0] == 0x7Fu && raw[1] == 'E') {
        fw = load_elf(raw);
    } else {
        fw = load_json(raw);
    }
    validate(fw);

    if (format == "mem") {
        require_single_region(fw, format);
        write_mem(fw, out);
    } else if (format == "vh") {
        require_single_region(fw, format);
        write_vh(fw, out);
    } else if (format == "manifest") {
        write_manifest(fw, out);
    } else if (format == "mem-split") {
        write_mem_split(fw, out);
    } else {
        std::cerr << "::error::未知 --format：" << format << "\n";
        usage();
        return kExitUsage;
    }
    if (format == "mem-split") {
        std::cout << "OK：装载 " << fw.words.size() << " 字 · 入口 0x" << std::hex
                  << fw.entry << std::dec << " · 区域 " << region_count(fw)
                  << " 个 ⇒ " << out << "/{itcm,dtcm}.mem\n";
    } else {
        std::cout << "OK：装载 " << fw.words.size() << " 字 · 入口 0x" << std::hex
                  << fw.entry << std::dec << " ⇒ " << out << "\n";
    }
    return kExitOk;
}
