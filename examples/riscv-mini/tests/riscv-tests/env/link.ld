/* See LICENSE for license details.
 *
 * riscv-tests "p" 链接布局 —— CppHDL 双 TCM 变体
 *
 * 上游 env/p/link.ld 假设**单一平坦内存**：.text.init/.tohost/.text/.data
 * 依次排在同一段 0x8000_0000 起（.tohost 靠 ALIGN(0x1000) 落到 0x8000_1000）。
 *
 * 本核是**双 TCM**（include/cpu/riscv/rv32i_mem_map.h）：
 *   I-TCM 0x8000_0000 —— 只走**取指**
 *   D-TCM 0x8001_0000 —— 只走**数据**（load / store）
 * ⇒ 上游布局会把 .data 放进 I-TCM ⇒ 读到 0（实测：lb/lbu/lh/lhu/lw/sb/sh
 *   共 7 个测试失败，且正/负对照可复现）。
 * ⇒ 因此：指令留在 I-TCM，数据（含 .tohost）全部搬到 D-TCM 起始处。
 *
 * 固定地址（判据依赖，勿随意改动）：
 *   .text.init / .text @ 0x8000_0000
 *   .tohost            @ 0x8001_0000   ← 运行器靠这个地址认 PASS/FAIL 写
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
OUTPUT_ARCH( "riscv" )
ENTRY(_start)

SECTIONS
{
  /* ── I-TCM：只放指令 ── */
  . = 0x80000000;
  .text.init : { *(.text.init) }
  .text      : { *(.text) }

  /* ── D-TCM：.tohost 钉在区首，随后是数据/未初始化数据 ── */
  . = 0x80010000;
  .tohost : { *(.tohost) }
  .data   : { *(.data) }
  .bss    : { *(.bss) *(COMMON) }
  . = ALIGN(4);
  _end = .;
}
