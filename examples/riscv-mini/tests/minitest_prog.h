/**
 * Minimal pipeline verification test using raw instructions.
 * Program: sets x1=1, then stores 1 to tohost addr 0x1000 via SW.
 * 
 * Binary encoding (little-endian RV32I):
 *  0x0000: addi x1, x0, 1     => 0x00000093
 *  0x0004: lui x10, 0x1       => 0x00001537 (x10 = 0x1000)
 *  0x0008: sw x1, 0x0(x10)    => 0x00152023
 *  0x000c: unimp / ebreak     => 0x00000073
 */
constexpr uint32_t minitest_prog[] = {
    0x00000093,  // addi x1, x0, 1
    0x00001537,  // lui x10, 0x1  (addr = 0x1000)
    0x00152023,  // sw x1, 0x0(x10) (write 1 to tohost)
    0x00000073,  // ebreak (trap / stop)
};
