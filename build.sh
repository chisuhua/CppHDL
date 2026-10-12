#!/usr/bin/env bash
# ============================================================================
# @file    build.sh
# @brief   CppHDL 统一构建 / 测试入口
#
# 用法:
#   ./build.sh [options] [actions...]
#
# 默认行为 (无参数): 仅构建 (向后兼容 — 不会触发任何测试)
#
# Actions (可组合):
#   --build              仅构建 (cmake 配置 + 编译)
#   --test               跑全部 ctest (跳过 perf_subprocess_isolation, ~10-15 min)
#   --test-all           跑完整 ctest (含 perf_subprocess_isolation, ~30-50 min)
#   --verilator          Verilator 后端完整覆盖 (~6-10 min)
#   --port-examples      跑 SpinalHDL 移植示例 (./run_all_ported_tests.sh)
#   --verilator-build    冷构 Verilator 子模块 (10-30 min)
#   --clean              清理 build 目录 (保留 verilator-install 节省 10-30 min)
#   --all                真正全做: --build --test-all --verilator --port-examples (~30-50 min)
#
# Build 选项 (影响 --build / --all):
#   --debug                      CMAKE_BUILD_TYPE=Debug
#   --release                    CMAKE_BUILD_TYPE=Release (默认)
#   --no-jit                     -DCH_JIT_ENABLE=OFF
#   --no-verilator-cmake         -DBUILD_VERILATOR=OFF
#   --asan                       -DENABLE_ASAN=OFF → ON
#   --verbose-log                -DENABLE_VERBOSE_LOGGING=ON
#   --debug-log                  -DENABLE_DEBUG_LOGGING=ON
#   -j, --jobs N                 并行构建数 (默认 nproc)
#
# 通用:
#   -h, --help              显示帮助
#   -n, --dry-run           显示命令但不执行
#   -v, --verbose           详细输出 (set -x)
#
# 退出码: 0=成功, 非 0=任意阶段失败
# ============================================================================

set -euo pipefail

# ----------------------------------------------------------------------------
# 全局变量
# ----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
JOBS="$(nproc 2>/dev/null || echo 4)"

# Action 开关
ACT_BUILD=0
ACT_TEST=0
ACT_TEST_ALL=0
ACT_VERILATOR=0
ACT_PORT_EXAMPLES=0
ACT_VERILATOR_BUILD=0
ACT_CLEAN=0
ACT_ALL=0

# Build 选项
BUILD_TYPE="Release"
ENABLE_JIT=1
ENABLE_VERILATOR_CMAKE=1
ENABLE_ASAN=0
VERBOSE_LOG=0
DEBUG_LOG=0

# 通用
DRY_RUN=0
VERBOSE=0

# 颜色
if [[ -t 1 ]]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
    BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; BLUE=''; BOLD=''; NC=''
fi

# 失败阶段记录
declare -a FAILED_PHASES=()

# ----------------------------------------------------------------------------
# 帮助
# ----------------------------------------------------------------------------
usage() {
    sed -n '3,/^# ====/{/^# ====/!p;}' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
    exit 0
}

# ----------------------------------------------------------------------------
# 工具函数
# ----------------------------------------------------------------------------
log()      { echo -e "${BLUE}[$(date +%H:%M:%S)]${NC} $*"; }
ok()       { echo -e "${GREEN}✓${NC} $*"; }
warn()     { echo -e "${YELLOW}!${NC} $*"; }
err()      { echo -e "${RED}✗${NC} $*" >&2; }
header()   { echo -e "\n${BOLD}========== $* ==========${NC}"; }

# 执行命令: 支持 dry-run + verbose
run() {
    if [[ ${VERBOSE} -eq 1 ]]; then
        echo -e "${YELLOW}\$ $*${NC}"
    fi
    if [[ ${DRY_RUN} -eq 0 ]]; then
        "$@"
    fi
}

# 阶段包装: 失败时记录但不立即退出 (除非严格模式)
phase() {
    local name="$1"; shift
    header "${name}"
    if "$@"; then
        ok "${name} 完成"
    else
        local rc=$?
        err "${name} 失败 (退出码 ${rc})"
        FAILED_PHASES+=("${name}")
        return ${rc}
    fi
}

