module VulQueue #(
    parameter int unsigned Width = 32,
    parameter int unsigned Depth = 4
) (
    input  logic             clk,
    input  logic             rstn,

    output logic             enqready,  // 本周期是否能接受enqnext()调用
    output logic             deqready,  // 本周期是否能接受deqnext()调用

    input  logic             enqnext_vld,   // enq一个元素，在下个周期生效
    input  logic [Width-1:0] enqnext_data,

    input  logic             deqnext_vld,   // deq一个元素，在下个周期生效
    output logic [Width-1:0] deqnext_data,

    input  logic             clrnext        // 清空队列，在下个周期生效
);

localparam int unsigned PtrW  = (Depth <= 1) ? 1 : $clog2(Depth);
localparam int unsigned SizeW = $clog2(Depth + 1);

logic [Width-1:0] data_q [0:Depth-1];
logic [PtrW-1:0]  head_q;
logic [PtrW-1:0]  tail_q;
logic [SizeW-1:0] size_q;
logic [Width-1:0] deq_buf_q;
logic             deq_buf_valid_q;

logic [Width-1:0] data_d [0:Depth-1];
logic [PtrW-1:0]  head_d;
logic [PtrW-1:0]  tail_d;
logic [SizeW-1:0] size_d;
logic [Width-1:0] deq_buf_d;
logic             deq_buf_valid_d;

