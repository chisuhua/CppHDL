// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#rv32i-official-rtl
//
// **官方 riscv-tests 套件的「交付物级」TB**（Verilog 侧）
//
// ## 判据是什么
//
// 跑**上游** riscv-tests 的物理内存（`p`）测试集，判定「官方 env 写 `tohost` 的值」：
//
// | `tohost` | 含义 |
// |---|---|
// | `1` | **PASS** |
// | `TESTNUM \| 1`（≥ 3） | **FAIL**（`TESTNUM` = 被判失败的用例号）
//
// ⇒ ⇒ 判据口径来自**上游 env 的协议**，不是本仓自拟 ⇒ ⇒ 与核级门禁
// （`examples/riscv-mini/tests/test_rv32i_official_suite.cpp`）**同源**。
//
// ## 与既有 `tb_rv32i_soc.sv` 的分工（**不要混淆**）
//
// | TB | 激励 | 判据 | 跑什么 |
// |---|---|---|---|
// | `tb_rv32i_soc.sv` | **固定**（`load_word` 写死 7 个字） | **24 条手推判据** | 本仓自写程序 |
// | **本 TB** | **镜像无关**（镜像在**产物**的 `initial` 块里 ⇒ TB **不装载**） | **上游 `tohost` 协议** | 官方 riscv-tests |
//
// ⇒ ⇒ 本 TB **不写程序**：`--emit-rtl --firmware <镜像目录>` 已把镜像放进产物的
// `initial` 块（实测 `top_itcm_itcm_memory[0] = 32'h93;` 对应镜像首字 `00000093`）
// ⇒ ⇒ 一个 TB 跑 40 个镜像。
//
// ## 怎么观察 `tohost`
//
// `tohost` 在 D-TCM 的**首字**（`0x8001_0000`，见 `tests/riscv-tests/PROVENANCE.md` §2.2）
// ⇒ ⇒ 核**自己**写它 ⇒ ⇒ 观察核的**既有**根端口上的 **store 提交事件**
// （`top_obs_data_write_en` / `top_obs_data_addr` / `top_obs_data_write_data`）
// ⇒ ⇒ **零新增端口**（53 端口契约表与 C6 快照不动）。
//
// ## 期望值从哪来（`+tohost_expected`）
//
// ⚠️ **TB 是纯观测器 + 协议判定器**：期望值由**门禁脚本**用 plusarg 给
// （`+tohost_expected=1` ⇒ PASS 形态 · `+tohost_expected=3` ⇒ 负向对照的 FAIL 形态）
// ⇒ ⇒ 默认 `1`。
// ⇒ ⇒ **负向对照**（手写镜像写 `3`）⇒ ⇒ TB MUST 如实读到 `3` ⇒ ⇒ 绿；
// 若读到 `1` ⇒ ⇒ **红**（判定被吞 ⇒ 恒绿）⇒ ⇒ 这是判别力的机械判据。
//
// ## 复位与拍数
//
// · 复位序列与既有 `tb_rv32i_soc.sv` **同形**（`rst=1` + `ext_rst=1` 保持 **4 拍** ⇒ 释放）
//   ⇒ ⇒ 装载固件期 MUST 保持 `ext_rst = 1`（否则计数器被装载污染 —— IP 集成契约）；
// · 拍数上限 **4000**（与核级门禁的 `kMaxCycles` **同值** ⇒ ⇒ 两后端口径一致）
//   ⇒ ⇒ 实测（40 个官方镜像 · Verilator，**`tohost` 写入拍数**）= **41 ~ 1552**
//   （最坏 = `rv32ui-p-ld_st` 的 1552）⇒ ⇒ 余量 **≈ 61%**
//   ⇒ ⇒ 若某测试在上限内**没写** `tohost` ⇒ ⇒ **红**（fail-closed，不静默截断）。
//
// ⚠️ **不写 `timescale`** ⇒ ⇒ 不触发 Verilator 的 `TIMESCALEMOD`
// （那会污染「产物 lint 是否干净」的判定 —— 告警来自 TB 而非产物）。

