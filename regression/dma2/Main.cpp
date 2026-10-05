#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <defhelper.hpp>
#include <run.hpp>
#include "header.hpp"
TOP("./TransferTop.hpp");
PROJECT(".");
PARAMETER(BASE_DEPTH, 3);
PARAMETER(RESET_SEED, 0x4A31);
REQUEST(submit0, handshake=1, ARG(DmaCommand) command);
REQUEST(submit1, handshake=1, ARG(DmaCommand) command);
QUERY(snapshot, TopStatus);
GLOBAL() {
    const char *phase = "initial";
    uint32_t tick = 0;
    bool done_ready[2] = {true, true};
    bool expected_done[2] = {};
    bool seen_done[2] = {};
    DmaCompletion expected_completion[2] = {};
    struct ReferenceChannel {
        std::deque<DmaCommand> queue;
        EngineStatus engine{};
        uint32_t enqueued = 0;
        uint32_t popped = 0;
    };
    ReferenceChannel reference[2];
    std::array<uint32_t, 37> memory{};
    AuditInfo expected_audit{};
    uint32_t accesses = 0;
    struct Coverage {
        uint32_t reads[2] = {};
        uint32_t writes[2] = {};
        uint32_t completions[2] = {};
        uint32_t full[2] = {};
        uint32_t done_stall[2] = {};
        uint32_t low_read_conflict = 0;
        uint32_t low_write_conflict = 0;
        uint32_t closed = 0;
        uint32_t full_pop[2] = {};
        uint32_t empty_push[2] = {};
    } coverage;
    void check(uint64_t got, uint64_t want, const char *field, int channel = -1) {
        if (got != want) {
            const EngineStatus &e = reference[channel < 0 ? 0 : channel].engine;
            std::printf("dma2 FAIL phase=%s cycle=%u channel=%d task=%u element=%u field=%s expected=%llu actual=%llu\n",
                phase, tick, channel, e.command.task, e.element, field,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
            std::exit(1);
        }
    }
    void check_command(const DmaCommand &a, const DmaCommand &b, int i) {
        check(a.task,b.task,"command.task",i); check(a.src,b.src,"command.src",i);
        check(a.dst,b.dst,"command.dst",i); check(a.length,b.length,"command.length",i);
    }
    void check_response(const MemResponse &a, const MemResponse &b, int i) {
        check(a.task,b.task,"response.task",i); check(a.element,b.element,"response.element",i);
        check(a.write,b.write,"response.write",i); check(a.data,b.data,"response.data",i);
    }
    void check_audit(const AuditInfo &a, const AuditInfo &b, const char *field) {
        check(a.cycle,b.cycle,field); check(a.reserved,b.reserved,"audit.reserved");
        check(a.executed,b.executed,"audit.executed"); check(a.channel,b.channel,"audit.channel");
        check(a.request.task,b.request.task,"audit.task"); check(a.request.element,b.request.element,"audit.element");
        check(a.request.write,b.request.write,"audit.write"); check(a.request.address,b.request.address,"audit.address");
        check(a.request.data,b.request.data,"audit.data");
    }
    void check_snapshot(const TopStatus &s) {
        check(s.base_depth,3,"top.base_depth"); check(s.seed,0x4A31,"top.seed");
        check(s.dma.base_depth,3,"dma.base_depth");
        check(s.memory.memory.words,37,"memory.words"); check(s.memory.memory.addr_bits,6,"memory.addr_bits");
        check(s.memory.memory.seed,0x4A31,"memory.seed");
        for (uint32_t j=0;j<37;++j) check(s.memory.memory.data[j],memory[j],"memory.data");
        check(s.memory.arbiter.cycle,tick,"arbiter.cycle");
        check(s.memory.arbiter.accesses,accesses,"arbiter.accesses");
        check_audit(s.memory.arbiter.last,expected_audit,"arbiter.audit.cycle");
        check_audit(s.observed,expected_audit,"observer.audit.cycle");
        for (int i=0;i<2;++i) {
            const ChannelStatus &actual = i==0 ? s.dma.high : s.dma.low;
            const ReferenceChannel &r=reference[i]; const EngineStatus &e=r.engine;
            check(actual.buffer.depth,i==0?1:5,"buffer.depth",i);
            check(actual.buffer.enqueued,r.enqueued,"buffer.enqueued",i);
            check(actual.buffer.popped,r.popped,"buffer.popped",i);
            check(actual.buffer.can_push,r.queue.size()<static_cast<size_t>(i==0?1:5),"buffer.can_push",i);
            check(actual.buffer.can_pop,!r.queue.empty(),"buffer.can_pop",i);
            const EngineStatus &a=actual.engine;
            check(a.width,i==0?17:29,"engine.width",i); check(a.stride,i==0?1:2,"engine.stride",i);
            check(a.channel,i,"engine.channel",i); check(a.state,e.state,"engine.state",i);
            check_command(a.command,e.command,i); check(a.element,e.element,"engine.element",i);
            check(a.data,e.data,"engine.data",i); check(a.accepted,e.accepted,"engine.accepted",i);
            check(a.rejected,e.rejected,"engine.rejected",i); check(a.responses,e.responses,"engine.responses",i);
            check(a.completed,e.completed,"engine.completed",i); check(actual.completed,e.completed,"channel.completed",i);
            check(a.last_offer.cycle,e.last_offer.cycle,"offer.cycle",i);
            check(a.last_offer.channel,e.last_offer.channel,"offer.channel",i);
            check_response(a.last_response,e.last_response,i); check(a.error,0,"engine.error",i);
        }
    }
    bool drained() {
        return reference[0].queue.empty() && reference[1].queue.empty() &&
            reference[0].engine.state==S_IDLE && reference[1].engine.state==S_IDLE;
    }
    void reset_reference() {
        tick=0; accesses=0; expected_audit=AuditInfo{};
        for (uint32_t j=0;j<37;++j) memory[j]=0x4A31U ^ (j*0x9E3779B9U);
        for (int i=0;i<2;++i) {
            reference[i]=ReferenceChannel{};
            done_ready[i]=true; expected_done[i]=false; seen_done[i]=false;
        }
    }
    // Predict a complete cycle from software state and external inputs only.
    // Submissions commit after the engines see the old FIFO contents.
    void predict(const std::array<bool,2> &push, const std::array<DmaCommand,2> &commands) {
        expected_audit=AuditInfo{}; expected_audit.cycle=tick;
        bool popped[2]={};
        for (int i=0;i<2;++i) {
            expected_done[i]=false; seen_done[i]=false;
            ReferenceChannel &r=reference[i]; EngineStatus &e=r.engine;
            if (e.state==S_IDLE) {
                if (!r.queue.empty()) {
                    e.command=r.queue.front(); e.element=0;
                    e.state=e.command.length==0?S_DONE:S_READ;
                    popped[i]=true; ++r.popped;
                }
            } else if (e.state==S_READ || e.state==S_WRITE) {
                bool write=e.state==S_WRITE;
                if (tick%5==2 || expected_audit.reserved) {
                    ++e.rejected;
                    if (tick%5==2) ++coverage.closed;
                    else if (i==1) {
                        if (write) ++coverage.low_write_conflict;
                        else ++coverage.low_read_conflict;
                    }
                } else {
                    expected_audit.reserved=true; expected_audit.executed=true; expected_audit.channel=i;
                    MemRequest &q=expected_audit.request;
                    q.task=e.command.task; q.element=e.element; q.write=write;
                    q.address=(write?e.command.dst:e.command.src)+e.element*(i==0?1U:2U);
                    q.data=write?e.data:0;
                    e.last_offer=OfferInfo{tick,static_cast<uint32_t>(i)};
                    ++e.accepted;
                    // The response is applied below, after both engines execute.
                    e.state=write?S_WAIT_WRITE:S_WAIT_READ;
                }
            } else if (e.state==S_ADVANCE) {
                ++e.element;
                e.state=e.element==e.command.length?S_DONE:S_READ;
            } else if (e.state==S_DONE) {
                if (done_ready[i]) {
                    expected_done[i]=true;
                    expected_completion[i]=DmaCompletion{e.command.task,e.command.length};
                    ++e.completed; ++coverage.completions[i]; e.state=S_IDLE;
                } else ++coverage.done_stall[i];
            } else check(e.state,999,"unexpected software wait state",i);
        }
        if (expected_audit.reserved) {
            int i=static_cast<int>(expected_audit.channel);
            EngineStatus &e=reference[i].engine; const MemRequest &q=expected_audit.request;
            check(q.address<37,true,"reference address in range",i);
            MemResponse response{q.task,q.element,q.write,0};
            if (q.write) {
                memory[q.address]=q.data; e.state=S_ADVANCE; ++coverage.writes[i];
            } else {
                response.data=memory[q.address];
                e.data=response.data & (i==0?0x1FFFFU:0x1FFFFFFFU);
                e.state=S_WRITE; ++coverage.reads[i];
            }
            e.last_response=response; ++e.responses; ++accesses;
        }
        for (int i=0;i<2;++i) {
            if (popped[i]) reference[i].queue.pop_front();
            if (push[i]) { reference[i].queue.push_back(commands[i]); ++reference[i].enqueued; }
        }
    }
    void completion_received(int i, const DmaCompletion &c) {
        check(expected_done[i],true,"unexpected completion",i);
        check(seen_done[i],false,"duplicate completion",i);
        check(c.task,expected_completion[i].task,"completion.task",i);
        check(c.elements,expected_completion[i].elements,"completion.elements",i);
        seen_done[i]=true;
    }
}
SERVICE(done0, ready=done_ready[0], ARG(DmaCompletion) completion) { completion_received(0,completion); }
SERVICE(done1, ready=done_ready[1], ARG(DmaCompletion) completion) { completion_received(1,completion); }
SIMULATION() {
    auto reset = [&](const char *name) {
        phase=name; sim_reset(); reset_reference(); check_snapshot(snapshot());
    };
    auto step = [&](std::array<bool,2> send, std::array<DmaCommand,2> commands) {
        if (tick>=20000) check(tick,0,"phase timeout");
        std::array<bool,2> accepted{};
        for (int i=0;i<2;++i) {
            if (!send[i]) continue;
            bool ready=reference[i].queue.size()<static_cast<size_t>(i==0?1:5);
            bool old_empty=reference[i].queue.empty();
            bool pop_this_cycle=reference[i].engine.state==S_IDLE && !old_empty;
            accepted[i]=ready;
            if (!ready) { ++coverage.full[i]; if (pop_this_cycle) ++coverage.full_pop[i]; }
            if (ready && old_empty && reference[i].engine.state==S_IDLE) ++coverage.empty_push[i];
        }
        predict(accepted,commands);
        // Prepare callback expectations before any request can evaluate RTL services.
        for (int i=0;i<2;++i) if (send[i]) {
            bool actual=i==0?submit0(commands[i]):submit1(commands[i]);
            check(actual,accepted[i],"submit.ready",i);
        }
        sim_nextcycle();
        for (int i=0;i<2;++i) check(seen_done[i],expected_done[i],"completion.valid",i);
        ++tick; check_snapshot(snapshot());
        return accepted;
    };
    auto idle_step = [&]() { step({false,false},{}); };
    auto drain = [&]() { done_ready[0]=true; done_ready[1]=true; while (!drained()) idle_step(); };
    auto submit = [&](int i, DmaCommand command) {
        std::array<bool,2> send{}; std::array<DmaCommand,2> commands{};
        send[i]=true; commands[i]=command;
        while (!step(send,commands)[i]) {}
    };

    reset("single-high");
    submit(0,{1,36,36,1}); submit(0,{2,0,10,4}); drain();
    submit(0,{3,0,0,0}); drain();
    reset("single-low");
    submit(1,{4,36,36,1}); submit(1,{5,0,20,4}); submit(1,{6,0,0,0}); drain();

    reset("competition-and-overlap");
    step({true,true},{DmaCommand{10,0,12,4},DmaCommand{10,0,12,4}});
    submit(0,{11,12,0,4}); submit(1,{11,12,0,4}); drain();
    for (int j=0;j<7;++j) idle_step(); // Idle audits must contain no previous reservation.

    for (int i=0;i<2;++i) {
        reset(i==0?"full-high":"full-low");
        done_ready[i]=false;
        submit(i,{20,0,0,0});
        while (reference[i].engine.state!=S_DONE) idle_step();
        int depth=i==0?1:5;
        for (int j=0;j<depth;++j) submit(i,{static_cast<uint32_t>(21+j),0,8,1});
        std::array<bool,2> send{}; std::array<DmaCommand,2> commands{};
        send[i]=true; commands[i]={30,0,16,1};
        for (int j=0;j<3;++j) check(step(send,commands)[i],false,"full queue rejection",i);
        done_ready[i]=true;
        check(step(send,commands)[i],false,"full queue during completion",i);
        // Now IDLE with a full FIFO: the pop must not make this submission ready early.
        check(step(send,commands)[i],false,"full queue during pop",i);
        submit(i,commands[i]); drain();
    }

    reset("reset-under-contention");
    step({true,true},{DmaCommand{40,0,12,4},DmaCommand{41,0,20,4}});
    idle_step(); idle_step(); idle_step();
    reset("after-contention-reset");
    for (int j=0;j<8;++j) idle_step();
    step({true,true},{DmaCommand{42,0,8,1},DmaCommand{43,2,12,1}}); drain();
    reset("reset-under-completion-backpressure");
    done_ready[0]=false; done_ready[1]=false;
    step({true,true},{DmaCommand{44,0,0,0},DmaCommand{45,0,0,0}});
    idle_step(); idle_step();
    reset("after-completion-reset");
    for (int j=0;j<8;++j) idle_step();
    step({true,true},{DmaCommand{46,0,8,1},DmaCommand{47,2,12,1}}); drain();

    reset("random-200");
    uint32_t rng=0xD2A05EEDU;
    auto random = [&]() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; };
    std::array<uint32_t,2> submitted{};
    std::array<DmaCommand,2> pending{};
    std::array<bool,2> have_pending{};
    while (submitted[0]<100 || submitted[1]<100 || !drained()) {
        std::array<bool,2> send{};
        for (int i=0;i<2;++i) {
            done_ready[i]=(random()&3U)!=0;
            if (submitted[i]<100) {
                if (!have_pending[i]) {
                    uint32_t length=random()%5;
                    uint32_t span=length==0?0:(length-1)*(i==0?1:2);
                    pending[i]={1000+submitted[i],random()%(37-span),random()%(37-span),length};
                    have_pending[i]=true;
                }
                send[i]=(random()&3U)!=0;
            }
        }
        auto accepted=step(send,pending);
        for (int i=0;i<2;++i) if (accepted[i]) { ++submitted[i]; have_pending[i]=false; }
    }
    for (int i=0;i<2;++i) {
        check(reference[i].engine.completed,100,"random completions",i);
        check(coverage.reads[i]>0,true,"coverage.reads",i);
        check(coverage.writes[i]>0,true,"coverage.writes",i);
        check(coverage.completions[i]>0,true,"coverage.completions",i);
        check(coverage.full[i]>0,true,"coverage.full",i);
        check(coverage.done_stall[i]>0,true,"coverage.done_stall",i);
        check(coverage.full_pop[i]>0,true,"coverage.full_pop",i);
        check(coverage.empty_push[i]>0,true,"coverage.empty_push",i);
    }
    check(coverage.low_read_conflict>0,true,"coverage.low_read_conflict");
    check(coverage.low_write_conflict>0,true,"coverage.low_write_conflict");
    check(coverage.closed>0,true,"coverage.closed");
    std::printf("dma2 PASS: 200 random tasks plus directed tests; conflicts read=%u write=%u, closed=%u, full=%u/%u, done stalls=%u/%u\n",
        coverage.low_read_conflict,coverage.low_write_conflict,coverage.closed,
        coverage.full[0],coverage.full[1],coverage.done_stall[0],coverage.done_stall[1]);
}
