// See LICENSE for license details.
//
// ============================================================================
// riscv-tests "p" (physical memory) environment — CppHDL minimal variant
// ============================================================================
//
// 来源：riscv-software-src/riscv-tests 的 env/p/riscv_test.h（BSD-3-Clause）。
//
// 为什么不是上游那份：上游 RVTEST_CODE_BEGIN 无条件做
//     csrw mtvec / csrwi mstatus / csrw mepc / mret
// 而 RVTEST_PASS / RVTEST_FAIL 走 `ecall`（a7 = 93 SYS_exit）+ trap 处理程序。
// 那要求一个完整的特权/trap 机器（CSR + 异常入口），本核不实现 CSR
// （见 .rddf/improvements/rv32i-privileged-csr-2026-11.md）。
//
// 因此本文件**只替换 4 个 harness 宏**，测试体（isa/rv64ui/*.S 与
// isa/rv32ui/*.S）一字未改 ⇒ 这正是 riscv-tests 上游维护者对该问题的建议
// 做法（riscv-tests issue #368）：
//   - RVTEST_CODE_BEGIN : 清寄存器 + 落进测试体（无 mtvec / mret）
//   - RVTEST_PASS       : `sw 1, 0(tohost)`
//   - RVTEST_FAIL       : `sw TESTNUM|1, 0(tohost)`
//   - RVTEST_DATA_BEGIN : 与上游同构（.tohost / .fromhost / begin_signature）
//
// 判定协议（与上游 host 侧一致）：
//   tohost == 1        ⇒ PASS
//   tohost == TESTNUM|1 ⇒ FAIL，TESTNUM 是被判失败的用例号（>= 2）
//
// SPDX-License-Identifier: BSD-3-Clause
// ============================================================================

#ifndef _ENV_PHYSICAL_SINGLE_CORE_H
#define _ENV_PHYSICAL_SINGLE_CORE_H

//-----------------------------------------------------------------------
// RVTEST_RV*U — 上游用这些宏选择 isa 变体；单 hart 无扩展初始化 ⇒ 空 init
//-----------------------------------------------------------------------
#define RVTEST_RV32U                                                    \
  .macro init;                                                          \
  .endm

#define RVTEST_RV32UF RVTEST_RV32U
#define RVTEST_RV32UV RVTEST_RV32U
#define RVTEST_RV32UVX RVTEST_RV32U
#define RVTEST_RV32M RVTEST_RV32U
#define RVTEST_RV32S RVTEST_RV32U

#define RVTEST_RV64U RVTEST_RV32U
#define RVTEST_RV64UF RVTEST_RV32U
#define RVTEST_RV64UV RVTEST_RV32U
#define RVTEST_RV64UVX RVTEST_RV32U
#define RVTEST_RV64M RVTEST_RV32U
#define RVTEST_RV64S RVTEST_RV32U

//-----------------------------------------------------------------------
// 上游 harness 期望存在但本环境不需要的钩子（一律定义为空）
//-----------------------------------------------------------------------
#define RISCV_MULTICORE_DISABLE
#define INIT_RNMI
#define INIT_SATP
#define INIT_PMP
#define DELEGATE_NO_TRAPS
#define CHECK_XLEN
#define EXTRA_INIT
#define EXTRA_INIT_TIMER
#define FILTER_TRAP
#define FILTER_PAGE_FAULT
#define INTERRUPT_HANDLER

//-----------------------------------------------------------------------
// 寄存器初始化（RISC-V 上电时 GPR 值未定义 ⇒ 测试必须从确定的 0 开始）
//-----------------------------------------------------------------------
#define INIT_XREG                                                       \
  li x1, 0; li x2, 0; li x3, 0; li x4, 0; li x5, 0; li x6, 0;           \
  li x7, 0; li x8, 0; li x9, 0; li x10, 0; li x11, 0; li x12, 0;        \
  li x13, 0; li x14, 0; li x15, 0; li x16, 0; li x17, 0; li x18, 0;     \
  li x19, 0; li x20, 0; li x21, 0; li x22, 0; li x23, 0; li x24, 0;     \
  li x25, 0; li x26, 0; li x27, 0; li x28, 0; li x29, 0; li x30, 0;     \
  li x31, 0;

//-----------------------------------------------------------------------
// RVTEST_CODE_BEGIN — 最小复位：清寄存器、TESTNUM 归零，然后**顺序落进**测试体
//   （无 trap_vector / mtvec / mret —— 本核无 CSR）
//-----------------------------------------------------------------------
#define RVTEST_CODE_BEGIN                                               \
        .section .text.init;                                            \
        .align  6;                                                      \
        .globl _start;                                                  \
_start:                                                                 \
        INIT_XREG;                                                      \
        li TESTNUM, 0;

//-----------------------------------------------------------------------
// RVTEST_CODE_END — 测试体正常走到这里说明它没经 PASS/FAIL 出口 ⇒ 非法编码
//-----------------------------------------------------------------------
#define RVTEST_CODE_END                                                 \
        unimp

//-----------------------------------------------------------------------
// PASS / FAIL —— 直接写 tohost（不经 ecall / trap）
//-----------------------------------------------------------------------
#define TESTNUM gp

#define RVTEST_PASS                                                     \
        fence;                                                          \
        li TESTNUM, 1;                                                  \
        la t5, tohost;                                                  \
        sw TESTNUM, 0(t5);                                              \
1:      j 1b

#define RVTEST_FAIL                                                     \
        fence;                                                          \
        ori TESTNUM, TESTNUM, 1;                                        \
        la t5, tohost;                                                  \
        sw TESTNUM, 0(t5);                                              \
1:      j 1b

//-----------------------------------------------------------------------
// 数据段（与上游同构）
//-----------------------------------------------------------------------
#define RVTEST_DATA_BEGIN                                               \
        .pushsection .tohost,"aw",@progbits;                            \
        .align 6; .global tohost; tohost: .dword 0; .size tohost, 8;    \
        .align 6; .global fromhost; fromhost: .dword 0; .size fromhost, 8;\
        .popsection;                                                    \
        .align 4; .global begin_signature; begin_signature:

#define RVTEST_DATA_END .align 4; .global end_signature; end_signature:

#endif  // _ENV_PHYSICAL_SINGLE_CORE_H