# ----------------------------------------------------------------------------
# 参数解析
# ----------------------------------------------------------------------------
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)            usage ;;
            -n|--dry-run)         DRY_RUN=1 ;;
            -v|--verbose)         VERBOSE=1; set -x ;;
            --build)              ACT_BUILD=1 ;;
            --test)               ACT_TEST=1 ;;
            --test-all)           ACT_TEST_ALL=1 ;;
            --verilator)          ACT_VERILATOR=1 ;;
            --port-examples)      ACT_PORT_EXAMPLES=1 ;;
            --verilator-build)    ACT_VERILATOR_BUILD=1 ;;
            --clean)              ACT_CLEAN=1 ;;
            --all)                ACT_ALL=1 ;;
            --debug)              BUILD_TYPE="Debug" ;;
            --release)            BUILD_TYPE="Release" ;;
            --no-jit)             ENABLE_JIT=0 ;;
            --no-verilator-cmake) ENABLE_VERILATOR_CMAKE=0 ;;
            --asan)               ENABLE_ASAN=1 ;;
            --verbose-log)        VERBOSE_LOG=1 ;;
            --debug-log)          DEBUG_LOG=1 ;;
            -j|--jobs)            JOBS="${2:?--jobs 需要参数}"; shift ;;
            -j*)                  JOBS="${1#-j}" ;;
            -*)
                err "未知选项: $1"; echo "使用 --help 查看帮助"; exit 2 ;;
            *)
                err "未知位置参数: $1"; exit 2 ;;
        esac
        shift
    done

    # 默认行为: 无参数 = 仅构建 (向后兼容)
    if [[ $((ACT_BUILD + ACT_TEST + ACT_TEST_ALL + ACT_VERILATOR + \
             ACT_PORT_EXAMPLES + ACT_VERILATOR_BUILD + ACT_CLEAN + ACT_ALL)) -eq 0 ]]; then
        ACT_BUILD=1
    fi

    # --all 展开: 真正全做 (含 perf_subprocess_isolation)
    if [[ ${ACT_ALL} -eq 1 ]]; then
        ACT_BUILD=1
        ACT_TEST_ALL=1
        ACT_VERILATOR=1
        ACT_PORT_EXAMPLES=1
    fi

    # --test-all 隐含 --test
    if [[ ${ACT_TEST_ALL} -eq 1 ]]; then
        ACT_TEST=1
    fi
}

# ----------------------------------------------------------------------------
# 阶段实现
# ----------------------------------------------------------------------------

# 子模块初始化 (仅当 third_party/verilator 缺失时)
init_submodules() {
    if [[ -f "${SCRIPT_DIR}/third_party/verilator/CMakeLists.txt" ]]; then
        ok "third_party/verilator 已就位, 跳过子模块初始化"
        return 0
    fi
    warn "third_party/verilator 缺失, 尝试初始化子模块..."
    if [[ -x "${SCRIPT_DIR}/scripts/init-submodules.sh" ]]; then
        run "${SCRIPT_DIR}/scripts/init-submodules.sh"
    else
        err "scripts/init-submodules.sh 不可用, 请手动: git submodule update --init --recursive"
        return 1
    fi
}

# cmake 配置
cmake_configure() {
    local extra_args=()
    [[ ${ENABLE_JIT} -eq 0 ]]               && extra_args+=("-DCH_JIT_ENABLE=OFF")
    [[ ${ENABLE_VERILATOR_CMAKE} -eq 0 ]]   && extra_args+=("-DBUILD_VERILATOR=OFF")
    [[ ${ENABLE_ASAN} -eq 1 ]]              && extra_args+=("-DENABLE_ASAN=ON")
    [[ ${VERBOSE_LOG} -eq 1 ]]              && extra_args+=("-DENABLE_VERBOSE_LOGGING=ON")
    [[ ${DEBUG_LOG} -eq 1 ]]                && extra_args+=("-DENABLE_DEBUG_LOGGING=ON")

    if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
        log "build/ 已配置, 增量复用 (如需重配请 --clean)"
        return 0
    fi
    log "cmake -B build -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${extra_args[*]:-}"
    run cmake -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -S "${SCRIPT_DIR}" \
        "${extra_args[@]}"
}

# 编译
cmake_build() {
    cmake_configure
    log "cmake --build build -j${JOBS}"
    run cmake --build "${BUILD_DIR}" -j"${JOBS}"
}

# Verilator 冷构
build_verilator() {
    log "cmake --build build --target verilator (10-30 min 冷构)"
    run cmake --build "${BUILD_DIR}" --target verilator -j"${JOBS}"
}

# 跑 ctest (默认排除 perf_subprocess_isolation)
run_ctest() {
    local extra=("$@")
    log "ctest (含 perf_subprocess_isolation) — 进入 ${BUILD_DIR}"
    (
        cd "${BUILD_DIR}"
        run ctest --output-on-failure -j"${JOBS}" "${extra[@]}"
    )
}