module tb_rv32i_official;

    // ── 拍数上限（与核级门禁同值）────────────────────────────────────
    localparam integer MAXC = 4000;

    // ── `tohost` 地址（D-TCM 区首 · 链接布局的固定地址）──────────────
    // ⚠️ **单一真相** = `examples/riscv-mini/tests/riscv-tests/env/link.ld`
    //    （`.tohost : { *(.tohost) }` @ `0x80010000`）⇒ ⇒ 与 `PROVENANCE.md` §2.2 一致。
    localparam logic [31:0] TOHOST_ADDR = 32'h8001_0000;

    logic clk = 0;
    logic rst = 1'b1;            // 框架复位（`default_reset`，高有效）
    logic ext_rst = 1'b1;        // 核内复位（`top_ext_rst`，**高有效**）

    logic        prog_we = 1'b0; // ⚠️ **不装载** ⇒ ⇒ 恒 0（镜像在产物里）
    logic [31:0] prog_addr = 32'h0;
    logic [31:0] prog_data = 32'h0;

    logic [31:0] obs_data_addr;
    logic [31:0] obs_data_write_data;
    logic        obs_data_write_en;
    logic [47:0] obs_perf_cycles;
    logic [47:0] obs_perf_instructions;

    logic [31:0] dbg_id_instr;
    logic [31:0] dbg_id_pc;
    logic        dbg_id_valid;
    logic        dbg_wb_write_en;
    logic [31:0] dbg_wb_write_data;
    logic [4:0]  dbg_wb_rd_addr;

    // ── S4a（G-9）：AXI4-Lite 主口 —— 核级 TB **无 MMIO 从设备** ⇒ ⇒
    //    8 根输入 **tie 0**（与 C++ 侧 `tie_axil_inputs` 逐项同值）
    logic        axil_awready = 1'b0;
    logic        axil_wready  = 1'b0;
    logic [1:0]  axil_bresp   = 2'b00;
    logic        axil_bvalid  = 1'b0;
    logic        axil_arready = 1'b0;
    logic [31:0] axil_rdata   = 32'h0;
    logic [1:0]  axil_rresp   = 2'b00;
    logic        axil_rvalid  = 1'b0;

    logic [95:0] sink_axil32;    // 3 个未用输出（宽度 32：awaddr / wdata / araddr）
    logic [12:0] sink_axil_small;// 8 个未用输出（2+1+4+1+1+2+1+1 = 13 位）

    logic [6:0] sink1;   // 7 个未用输出（宽度 1）
    logic [1:0] sink2;   // 1 个未用输出（宽度 2）
    logic [287:0] sink32;   // 9 个未用输出（宽度 32）

    integer checks = 0;
    integer fails  = 0;

    // ── 观测（`tohost` 的写事件）─────────────────────────────────────
    logic        saw_tohost = 1'b0;
    logic [31:0] tohost_val = 32'h0;
    integer      tohost_cycle = 0;
    integer      cycle = 0;

    // ── 期望值（由门禁脚本用 plusarg 给 · 默认 1 = PASS 形态）─────────
    logic [31:0] expected = 32'h1;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_ext_rst(ext_rst),
        .top_prog_we(prog_we),
        .top_prog_addr(prog_addr),
        .top_prog_data(prog_data),
        .top_obs_instr_addr(sink32[0 +: 32]),
        .top_obs_data_addr(obs_data_addr),
        .top_obs_data_write_data(obs_data_write_data),
        .top_obs_data_write_en(obs_data_write_en),
        .top_obs_perf_cycles(obs_perf_cycles),
        .top_obs_perf_instructions(obs_perf_instructions),
        .top_obs_perf_branch_count(sink32[32 +: 32]),
        .top_obs_perf_branch_mispredict(sink32[64 +: 32]),
        .top_obs_perf_load_use_stalls(sink32[96 +: 32]),
        .top_obs_perf_control_stalls(sink32[128 +: 32]),
        .top_dbg_id_instr(dbg_id_instr),
        .top_dbg_id_pc(dbg_id_pc),
        .top_dbg_id_valid(dbg_id_valid),
        .top_dbg_id_is_branch(sink1[0 +: 1]),
        .top_dbg_id_is_load(sink1[1 +: 1]),
        .top_dbg_id_is_store(sink1[2 +: 1]),
        .top_dbg_branch_taken(sink1[3 +: 1]),
        .top_dbg_wb_write_en(dbg_wb_write_en),
        .top_dbg_wb_write_data(dbg_wb_write_data),
        .top_dbg_wb_rd_addr(dbg_wb_rd_addr),
        .top_dbg_exmem_alu_result(sink32[160 +: 32]),
        .top_dbg_exmem_imm(sink32[192 +: 32]),
        .top_dbg_exmem_rs2_data(sink32[224 +: 32]),
        .top_obs_data_rdata(sink32[256 +: 32]),
        .top_dbg_exmem_valid(sink1[4 +: 1]),
        .top_dbg_mem_valid_out(sink1[5 +: 1]),
        .top_dbg_load_use_detected(sink1[6 +: 1]),
        .top_dbg_illegal_reason(sink2[0 +: 2]),
        // ── S4a：AXI4-Lite 主口（11 输出 → sink · 8 输入 ← tie 0）──
        .top_axil_awaddr(sink_axil32[0 +: 32]),
        .top_axil_awprot(sink_axil_small[0 +: 2]),
        .top_axil_awvalid(sink_axil_small[2]),
        .top_axil_awready(axil_awready),
        .top_axil_wdata(sink_axil32[32 +: 32]),
        .top_axil_wstrb(sink_axil_small[3 +: 4]),
        .top_axil_wvalid(sink_axil_small[7]),
        .top_axil_wready(axil_wready),
        .top_axil_bresp(axil_bresp),
        .top_axil_bvalid(axil_bvalid),
        .top_axil_bready(sink_axil_small[8]),
        .top_axil_araddr(sink_axil32[64 +: 32]),
        .top_axil_arprot(sink_axil_small[9 +: 2]),
        .top_axil_arvalid(sink_axil_small[11]),
        .top_axil_arready(axil_arready),
        .top_axil_rdata(axil_rdata),
        .top_axil_rresp(axil_rresp),
        .top_axil_rvalid(axil_rvalid),
        .top_axil_rready(sink_axil_small[12])
    );

    always #5 clk = ~clk;

    // ── 观测 `tohost` 的写（store 提交事件）────────────────────────────
    always @(posedge clk) begin
        if (!rst) begin
            cycle = cycle + 1;
            if (obs_data_write_en === 1'b1) begin
                if (obs_data_addr == TOHOST_ADDR) begin
                    saw_tohost  = 1'b1;
                    tohost_val  = obs_data_write_data;
                    tohost_cycle = cycle;
                end
            end
        end
    end

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq32(input logic [31:0] got, input logic [31:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    initial begin
        // ── 期望值（plusarg · 默认 1）───────────────────────────────
        void'($value$plusargs("tohost_expected=%x", expected));

        // ── 复位 + 释放（装载固件期 MUST 保持 `ext_rst = 1`）──────────
        rst     = 1'b1;
        ext_rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst     = 1'b0;
        ext_rst = 1'b0;

        // ── 跑（上限 `MAXC` 拍）─────────────────────────────────────
        repeat (MAXC) @(posedge clk);
        #1;

        // ══════════════════════════════════════════════════════════════
        // O1：**必须看到** `tohost` 的写
        //   ⚠️ 看不到 ⇒ ⇒ **fail-closed**（不是 skip · 不是「空洞通过」）
        // ══════════════════════════════════════════════════════════════
        expect_bit(saw_tohost, 1'b1);

        // ══════════════════════════════════════════════════════════════
        // O2：`tohost` 的值 MUST 等于期望值
        //   ⇒ ⇒ 正向：`1`（PASS）· 负向对照：`3`（FAIL 形态）
        //   ⇒ ⇒ **读到 1 却期望 3 ⇒ ⇒ 红** ⇒ ⇒ 判定不是恒绿
        // ══════════════════════════════════════════════════════════════
        expect_eq32(tohost_val, expected);

        // ── 机器可读的观测行（门禁脚本按字段解析）────────────────────
        $display("TOHOST: seen=%0d value=0x%08x expected=0x%08x cycle=%0d retired=%0d",
                 saw_tohost, tohost_val, expected, tohost_cycle, obs_perf_instructions);
        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        $finish;
    end

endmodule
