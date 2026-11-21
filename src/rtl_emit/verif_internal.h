// src/rtl_emit/verif_internal.h
//
// `.verif` 的**内部**共享设施（ADR-076）—— ⚠️ **不是公开面**（公开面 = `include/rtl_emit/verif.h`）。
//
// ⚠️ **为什么需要它**：历史序列的 Verilog 数组名 MUST 由「**DSL 标识符**」确定性地导出
//   ⇒ ⇒ 扫描器（撞名检测）与渲染器（数组声明）MUST 用**同一个**函数
//   ⇒ ⇒ 否则两端算出不同的名字 ⇒ ⇒ 两条历史**静默共用一个数组** ⇒ ⇒ 数据串流。
//
// ⚠️ **单射性（ADR-076 D8）**：输出串 MUST 能**唯一**还原输入 ⇒ ⇒ 规则 =
//   逐字符转义：`[A-Za-z0-9]` **原样保留**；**其余一律**（含 `_`）转成 `_` + 2 位十六进制。
//   ⇒ ⇒ 输出里 `_` 之后**必跟** 2 位 hex ⇒ ⇒ 解析唯一 ⇒ ⇒ **单射**。
//   ⇒ ⇒ 代价 = 可读性下降（`in0_payload` → `in0_5Fpayload`）⇒ ⇒ 但数组名是**内部**的
//   ⇒ ⇒ 不影响 `.verif` 的可读性（`.verif` 里写的仍是原标识符）。
//
// ⚠️ **不要**在本文件放任何**公开** API ⇒ ⇒ 公开面登记义务不因此触发。

#pragma once

#include <cstdint>
#include <string>

namespace ch {
namespace rtl_emit {

/// 把 `.verif` 里的**端口标识符**转成 Verilog 合法且**单射**的后缀名。
///
/// ⚠️ 输出**不含** `hist_` 前缀（调用方自己加）⇒ ⇒ 本函数只负责「标识符 → 合法标识符」。
/// ⚠️ **空标识符** ⇒ 返回空串 ⇒ ⇒ 调用方 MUST 拒绝（DSL 端口名解析会先失败）。
inline std::string verif_hist_ident(const std::string &ident) {
    static const char *kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(ident.size() * 2);
    for (const char c : ident) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9');
        if (plain) {
            out.push_back(c);
        } else {
            out.push_back('_');
            const auto u = static_cast<unsigned char>(c);
            out.push_back(kHex[(u >> 4) & 0xFu]);
            out.push_back(kHex[u & 0xFu]);
        }
    }
    return out;
}

} // namespace rtl_emit
} // namespace ch
