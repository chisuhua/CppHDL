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

#include "rtl_emit/verif.h"

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

/// `.verif` 的 `RESET` 原语**不得**指向**框架复位端口**（manifest `role == "reset"`）。
///
/// ⚠️ **为什么（2026-11-21 实测 · 两端同证）**：框架复位在**两个后端都是静默空操作**
///   · **解释器**：`build_register` 给寄存器的 `rst_val` 是 **nullptr**
///     ⇒ 复位期间「只 return、不赋值」⇒ ⇒ 寄存器**保持原值**；
///   · **渲染 TB**：释放动作与**最后一个**时钟沿**同拍**⇒ ⇒ 该沿走 `else` 分支
///     ⇒ ⇒ 刚写下的复位值被**同拍覆盖**。
/// ⇒ ⇒ **看起来像复位、实际不复位** ⇒ ⇒ 「看起来能用」比「明确不支持」更危险
/// ⇒ ⇒ **fail-closed 拒绝**（与 ADR-076 的「宽端口 / sanitize 撞名 / 历史不足」同类）。
///
/// ⚠️ **两端 MUST 共用本函数**：否则「解释器接受、渲染器拒绝」⇒ ⇒ **两端对同一份
///   `.verif` 给出不同结论**（两端同结论是 ADR-074 D1 的核心不变量）。
///
/// @param program 待判的 IR（只需看 `Reset` 原语）
/// @param manifest 端口表（`port_table[]` 按产物端口名 · `ports[]` 按 C++ 成员名）
/// @return 空串 = 合法；非空 = 错误消息（点名端口与原因）
inline std::string verif_reset_port_violation(const VerifProgram &program,
                                               const RtlManifest &manifest) {
    // 端口标识符 → role
    // ⚠️ **优先级 MUST 镜像解释器的 `Resolver::resolve`**（`src/rtl_emit/verif_run.cpp`）：
    //   ① 根组件自有的成员名 → ② 产物端口名（`top_<member>`）→ ③ **成员名唯一**的兜底
    //   （不限 owner ⇒ 后代端口）。
    //   ⇒ 否则「解析器认为端口存在、但本检查查不到 role」⇒ ⇒ **漏判** ⇒ ⇒ 一端接受并空跑、
    //   另一端拒绝 ⇒ ⇒ 两端结论分叉（本次改动自己声明的不变量被破坏）。
    //   ⇒ ⇒ 路径 ③ 命中多个 ⇒ **多义** ⇒ 交由解析器自己的 fail-closed 处理 ⇒ ⇒ 本检查不判。
    auto role_of = [&](const std::string &ident) -> std::string {
        // ① 根组件自有（`is_top_io`）的成员名
        for (const auto &e : manifest.ports) {
            if (e.is_top_io && !e.cpp_member.empty() && e.cpp_member == ident) {
                return e.role;
            }
        }
        // ② 产物端口名
        for (const auto &p : manifest.port_table) {
            if (p.name == ident) {
                return p.role;
            }
        }
        // ③ 成员名唯一的兜底（后代端口）
        std::string role;
        int hits = 0;
        for (const auto &e : manifest.ports) {
            if (!e.cpp_member.empty() && e.cpp_member == ident) {
                role = e.role;
                ++hits;
            }
        }
        return hits == 1 ? role : std::string{};
    };
    for (const auto &sc : program.scenarios) {
        for (const auto &op : sc.ops) {
            if (op.kind != VerifOpKind::Reset) {
                continue;
            }
            if (role_of(op.port) == "reset") {
                return "场景 `" + sc.name + "`：`RESET " + op.port +
                       "` 指向**框架复位端口**（role == \"reset\"）⇒ ⇒ 框架复位在**两个"
                       "后端都是静默空操作**（解释器：`rst_val` 为 nullptr ⇒ 只 return；"
                       "渲染 TB：释放动作与最后一个沿同拍 ⇒ 复位值被覆盖）⇒ ⇒ "
                       "看起来像复位、实际不复位 ⇒ ⇒ fail-closed（运行期中框架复位"
                       "当前**不可表达**）";
            }
        }
    }
    return {};
}

} // namespace rtl_emit
} // namespace ch
