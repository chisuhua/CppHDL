#!/usr/bin/env bash
# scripts/lib/cxx23_compilers.sh
#
# **C++23 编译器探测**（被多个护栏 `source`）——唯一实现，避免各自漂移。
#
# ⚠️ **为什么需要它**（2026-11-13 · CI 实测 RC-7）：
#   本仓要求 C++23，而多个护栏是「`for c in g++ clang++; do … -std=c++23`」的写法
#   ⇒ ⇒ 在 Ubuntu 22.04 上**裸 `clang++` 是 clang-14** ⇒ 它只认 `-std=c++2b`
#   ⇒ ⇒ 传 `-std=c++23` 直接报 `error: invalid value 'c++23'`
#   ⇒ ⇒ 护栏把**编译器太旧**误报成**业务失败**（假红，且诊断指向错误方向）。
#   ⇒ ⇒ 而 CI 装的是**版本化**的 `clang-22`（`clang++-22`）⇒ 没有裸 `clang++`
#   ⇒ ⇒ 只按裸名字探测的话，CI 里永远只用得上 g++。
#
# 语义：`cxx23_probe` ⇒ 设置调用者的 `CXX_LIST`（**绝对路径**、已去重、已按
#   「确实能编译 `-std=c++23`」筛选）。一个都筛不出来 ⇒ 返回 1（**fail-closed**：
#   不得退化成「跳过检查」）。被筛掉的候选会打一行说明（**不静默**）。
#
# 用法：
#   source "$REPO/scripts/lib/cxx23_compilers.sh"
#   cxx23_probe || exit 1
#   note "编译器：${CXX_LIST[*]}"

cxx23_probe() {
    local c bin real
    local seen=" "
    local candidates=()

    CXX_LIST=()

    # ① 裸名字（本地开发机常见）
    for c in g++ clang++; do
        command -v "$c" >/dev/null 2>&1 && candidates+=("$c")
    done
    # ② 版本化名字（CI 装 clang-NN ⇒ 只有 clang++-NN）
    for c in /usr/bin/clang++-* /usr/local/bin/clang++-*; do
        [ -x "$c" ] && candidates+=("$c")
    done

    for c in ${candidates[@]+"${candidates[@]}"}; do
        bin="$(command -v "$c" 2>/dev/null || true)"
        [ -n "$bin" ] || continue
        # 去重：同一个二进制的多个名字（clang++ / clang++-22 指向同一文件）只留一次
        real="$(readlink -f "$bin" 2>/dev/null || printf '%s' "$bin")"
        case "$seen" in *" $real "*) continue ;; esac
        seen="$seen$real "
        if printf 'int main(){}\n' | "$bin" -std=c++23 -x c++ -fsyntax-only - >/dev/null 2>&1; then
            CXX_LIST+=("$bin")
        else
            printf '   跳过 %s（不支持 -std=c++23 ⇒ 不是本项目的有效检查对象）\n' "$bin"
        fi
    done

    if [ ${#CXX_LIST[@]} -eq 0 ]; then
        printf '❌ 未找到能编译 C++23 的编译器（需要 g++ 或 clang++ 且支持 -std=c++23）\n' >&2
        return 1
    fi
    return 0
}
