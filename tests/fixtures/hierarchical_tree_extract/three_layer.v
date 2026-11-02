// 手写 3 层 SV —— **故意不用 CppHDL 的 emitter 生成**
//
// ⚠️ 为什么必须手写：「结构同构」判据要能**独立于 emitter** 读出 RTL 的 module 树
//    ⇒ 若 fixture 由 emitter 生成 ⇒ 提取器与 emitter 共享同一套假设 ⇒ 判据**自证**
//    （⇒ oracle R1 / critic 第 3 轮 B7 的要求）。
//
// 层级：top_mod → mid_mod → leaf_mod（2 个实例、3 个 module）
module leaf_mod (input clk, input [7:0] a, output [7:0] y);
  reg [7:0] r;
  always @(posedge clk) r <= a;
  assign y = r;
endmodule

module mid_mod (input clk, input [7:0] in, output [7:0] out);
  wire [7:0] leaf_y;
  leaf_mod u_leaf (.clk(clk), .a(in), .y(leaf_y));
  assign out = leaf_y;
endmodule

module top_mod (input clk, input [7:0] a, output [7:0] y);
  wire [7:0] mid_out;
  mid_mod u_mid (.clk(clk), .in(a), .out(mid_out));
  assign y = mid_out;
endmodule
