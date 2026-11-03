#pragma once

// Inline implementations for ch_stream member functions
// This file must be included AFTER both:
//   - bundle/stream_bundle.h
//   - chlib/stream_pipeline.h
//
// ⚠️⚠️ **`chlib::` MUST 写成 `::chlib::`（2026-11-03 修复「`chlib.h` 单独编译失败」）**
//
// 本仓有**两个** `chlib` 命名空间：
//   - `::chlib`（**顶层**）—— `include/chlib/*.h` 里**绝大多数**组件实际所在
//     （`stream_pipeline.h` 的 `stream_m2s_pipe` / `stream_s2m_pipe` /
//     `stream_half_pipe` 都在这里）；
//   - `ch::chlib` —— `include/chlib/simulator_trace.h` 与
//     `include/cpu/riscv/clint.h` 所在。
//
// ⇒ ⇒ 本文件位于 `namespace ch` **内部** ⇒ ⇒ 此时写 `chlib::X` 的名字查找
// **先在 `ch` 里找** ⇒ ⇒ 命中 `ch::chlib` ⇒ ⇒ **遮蔽**了顶层的 `::chlib`
// ⇒ ⇒ 编译错误：
//     'stream_m2s_pipe' is not a member of 'ch::chlib';
//     did you mean 'chlib::stream_m2s_pipe'?
// ⇒ ⇒ 该错误是**包含顺序相关**的：只要 `ch::chlib` 先被声明（如
// `chlib.h` 先 include 了 `chlib/simulator_trace.h`）就会触发 ⇒ ⇒
// `#include "chlib.h"`（`include/AGENTS.md` 记载的**公共入口**）**单独编译失败**
// ⇒ ⇒ ⇒ **显式写 `::chlib::`** 消除遮蔽 ⇒ ⇒ 与 include 顺序**无关**。
//
// ⚠️ 本仓**声明的**公共命名空间是 `ch::chlib`（`include/AGENTS.md`），
//    而**实际**多数组件在 `::chlib` ⇒ ⇒ 二者**不一致** ⇒ ⇒ 该不一致
//    **独立立项**（改命名空间 = 公开 API 变更 ⇒ 需 ADR）⇒ 本次**只修编译失败**，
//    **不动命名空间布局**。

namespace ch {

template <typename T>
auto ch_stream<T>::m2sPipe() {
    return ::chlib::stream_m2s_pipe(*this);
}

template <typename T>
auto ch_stream<T>::stage() {
    return ::chlib::stream_m2s_pipe(*this);
}

template <typename T>
auto ch_stream<T>::s2mPipe() {
    return ::chlib::stream_s2m_pipe(*this);
}

template <typename T>
auto ch_stream<T>::halfPipe() {
    return ::chlib::stream_half_pipe(*this);
}

} // namespace ch
