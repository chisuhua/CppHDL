module top (
    input [31:0] top_io,
    input [1:0] top_io_1,
    input  top_io_2,
    input [31:0] top_io_4,
    input [3:0] top_io_5,
    input  top_io_6,
    input  top_io_10,
    input [31:0] top_io_11,
    input [1:0] top_io_12,
    input  top_io_13,
    input  top_io_18
);

    input [31:0] top_io;
    input [1:0] top_io_1;
    input  top_io_2;
    input [31:0] top_io_4;
    input [3:0] top_io_5;
    input  top_io_6;
    input  top_io_10;
    input [31:0] top_io_11;
    input [1:0] top_io_12;
    input  top_io_13;
    input  top_io_18;
    output  top_io_3;
    output  top_io_7;
    output [1:0] top_io_8;
    output  top_io_9;
    output  top_io_14;
    output [31:0] top_io_15;
    output [1:0] top_io_16;
    output  top_io_17;
    output  top_io_19;
    output  top_io_20;
    output  top_io_21;
    output  top_io_22;
    output  top_io_23;
    wire [31:0] top__io;
    wire [1:0] top__io_1;
    wire  top__io_2;
    wire  top__io_3;
    wire  top_bool_lit;
    wire  top__reg;
    wire  top_bool_lit_1;
    wire  top_bool_lit_2;
    wire  top_bool_lit_3;
    wire [31:0] top__io_4;
    wire [3:0] top__io_5;
    wire  top__io_6;
    wire  top__io_7;
    wire  top_bool_lit_4;
    wire [1:0] top__io_8;
    wire [1:0] top_uint_lit;
    wire  top__io_9;
    wire  top__io_10;
    wire [31:0] top__io_11;
    wire [1:0] top__io_12;
    wire  top__io_13;
    wire  top__io_14;
    wire  top_bool_lit_5;
    wire  top_bool_lit_6;
    wire  top_bool_lit_7;
    wire [31:0] top__io_15;
    wire [31:0] top_uint_lit_1;
    wire  top_eq_eq;
    wire  top__eq_eq;
    wire [31:0] top_uint_lit_2;
    wire [31:0] top__reg_1;
    wire [31:0] top_uint_lit_3;
    wire  top_eq_eq_1;
    wire  top__eq_eq_1;
    wire [31:0] top_uint_lit_4;
    wire [31:0] top__reg_2;
    wire [31:0] top_uint_lit_5;
    wire  top_eq_eq_2;
    wire  top__eq_eq_2;
    wire [31:0] top_uint_lit_6;
    wire [31:0] top__reg_3;
    wire [31:0] top_uint_lit_7;
    wire  top_eq_eq_3;
    wire  top__eq_eq_3;
    wire [31:0] top_uint_lit_8;
    wire [31:0] top__reg_4;
    wire [31:0] top_uint_lit_9;
    wire  top_eq_eq_4;
    wire  top__eq_eq_4;
    wire [31:0] top_uint_lit_10;
    wire [31:0] top__reg_5;
    wire [31:0] top_uint_lit_11;
    wire  top_eq_eq_5;
    wire  top__eq_eq_5;
    wire [31:0] top_uint_lit_12;
    wire [31:0] top__reg_6;
    wire [31:0] top_uint_lit_13;
    wire  top_eq_eq_6;
    wire  top__eq_eq_6;
    wire [31:0] top_uint_lit_14;
    wire [31:0] top__reg_7;
    wire [31:0] top_uint_lit_15;
    wire [1:0] top__io_16;
    wire [1:0] top_uint_lit_16;
    wire  top__io_17;
    wire  top__io_18;
    wire  top__io_19;
    wire [31:0] top_uint_lit_17;
    wire [31:0] top_and_and;
    wire [31:0] top__and_and;
    wire [31:0] top_uint_lit_18;
    wire  top_ne_ne;
    wire  top__ne_ne;
    wire  top_ch_lit;
    wire [15:0] top__reg_8;
    wire [63:0] top_bits_range;
    wire [15:0] top_bits;
    wire  top_lt_lt;
    wire  top__lt_lt;
    wire  top_bool_lit_8;
    wire  top__io_20;
    wire [31:0] top_uint_lit_19;
    wire [31:0] top_and_and_1;
    wire [31:0] top__and_and_1;
    wire [31:0] top_uint_lit_20;
    wire  top_ne_ne_1;
    wire  top__ne_ne_1;
    wire [63:0] top_bits_range_1;
    wire [15:0] top_bits_1;
    wire  top_lt_lt_1;
    wire  top__lt_lt_1;
    wire  top_bool_lit_9;
    wire  top__io_21;
    wire [31:0] top_uint_lit_21;
    wire [31:0] top_and_and_2;
    wire [31:0] top__and_and_2;
    wire [31:0] top_uint_lit_22;
    wire  top_ne_ne_2;
    wire  top__ne_ne_2;
    wire [63:0] top_bits_range_2;
    wire [15:0] top_bits_2;
    wire  top_lt_lt_2;
    wire  top__lt_lt_2;
    wire  top_bool_lit_10;
    wire  top__io_22;
    wire [31:0] top_uint_lit_23;
    wire [31:0] top_and_and_3;
    wire [31:0] top__and_and_3;
    wire [31:0] top_uint_lit_24;
    wire  top_ne_ne_3;
    wire  top__ne_ne_3;
    wire [63:0] top_bits_range_3;
    wire [15:0] top_bits_3;
    wire  top_lt_lt_3;
    wire  top__lt_lt_3;
    wire  top_bool_lit_11;
    wire  top__io_23;
    wire [31:0] top_uint_lit_25;
    wire [31:0] top_and_and_4;
    wire [31:0] top__and_and_4;
    wire [31:0] top_uint_lit_26;
    wire  top_ne_ne_4;
    wire  top__ne_ne_4;
    wire [31:0] top_uint_lit_27;
    wire  top_eq_eq_7;
    wire  top__eq_eq_7;
    wire [31:0] top_uint_lit_28;
    wire  top_eq_eq_8;
    wire  top__eq_eq_8;
    wire [31:0] top_uint_lit_29;
    wire  top_eq_eq_9;
    wire  top__eq_eq_9;
    wire [31:0] top_uint_lit_30;
    wire  top_eq_eq_10;
    wire  top__eq_eq_10;
    wire [31:0] top_uint_lit_31;
    wire  top_eq_eq_11;
    wire  top__eq_eq_11;
    wire [31:0] top_uint_lit_32;
    wire  top_eq_eq_12;
    wire  top__eq_eq_12;
    wire [31:0] top_uint_lit_33;
    wire  top_eq_eq_13;
    wire  top__eq_eq_13;
    wire  top_bool_lit_12;
    wire  top_bool_lit_13;
    wire  top_bool_lit_14;
    wire  top_bool_lit_15;
    wire  top_bool_lit_16;
    wire  top_bool_lit_17;
    wire  top_bool_lit_18;
    wire [63:0] top_bits_range_4;
    wire [15:0] top_bits_4;
    wire  top_ge_ge;
    wire  top__ge_ge;
    wire [15:0] top_uint_lit_34;
    wire [16:0] top_add_add;
    wire [16:0] top__add_add;
    wire [15:0] top_uint_lit_35;
    wire [31:0] top_uint_lit_36;
    wire [31:0] top_or_or;
    wire [31:0] top__or_or;
    wire  top_bool_lit_19;
    wire  top_bool_lit_20;
    wire  top_bool_lit_21;
    wire  top_bool_lit_22;
    wire  top_bool_lit_23;
    wire  top_bool_lit_24;
    reg  top_reg;
    always @(posedge default_clock) begin // Register update for top_reg
        top_reg <= top_mux_select_36;
    end
    reg [31:0] top_reg_1;
    always @(posedge default_clock) begin // Register update for top_reg_1
        top_reg_1 <= top_mux_select_31;
    end
    reg [31:0] top_reg_2;
    always @(posedge default_clock) begin // Register update for top_reg_2
        top_reg_2 <= top_mux_select_28;
    end
    reg [31:0] top_reg_3;
    always @(posedge default_clock) begin // Register update for top_reg_3
        top_reg_3 <= top_mux_select_27;
    end
    reg [31:0] top_reg_4;
    always @(posedge default_clock) begin // Register update for top_reg_4
        top_reg_4 <= top_mux_select_26;
    end
    reg [31:0] top_reg_5;
    always @(posedge default_clock) begin // Register update for top_reg_5
        top_reg_5 <= top_mux_select_25;
    end
    reg [31:0] top_reg_6;
    always @(posedge default_clock) begin // Register update for top_reg_6
        top_reg_6 <= top_mux_select_24;
    end
    reg [31:0] top_reg_7;
    always @(posedge default_clock) begin // Register update for top_reg_7
        top_reg_7 <= top_mux_select_23;
    end
    reg [15:0] top_reg_8;
    always @(posedge default_clock) begin // Register update for top_reg_8
        top_reg_8 <= top_mux_select_30;
    end

    assign top__io = top_io;
    assign top__io_1 = top_io_1;
    assign top__io_2 = top_io_2;
    reg  top_reg;
    always @(posedge default_clock) begin // Register update for top_reg
        top_reg <= top_mux_select_36;
    end
    assign top__reg = top_reg;
    assign top_io_3 = top_mux_select_1;
    assign top__io_4 = top_io_4;
    assign top__io_5 = top_io_5;
    assign top__io_6 = top_io_6;
    assign top_io_7 = top_mux_select_2;
    assign top_io_8 = top_uint_lit;
    assign top_io_9 = top_mux_select_2;
    assign top__io_10 = top_io_10;
    assign top__io_11 = top_io_11;
    assign top__io_12 = top_io_12;
    assign top__io_13 = top_io_13;
    assign top_io_14 = top_mux_select_4;
    assign top_eq_eq = top_io_11 == 32'h24;
    assign top__eq_eq = top_eq_eq;
    reg [31:0] top_reg_1;
    always @(posedge default_clock) begin // Register update for top_reg_1
        top_reg_1 <= top_mux_select_31;
    end
    assign top__reg_1 = top_reg_1;
    assign top_eq_eq_1 = top_io_11 == 32'h20;
    assign top__eq_eq_1 = top_eq_eq_1;
    reg [31:0] top_reg_2;
    always @(posedge default_clock) begin // Register update for top_reg_2
        top_reg_2 <= top_mux_select_28;
    end
    assign top__reg_2 = top_reg_2;
    assign top_eq_eq_2 = top_io_11 == 32'h1c;
    assign top__eq_eq_2 = top_eq_eq_2;
    reg [31:0] top_reg_3;
    always @(posedge default_clock) begin // Register update for top_reg_3
        top_reg_3 <= top_mux_select_27;
    end
    assign top__reg_3 = top_reg_3;
    assign top_eq_eq_3 = top_io_11 == 32'h18;
    assign top__eq_eq_3 = top_eq_eq_3;
    reg [31:0] top_reg_4;
    always @(posedge default_clock) begin // Register update for top_reg_4
        top_reg_4 <= top_mux_select_26;
    end
    assign top__reg_4 = top_reg_4;
    assign top_eq_eq_4 = top_io_11 == 32'h14;
    assign top__eq_eq_4 = top_eq_eq_4;
    reg [31:0] top_reg_5;
    always @(posedge default_clock) begin // Register update for top_reg_5
        top_reg_5 <= top_mux_select_25;
    end
    assign top__reg_5 = top_reg_5;
    assign top_eq_eq_5 = top_io_11 == 32'h10;
    assign top__eq_eq_5 = top_eq_eq_5;
    reg [31:0] top_reg_6;
    always @(posedge default_clock) begin // Register update for top_reg_6
        top_reg_6 <= top_mux_select_24;
    end
    assign top__reg_6 = top_reg_6;
    assign top_eq_eq_6 = top_io_11 == 32'h0;
    assign top__eq_eq_6 = top_eq_eq_6;
    reg [31:0] top_reg_7;
    always @(posedge default_clock) begin // Register update for top_reg_7
        top_reg_7 <= top_mux_select_23;
    end
    assign top__reg_7 = top_reg_7;
    assign top_io_15 = top_mux_select_11;
    assign top_io_16 = top_uint_lit_16;
    assign top_io_17 = top_mux_select_4;
    assign top__io_18 = top_io_18;
    assign top_and_and = top__reg_2 & 32'h1;
    assign top__and_and = top_and_and;
    assign top_ne_ne = top__and_and != 32'h0;
    assign top__ne_ne = top_ne_ne;
    reg [15:0] top_reg_8;
    always @(posedge default_clock) begin // Register update for top_reg_8
        top_reg_8 <= top_mux_select_30;
    end
    assign top__reg_8 = top_reg_8;
    assign top_bits = top__reg_6 <OP_NOT_IMPLEMENTED> 64'hf00000000;
    assign top_lt_lt = top__reg_8 < top_bits;
    assign top__lt_lt = top_lt_lt;
    assign top_io_19 = top_mux_select_12;
    assign top_and_and_1 = top__reg_2 & 32'h2;
    assign top__and_and_1 = top_and_and_1;
    assign top_ne_ne_1 = top__and_and_1 != 32'h0;
    assign top__ne_ne_1 = top_ne_ne_1;
    assign top_bits_1 = top__reg_5 <OP_NOT_IMPLEMENTED> 64'hf00000000;
    assign top_lt_lt_1 = top__reg_8 < top_bits_1;
    assign top__lt_lt_1 = top_lt_lt_1;
    assign top_io_20 = top_mux_select_13;
    assign top_and_and_2 = top__reg_2 & 32'h4;
    assign top__and_and_2 = top_and_and_2;
    assign top_ne_ne_2 = top__and_and_2 != 32'h0;
    assign top__ne_ne_2 = top_ne_ne_2;
    assign top_bits_2 = top__reg_4 <OP_NOT_IMPLEMENTED> 64'hf00000000;
    assign top_lt_lt_2 = top__reg_8 < top_bits_2;
    assign top__lt_lt_2 = top_lt_lt_2;
    assign top_io_21 = top_mux_select_14;
    assign top_and_and_3 = top__reg_2 & 32'h8;
    assign top__and_and_3 = top_and_and_3;
    assign top_ne_ne_3 = top__and_and_3 != 32'h0;
    assign top__ne_ne_3 = top_ne_ne_3;
    assign top_bits_3 = top__reg_3 <OP_NOT_IMPLEMENTED> 64'hf00000000;
    assign top_lt_lt_3 = top__reg_8 < top_bits_3;
    assign top__lt_lt_3 = top_lt_lt_3;
    assign top_io_22 = top_mux_select_15;
    assign top_and_and_4 = top__reg_1 & 32'h1;
    assign top__and_and_4 = top_and_and_4;
    assign top_ne_ne_4 = top__and_and_4 != 32'h0;
    assign top__ne_ne_4 = top_ne_ne_4;
    assign top_io_23 = top__ne_ne_4;
    assign top_eq_eq_7 = top_io == 32'h0;
    assign top__eq_eq_7 = top_eq_eq_7;
    assign top_eq_eq_8 = top_io == 32'h10;
    assign top__eq_eq_8 = top_eq_eq_8;
    assign top_eq_eq_9 = top_io == 32'h14;
    assign top__eq_eq_9 = top_eq_eq_9;
    assign top_eq_eq_10 = top_io == 32'h18;
    assign top__eq_eq_10 = top_eq_eq_10;
    assign top_eq_eq_11 = top_io == 32'h1c;
    assign top__eq_eq_11 = top_eq_eq_11;
    assign top_eq_eq_12 = top_io == 32'h20;
    assign top__eq_eq_12 = top_eq_eq_12;
    assign top_eq_eq_13 = top_io == 32'h24;
    assign top__eq_eq_13 = top_eq_eq_13;
    assign top_bits_4 = top__reg_7 <OP_NOT_IMPLEMENTED> 64'hf00000000;
    assign top_ge_ge = top__reg_8 >= top_bits_4;
    assign top__ge_ge = top_ge_ge;
    assign top_add_add = top__reg_8 + 16'h1;
    assign top__add_add = top_add_add;
    assign top_or_or = top__reg_1 | 32'h1;
    assign top__or_or = top_or_or;

endmodule
