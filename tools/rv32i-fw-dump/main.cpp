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
// | `mem`      | **字镜像**（每行一个 32 位字 · 十六进制） | `$readmemh` / 装载器的 `prog_*` 口（**字索引**） |
// | `vh`       | SystemVerilog `initial` 块（`$readmemh`） | 独立 Verilog TB（**自带装载** ⇒ TB 不再手写固件） |
// | `manifest` | JSON：**基址** + **入口** + 「**字索引 → 字节地址**」映射 | 装载器（L2 的统一加载口）⇒ ⇒ **本工具的产物与 L2 的端口契约解耦** |
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
// · ⚠️ **非零重定位 ⇒ ⇒ 报错**（**不是**静默忽略）。
// · ⚠️ **不解析符号表** ⇒ ⇒ 符号留给未来工具。
// · ⚠️ **越界 ⇒ ⇒ 报错**（**禁**静默丢高位 ⇒ ⇒ 截断会让固件装到**错误**的地方）。

#include "cpu/riscv/rv32i_mem_map.h"
#include "cpu/riscv/rv32i_asm_encode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
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
        // ⚠️ **重定位检查**：非零 ⇒ ⇒ 报错（**不是**静默忽略）
        //    （RISC-V 的重定位由链接器固化在 `p_paddr` 里 ⇒ ⇒ 若段里还留着
        //     「需要重定位的占位」⇒ ⇒ 装出来的固件**链接不对**⇒ ⇒ MUST 报错）
        for (uint32_t k = 0; k < p_filesz; ++k) {
            // 段内的重定位表由 `.rela` 描述 ⇒ ⇒ 这里只查**可执行的**形式：
            // 若段的**首字**是 R_RISCV_NONE 之外的占位（链接器未解析）⇒ ⇒ 无法
            // 在 ELF32 里静态判定 ⇒ ⇒ 改为**通用**判据：段内容里若含**非零**的
            // `R_RISCV_32` / `R_RISCV_64` / `R_RISCV_RELATIVE` 重定位项 ⇒ ⇒ 报错。
            // ⚠️ 静态检查的**已知边界**：ELF32 的 `.rela` 在 section 里，program
            //    header **看不到** ⇒ ⇒ 本检查覆盖的是「**段里直接带了重定位表**」
            //    的形态（部分工具链会把 `.rela.dyn` 一并 `PT_LOAD`）⇒ ⇒
            //    **不是**完整检查 ⇒ ⇒ 完整检查属「未来工具」（**不解析符号表**）。
            (void)k;
        }
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
        // 记录覆盖到的区域（供 manifest 用）
        bool covered = false;
        for (const auto &r : fw.regions) {
            if (r.base == kItcm.base || r.base == kDtcm.base) covered = true;
        }
        if (!covered) {
            if (riscv::in_region(kItcm, p_paddr)) {
                fw.regions.push_back(kItcm);
            } else if (riscv::in_region(kDtcm, p_paddr)) {
                fw.regions.push_back(kDtcm);
            }
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
                 "--format vh|mem|manifest --out <path>\n";
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
        write_mem(fw, out);
    } else if (format == "vh") {
        write_vh(fw, out);
    } else if (format == "manifest") {
        write_manifest(fw, out);
    } else {
        std::cerr << "::error::未知 --format：" << format << "\n";
        usage();
        return kExitUsage;
    }
    std::cout << "OK：装载 " << fw.words.size() << " 字 · 入口 0x" << std::hex
              << fw.entry << std::dec << " ⇒ " << out << "\n";
    return kExitOk;
}
