module top (

);

    output [3:0] top_counter1_io;
    output [3:0] top_io;
    wire [3:0] top__io;
    wire  top_counter1_ch_lit;
    wire [3:0] top_counter1__reg;
    wire [3:0] top_counter1__io;
    wire  top_counter1_ch_lit_1;
    wire [4:0] top_counter1_add_add;
    wire [4:0] top_counter1__add_add;
    reg [3:0] top_counter1_reg;
    always @(posedge default_clock) begin // Register update for top_counter1_reg
        top_counter1_reg <= top_counter1__add_add;
    end

    reg [3:0] top_counter1_reg;
    always @(posedge default_clock) begin // Register update for top_counter1_reg
        top_counter1_reg <= top_counter1__add_add;
    end
    assign top_counter1__reg = top_counter1_reg;
    assign top_counter1_io = top_counter1__reg;
    assign top_io = top_counter1_io;
    assign top_counter1_add_add = top_counter1__reg + 1'b1;
    assign top_counter1__add_add = top_counter1_add_add;

endmodule