always_comb begin
    int unsigned head_n;
    int unsigned tail_n;
    int unsigned size_n;
    bit          enq_accepted;

    for (int i = 0; i < Depth; i = i + 1) begin
        data_d[i] = data_q[i];
    end
    head_d = head_q;
    tail_d = tail_q;
    size_d = size_q;
    deq_buf_d = deq_buf_q;
    deq_buf_valid_d = deq_buf_valid_q;

    head_n = int'(head_q);
    tail_n = int'(tail_q);
    size_n = int'(size_q);

    // enqnext() acceptance is decided in current-cycle visible state.
    enq_accepted = enqnext_vld && (int'(size_q) < Depth);

    // apply_next_tick(): clr -> deq -> enq.
    if (clrnext) begin
        head_n = 0;
        tail_n = 0;
        size_n = 0;
    end

    if (deqnext_vld && (size_n > 0)) begin
        if (head_n + 1 == Depth) begin
            head_n = 0;
        end else begin
            head_n = head_n + 1;
        end
        size_n = size_n - 1;
    end

    if (enq_accepted && (size_n < Depth)) begin
        data_d[tail_n] = enqnext_data;
        if (tail_n + 1 == Depth) begin
            tail_n = 0;
        end else begin
            tail_n = tail_n + 1;
        end
        size_n = size_n + 1;
    end

    head_d = head_n[PtrW-1:0];
    tail_d = tail_n[PtrW-1:0];
    size_d = size_n[SizeW-1:0];

    if (size_n > 0) begin
        deq_buf_d = data_d[head_n];
        deq_buf_valid_d = 1'b1;
    end else begin
        deq_buf_valid_d = 1'b0;
    end
end

always_ff @(posedge clk or negedge rstn) begin
    if (!rstn) begin
        for (int i = 0; i < Depth; i = i + 1) begin
            data_q[i] <= '0;
        end
        head_q <= '0;
        tail_q <= '0;
        size_q <= '0;
        deq_buf_q <= '0;
        deq_buf_valid_q <= 1'b0;
    end else begin
        for (int i = 0; i < Depth; i = i + 1) begin
            data_q[i] <= data_d[i];
        end
        head_q <= head_d;
        tail_q <= tail_d;
        size_q <= size_d;
        deq_buf_q <= deq_buf_d;
        deq_buf_valid_q <= deq_buf_valid_d;
    end
end

always_comb begin
    enqready = (int'(size_q) < Depth);
    deqready = deq_buf_valid_q;
    deqnext_data = deq_buf_q;
end

endmodule


module VulQueueMP #(
    parameter int unsigned Width = 32,
    parameter int unsigned Depth = 4,
    parameter int unsigned EnqWidth = 2,
    parameter int unsigned DeqWidth = 2
) (
    input  logic                               clk,
    input  logic                               rstn,

    output logic [$clog2(EnqWidth+1)-1:0]     enqready,     // 本周期能接受的enq数量
    output logic [$clog2(DeqWidth+1)-1:0]     deqready,     // 本周期能接受的deq数量

    input  logic [$clog2(EnqWidth+1)-1:0]     enqnext_vld,                  // 本周期需要enq的数量，在下个周期生效，值被截断到enqready
    input  logic [Width-1:0]                  enqnext_data [0:EnqWidth-1],

    input  logic [$clog2(DeqWidth+1)-1:0]     deqnext_vld,                  // 本周期需要deq的数量，在下个周期生效，值被截断到deqready
    output logic [Width-1:0]                  deqnext_data [0:DeqWidth-1],

    input  logic                               clrnext        // 清空队列，在下个周期生效
);

localparam int unsigned PtrW     = (Depth <= 1) ? 1 : $clog2(Depth);
localparam int unsigned SizeW    = $clog2(Depth + 1);
localparam int unsigned EnqCntW  = $clog2(EnqWidth + 1);
localparam int unsigned DeqCntW  = $clog2(DeqWidth + 1);

logic [Width-1:0] data_q [0:Depth-1];
logic [PtrW-1:0]  head_q;
logic [PtrW-1:0]  tail_q;
logic [SizeW-1:0] size_q;

logic [Width-1:0] deq_buf_q [0:DeqWidth-1];
logic [DeqCntW-1:0] deq_valid_num_q;

logic [Width-1:0] data_d [0:Depth-1];
logic [PtrW-1:0]  head_d;
logic [PtrW-1:0]  tail_d;
logic [SizeW-1:0] size_d;

logic [Width-1:0] deq_buf_d [0:DeqWidth-1];
logic [DeqCntW-1:0] deq_valid_num_d;

always_comb begin
    int unsigned head_n;
    int unsigned tail_n;
    int unsigned size_n;
    int unsigned idx_n;

    int unsigned free_slots_pre;
    int unsigned enq_req;
    int unsigned enq_rdy_pre;
    int unsigned enq_accepted;

    int unsigned deq_req;
    int unsigned deq_pending_num;
    int unsigned pop_num;

    int unsigned can_push;
    int unsigned push_num;

    for (int i = 0; i < Depth; i = i + 1) begin
        data_d[i] = data_q[i];
    end
    for (int i = 0; i < DeqWidth; i = i + 1) begin
        deq_buf_d[i] = deq_buf_q[i];
    end
    head_d = head_q;
    tail_d = tail_q;
    size_d = size_q;
    deq_valid_num_d = deq_valid_num_q;

    head_n = int'(head_q);
    tail_n = int'(tail_q);
    size_n = int'(size_q);

    free_slots_pre = Depth - int'(size_q);

    enq_req = int'(enqnext_vld);
    if (enq_req > EnqWidth) begin
        enq_req = EnqWidth;
    end

    enq_rdy_pre = free_slots_pre;
    if (enq_rdy_pre > EnqWidth) begin
        enq_rdy_pre = EnqWidth;
    end

    enq_accepted = (enq_req < enq_rdy_pre) ? enq_req : enq_rdy_pre;

    deq_req = int'(deqnext_vld);
    if (deq_req > DeqWidth) begin
        deq_req = DeqWidth;
    end
    deq_pending_num = (deq_req < int'(deq_valid_num_q)) ? deq_req : int'(deq_valid_num_q);

    // apply_next_tick(): clr -> deq -> enq.
    if (clrnext) begin
        head_n = 0;
        tail_n = 0;
        size_n = 0;
    end

    pop_num = (deq_pending_num < size_n) ? deq_pending_num : size_n;
    while (pop_num > 0) begin
        if (head_n + 1 == Depth) begin
            head_n = 0;
        end else begin
            head_n = head_n + 1;
        end
        size_n = size_n - 1;
        pop_num = pop_num - 1;
    end

    can_push = Depth - size_n;
    push_num = (enq_accepted < can_push) ? enq_accepted : can_push;
    for (int i = 0; i < push_num; i = i + 1) begin
        data_d[tail_n] = enqnext_data[i];
        if (tail_n + 1 == Depth) begin
            tail_n = 0;
        end else begin
            tail_n = tail_n + 1;
        end
        size_n = size_n + 1;
    end

    head_d = head_n[PtrW-1:0];
    tail_d = tail_n[PtrW-1:0];
    size_d = size_n[SizeW-1:0];

    deq_valid_num_d = (size_n < DeqWidth) ? size_n[DeqCntW-1:0] : DeqWidth[DeqCntW-1:0];
    idx_n = head_n;
    for (int i = 0; i < deq_valid_num_d; i = i + 1) begin
        deq_buf_d[i] = data_d[idx_n];
        if (idx_n + 1 == Depth) begin
            idx_n = 0;
        end else begin
            idx_n = idx_n + 1;
        end
    end
end

always_ff @(posedge clk or negedge rstn) begin
    if (!rstn) begin
        for (int i = 0; i < Depth; i = i + 1) begin
            data_q[i] <= '0;
        end
        for (int i = 0; i < DeqWidth; i = i + 1) begin
            deq_buf_q[i] <= '0;
        end
        head_q <= '0;
        tail_q <= '0;
        size_q <= '0;
        deq_valid_num_q <= '0;
    end else begin
        for (int i = 0; i < Depth; i = i + 1) begin
            data_q[i] <= data_d[i];
        end
        for (int i = 0; i < DeqWidth; i = i + 1) begin
            deq_buf_q[i] <= deq_buf_d[i];
        end
        head_q <= head_d;
        tail_q <= tail_d;
        size_q <= size_d;
        deq_valid_num_q <= deq_valid_num_d;
    end
end

always_comb begin
    int unsigned free_slots;
    logic [EnqCntW-1:0] enq_rdy_cnt;

    free_slots = Depth - int'(size_q);
    enq_rdy_cnt = (free_slots < EnqWidth) ? EnqCntW'(free_slots) : EnqCntW'(EnqWidth);

    enqready = enq_rdy_cnt;

    if (deqnext_vld != '0) begin
        deqready = '0;
    end else begin
        deqready = deq_valid_num_q;
    end

    for (int i = 0; i < DeqWidth; i = i + 1) begin
        deqnext_data[i] = deq_buf_q[i];
    end
end

endmodule
