#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ch {
namespace jit {

enum class JitOp : uint8_t {
  LOAD_DATA,
  STORE_DATA,
  ADD,
  SUB,
  MUL,
  MOD,
  AND,
  OR,
  XOR,
  NOT,
  SHIFT_LEFT,
  SHIFT_RIGHT,
  ROTATE_LEFT,
  ROTATE_RIGHT,
  SEXT,
  ZEXT,
  SSHRSHN,
  NEG,
  EQ,
  NE,
  LT,
  LE,
  GT,
  GE,
  SELECT,
  BIT_SELECT,
  BITS_EXTRACT,
  CONCAT,
  SLICE,
  LOAD_CONST,
  JUMP,
  BRANCH,
  LABEL,
  REG_NEXT,
  MEM_READ,
  MEM_WRITE,
  DIV,
  POPCOUNT,
  AND_REDUCE,
  OR_REDUCE,
  XOR_REDUCE,
  BITS_UPDATE,
  ASSIGN,
  CALL_EXTERNAL,
  NOP
};

using VRegId = uint32_t;
constexpr VRegId INVALID_VREG = ~0u;
using NodeId = uint32_t;
using BitWidth = uint16_t;

struct ImmValue {
  uint64_t value;
  BitWidth bitwidth;
  ImmValue(uint64_t v, BitWidth bw) : value(v), bitwidth(bw) {}
};

struct JitInstr {
  JitOp op;
  VRegId dst = INVALID_VREG;
  VRegId src0 = INVALID_VREG;
  VRegId src1 = INVALID_VREG;
  VRegId src2 = INVALID_VREG;
  NodeId node_id = 0;
  BitWidth bitwidth = 0;     // 结果位宽
  BitWidth src_bitwidth = 0; // 源操作数位宽（用于 sext/zext/sshr 等）
  ImmValue imm = ImmValue(0, 0);

  JitInstr() = default;
  explicit JitInstr(JitOp operation, BitWidth bw = 0)
      : op(operation), bitwidth(bw) {}
};

struct JitBlock {
  std::string name;
  std::vector<JitInstr> instrs;
  uint32_t node_count = 0;

  explicit JitBlock(const std::string &n) : name(n), node_count(0) {}
};

struct JitFunction {
  std::string name;
  std::vector<JitBlock> blocks;
  uint32_t node_count = 0;
  uint32_t vreg_count = 0;

  explicit JitFunction(const std::string &n) : name(n) {}
};

inline JitInstr make_load(VRegId dst, NodeId node_id, BitWidth bw) {
  JitInstr i(JitOp::LOAD_DATA, bw);
  i.dst = dst;
  i.node_id = node_id;
  return i;
}

inline JitInstr make_store(NodeId node_id, VRegId src, BitWidth bw) {
  JitInstr i(JitOp::STORE_DATA, bw);
  i.src0 = src;
  i.node_id = node_id;
  return i;
}

inline JitInstr make_binary(JitOp op, VRegId dst, VRegId src0, VRegId src1,
                            BitWidth bw) {
  JitInstr i(op, bw);
  i.dst = dst;
  i.src0 = src0;
  i.src1 = src1;
  return i;
}

inline JitInstr make_unary(JitOp op, VRegId dst, VRegId src, BitWidth bw) {
  JitInstr i(op, bw);
  i.dst = dst;
  i.src0 = src;
  return i;
}

inline JitInstr make_const(VRegId dst, uint64_t value, BitWidth bw) {
  JitInstr i(JitOp::LOAD_CONST, bw);
  i.dst = dst;
  i.imm = ImmValue(value, bw);
  return i;
}

inline JitInstr make_select(VRegId dst, VRegId cond, VRegId src0, VRegId src1,
                            BitWidth bw) {
  JitInstr i(JitOp::SELECT, bw);
  i.dst = dst;
  i.src0 = cond;
  i.src1 = src0;
  i.src2 = src1;
  return i;
}

inline JitInstr make_reg_next(VRegId dst, NodeId node_id, BitWidth bw) {
  JitInstr i(JitOp::REG_NEXT, bw);
  i.dst = dst;
  i.node_id = node_id;
  return i;
}

} // namespace jit
} // namespace ch