run_ctest_fast() {
    log "ctest (排除 perf_subprocess_isolation) — 进入 ${BUILD_DIR}"
    (
        cd "${BUILD_DIR}"
        run ctest --output-on-failure -j"${JOBS}" -E perf_subprocess_isolation
    )
}

# Verilator 后端完整覆盖
run_verilator_full() {
    log "Verilator 后端完整覆盖:"
    log "  • ctest -L verilator              (perf_three_way, perf_verilator_integration)"
    log "  • ctest -R test_verilator_e2e_harness"
    log "  • ctest -R test_axi_lite_verilator_e2e"
    log "  • ctest -R test_verilator_three_way|backend|error_flow  (无二进制依赖)"
    (
        cd "${BUILD_DIR}"
        run ctest --output-on-failure -L verilator -E perf_subprocess_isolation
        run ctest --output-on-failure -R 'test_verilator_e2e_harness|test_axi_lite_verilator_e2e'
        run ctest --output-on-failure -R 'test_verilator_(backend|error_flow|three_way)'
    )
}

# SpinalHDL 移植示例
run_port_examples() {
    log "SpinalHDL 移植示例 (./run_all_ported_tests.sh)"
    run "${SCRIPT_DIR}/run_all_ported_tests.sh" "${BUILD_DIR##*/}"
}

# 清理 build 目录
do_clean() {
    if [[ ! -d "${BUILD_DIR}" ]]; then
        ok "build/ 不存在, 无需清理"
        return 0
    fi
    # 保留 verilator-install 节省 10-30 min 冷构
    local preserved=""
    if [[ -d "${BUILD_DIR}/verilator-install" ]]; then
        preserved="${BUILD_DIR}/verilator-install"
        warn "保留 verilator-install (下次 --build 跳过 10-30 min 冷构)"
    fi
    log "清理 ${BUILD_DIR}/ (除 verilator-install)"
    if [[ ${DRY_RUN} -eq 0 ]]; then
        find "${BUILD_DIR}" -mindepth 1 -maxdepth 1 \
            ! -name 'verilator-install' -exec rm -rf {} +
    fi
}

# ----------------------------------------------------------------------------
# 主流程
# ----------------------------------------------------------------------------
main() {
    parse_args "$@"

    log "CppHDL build.sh"
    log "  BUILD_DIR=${BUILD_DIR}"
    log "  BUILD_TYPE=${BUILD_TYPE}, JOBS=${JOBS}, DRY_RUN=${DRY_RUN}"
    log "  ACT: build=${ACT_BUILD} test=${ACT_TEST} test_all=${ACT_TEST_ALL} \
verilator=${ACT_VERILATOR} port=${ACT_PORT_EXAMPLES} \
verilator_build=${ACT_VERILATOR_BUILD} clean=${ACT_CLEAN}"

    if [[ ${DRY_RUN} -eq 1 ]]; then
        warn "DRY-RUN 模式: 仅显示命令, 不执行"
    fi

    # 前置: 子模块
    init_submodules

    # 阶段执行 (按依赖顺序, 失败时继续收集)
    [[ ${ACT_CLEAN} -eq 1 ]]             && phase "clean"                       do_clean
    [[ ${ACT_VERILATOR_BUILD} -eq 1 ]]   && phase "verilator-build"             build_verilator
    [[ ${ACT_BUILD} -eq 1 ]]             && phase "build (${BUILD_TYPE})"       cmake_build
    [[ ${ACT_TEST} -eq 1 && ${ACT_TEST_ALL} -eq 0 ]] \
                                        && phase "ctest (fast, no perf-iso)"   run_ctest_fast
    [[ ${ACT_TEST_ALL} -eq 1 ]]          && phase "ctest (full, + perf-iso)"    run_ctest
    [[ ${ACT_VERILATOR} -eq 1 ]]         && phase "verilator full coverage"     run_verilator_full
    [[ ${ACT_PORT_EXAMPLES} -eq 1 ]]     && phase "port examples"               run_port_examples

    # 汇总
    header "汇总"
    if [[ ${#FAILED_PHASES[@]} -eq 0 ]]; then
        ok "全部阶段成功"
        exit 0
    else
        err "失败阶段 (${#FAILED_PHASES[@]}):"
        for p in "${FAILED_PHASES[@]}"; do
            err "  - ${p}"
        done
        exit 1
    fi
}

main "$@"
