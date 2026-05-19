module VulBRAM1RW #(
    parameter DataWidth = 32,
    parameter AddrWidth = 10
) (
    input wire clk,
    input wire rstn,

    input wire s1_en,
    input wire s1_we,
    input wire [AddrWidth-1:0] s1_addr,
    input wire [DataWidth-1:0] s1_wdata,

    output wire [DataWidth-1:0] s2_rdata
);

localparam int unsigned Depth = (1 << AddrWidth);

logic [DataWidth-1:0] memory [0:Depth-1];

logic [AddrWidth-1:0] req_addr_q;
logic [DataWidth-1:0] req_wdata_q;
logic                 req_we_q;
logic [DataWidth-1:0] read_data_q;

assign s2_rdata = read_data_q;

always_ff @(posedge clk or negedge rstn) begin
    if (!rstn) begin
        req_addr_q <= '0;
        req_wdata_q <= '0;
        req_we_q <= 1'b0;
        read_data_q <= '0;
    end else begin
        logic [AddrWidth-1:0] req_addr_n;
        logic [DataWidth-1:0] req_wdata_n;
        logic                 req_we_n;

        req_addr_n = req_addr_q;
        req_wdata_n = req_wdata_q;
        req_we_n = req_we_q;

        if (s1_en) begin
            req_addr_n = s1_addr;
            req_wdata_n = s1_wdata;
            req_we_n = s1_we;
        end

        req_addr_q <= req_addr_n;
        req_wdata_q <= req_wdata_n;
        req_we_q <= req_we_n;

        // VulBRAM1RW::apply_next_tick(): write then read.
        if (req_we_n) begin
            memory[req_addr_n] <= req_wdata_n;
            read_data_q <= req_wdata_n;
        end else begin
            read_data_q <= memory[req_addr_n];
        end
    end
end

endmodule


module VulBRAM #(
    parameter DataWidth = 32,
    parameter AddrWidth = 10,
    parameter ReadPorts = 2,
    parameter WritePorts = 1
) (
    input wire clk,
    input wire rstn,

    input wire                  s1_readreq [ReadPorts],
    input wire [AddrWidth-1:0]  s1_readaddr [ReadPorts],

    output wire [DataWidth-1:0] s2_readdata [ReadPorts],

    input wire                  s1_write [WritePorts],
    input wire [AddrWidth-1:0]  s1_writeaddr [WritePorts],
    input wire [DataWidth-1:0]  s1_writedata [WritePorts]
);

localparam int unsigned Depth = (1 << AddrWidth);

logic [DataWidth-1:0] memory [0:Depth-1];

logic [AddrWidth-1:0] read_addr_q [ReadPorts];
logic [AddrWidth-1:0] read_addr_d [ReadPorts];
logic [DataWidth-1:0] read_data_q [ReadPorts];

assign s2_readdata = read_data_q;

always_comb begin
    for (int i = 0; i < ReadPorts; i = i + 1) begin
        read_addr_d[i] = read_addr_q[i];
        if (s1_readreq[i]) begin
            read_addr_d[i] = s1_readaddr[i];
        end
    end
end

always_ff @(posedge clk or negedge rstn) begin
    if (!rstn) begin
        for (int i = 0; i < ReadPorts; i = i + 1) begin
            read_addr_q[i] <= '0;
            read_data_q[i] <= '0;
        end
    end else begin
        for (int i = 0; i < ReadPorts; i = i + 1) begin
            read_addr_q[i] <= read_addr_d[i];
            // VulBRAM::apply_next_tick(): read then write.
            read_data_q[i] <= memory[read_addr_d[i]];
        end

        // Larger write-port index has higher priority for same address.
        for (int i = 0; i < WritePorts; i = i + 1) begin
            if (s1_write[i]) begin
                memory[s1_writeaddr[i]] <= s1_writedata[i];
            end
        end
    end
end

endmodule


module VulROM #(
    parameter DataWidth = 32,
    parameter AddrWidth = 10,
    parameter ReadPorts = 2,
    parameter string ReadMemHPath = ""
) (
    input wire clk,
    input wire rstn,

    input wire                  s1_readreq [ReadPorts],
    input wire [AddrWidth-1:0]  s1_readaddr [ReadPorts],

    output wire [DataWidth-1:0] s2_readdata [ReadPorts]
);

localparam int unsigned Depth = (1 << AddrWidth);

logic [DataWidth-1:0] memory [0:Depth-1];

logic [AddrWidth-1:0] read_addr_q [ReadPorts];
logic [AddrWidth-1:0] read_addr_d [ReadPorts];
logic [DataWidth-1:0] read_data_q [ReadPorts];

assign s2_readdata = read_data_q;

initial begin
    if (ReadMemHPath != "") begin
        $readmemh(ReadMemHPath, memory);
    end
end

always_comb begin
    for (int i = 0; i < ReadPorts; i = i + 1) begin
        read_addr_d[i] = read_addr_q[i];
        if (s1_readreq[i]) begin
            read_addr_d[i] = s1_readaddr[i];
        end
    end
end

always_ff @(posedge clk or negedge rstn) begin
    if (!rstn) begin
        for (int i = 0; i < ReadPorts; i = i + 1) begin
            read_addr_q[i] <= '0;
            read_data_q[i] <= '0;
        end
    end else begin
        for (int i = 0; i < ReadPorts; i = i + 1) begin
            read_addr_q[i] <= read_addr_d[i];
            read_data_q[i] <= memory[read_addr_d[i]];
        end
    end
end

endmodule
