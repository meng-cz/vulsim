// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "module.h"
#include "configexpr.hpp"
#include "toposort.hpp"
#include "cppparse.hpp"
#include <cassert>
#include <functional>
#include <iostream>

using std::make_shared;

VulStaticReqServ staticalizeReqServ(const VulTempReqServBase &item, const VulStaticConfigLib &config_lib) {
    VulStaticReqServ static_req;
    static_req.name = item.name;
    static_req.has_handshake = item.has_handshake;
    for (const auto &arg : item.args) {
        VulStaticArg static_arg;
        static_arg.name = arg.second;
        static_arg.type = parseTypeSignature(arg.first, config_lib);
        static_req.args.push_back(static_arg);
    }
    for (const auto &ret : item.rets) {
        VulStaticArg static_arg;
        static_arg.name = ret.second;
        static_arg.type = parseTypeSignature(ret.first, config_lib);
        static_req.rets.push_back(static_arg);
    }
    return static_req;
}

void instantiateModule(
    VulStaticModuleInstance &instance,
    const VulTempModule &temp,
    const VulStaticConfigLib &param_overrides,
    const VulStaticConfigLib &global_config,
    const VulStaticBundleLib &global_bundles
) {
    instance.filepath = temp.filepath;
    instance.module_name = temp.name;

    VulStaticConfigLib local_config_lib = global_config;
    
    for (const auto &conf : temp.configs) {
        VulErrorContextGuard _err{"Processing config '" + conf.name + "'"};
        ConfigRealValue value = calculateConstexprValue(conf.value, local_config_lib);
        local_config_lib[conf.name] = value;
        instance.local_consts[conf.name] = value;
    }
    for (const auto &param : temp.params) {
        VulErrorContextGuard _err{"Processing parameter '" + param.name + "'"};
        ConfigRealValue value;
        auto override_iter = param_overrides.find(param.name);
        if (override_iter != param_overrides.end()) {
            value = override_iter->second;
        } else {
            value = calculateConstexprValue(param.value, local_config_lib);
        }
        local_config_lib[param.name] = value;
        instance.local_parameters[param.name] = value;
    }

    for (const auto &bundle : temp.bundles) {
        VulErrorContextGuard _err{"Processing bundle '" + bundle.name + "'"};
        VulStaticBundle sb = staticalizeBundle(bundle, local_config_lib);
        instance.local_bundles.push_back(sb);
    }
    VulStaticBundleLib local_config_library = mergeStaticBundleLibs(global_bundles, instance.local_bundles);

    // Requests
    for (const auto &req : temp.requests) {
        VulErrorContextGuard _err{"Processing request '" + req.name + "'"};
        instance.requests[req.name] = staticalizeReqServ(req, local_config_lib);
    }

    // services
    VulLogicBlockID next_logic_block_id = 1;
    for (const auto &serv : temp.services) {
        const string &serv_name = serv.name;
        VulErrorContextGuard _err{"Processing service '" + serv_name + "'"};
        instance.services[serv_name] = staticalizeReqServ(serv, local_config_lib);
        VulLogicBlock lb;
        lb.block_id = next_logic_block_id++;
        lb.with_priority = !serv.priority.empty();
        if (lb.with_priority) {
            ConfigRealValue priority_value = calculateConstexprValue(serv.priority, local_config_lib);
            lb.priority = priority_value;
        } else {
            lb.priority = 0;
        }
        lb.codelines = serv.codelines;
        if (serv.has_handshake) {
            lb.cond_codelines.push_back("return (" + serv.cond + ");\n");
        }
        instance.serv_logic_blocks[serv_name] = lb;
    }

    // registers
    for (const auto &reg : temp.registers) {
        VulErrorContextGuard _err{"Processing register '" + reg.name + "'"};
        VulStaticRegister static_reg;
        static_reg.name = reg.name;
        static_reg.signature = parseTypeSignature(reg.type, local_config_lib);
        static_reg.ports = 1;
        if (!reg.portnum.empty()) {
            ConfigRealValue portnum_value = calculateConstexprValue(reg.portnum, local_config_lib);
            static_reg.ports = (portnum_value > 1) ? portnum_value : 1;
        }
        static_reg.reset_codelines = reg.reset_codelines;
        for (const auto &dim : reg.dims) {
            ConfigRealValue dim_value = calculateConstexprValue(dim, local_config_lib);
            static_reg.dims.push_back(dim_value);
        }
        instance.registers.push_back(std::move(static_reg));
    }

    // wires
    for (const auto &wire : temp.wires) {
        VulErrorContextGuard _err{"Processing wire '" + wire.name + "'"};
        VulStaticWire static_wire;
        static_wire.name = wire.name;
        static_wire.signature = parseTypeSignature(wire.type, local_config_lib);
        static_wire.reset_codelines = wire.reset_codelines;
        instance.wires.push_back(std::move(static_wire));
    }

    // brams
    for (const auto &bram : temp.brams) {
        VulErrorContextGuard _err{"Processing BRAM '" + bram.name + "'"};
        VulStaticBRAM static_bram;
        static_bram.name = bram.name;
        static_bram.data_type = parseTypeSignature(bram.data_type, local_config_lib);
        static_bram.addr_width = calculateConstexprValue(bram.addr_width, local_config_lib);
        static_bram.read_ports = calculateConstexprValue(bram.read_ports, local_config_lib);
        static_bram.write_ports = calculateConstexprValue(bram.write_ports, local_config_lib);
        instance.brams.push_back(std::move(static_bram));
    }

    // digital ROMs
    for (const auto &rom : temp.roms) {
        VulErrorContextGuard _err{"Processing ROM '" + rom.name + "'"};
        VulStaticDigitalROM static_rom;
        static_rom.name = rom.name;
        static_rom.data_width = calculateConstexprValue(rom.data_width, local_config_lib);
        static_rom.addr_width = calculateConstexprValue(rom.addr_width, local_config_lib);
        static_rom.read_ports = calculateConstexprValue(rom.read_ports, local_config_lib);
        static_rom.init_path = rom.init_path;
        instance.roms.push_back(std::move(static_rom));
    }

    // queues
    for (const auto &queue : temp.queues) {
        VulErrorContextGuard _err{"Processing queue '" + queue.name + "'"};
        VulStaticQueue static_queue;
        static_queue.name = queue.name;
        static_queue.type = parseTypeSignature(queue.type, local_config_lib);
        static_queue.depth = calculateConstexprValue(queue.depth, local_config_lib);
        static_queue.enq_width = calculateConstexprValue(queue.enq_width, local_config_lib);
        static_queue.deq_width = calculateConstexprValue(queue.deq_width, local_config_lib);
        instance.queues.push_back(std::move(static_queue));
    }

    // instances
    for (const auto &inst : temp.instances) {
        VulErrorContextGuard _err{"Processing instance '" + inst.name + "'"};
        VulStaticInstanceDecl static_inst;
        static_inst.name = inst.name;
        static_inst.module_name = inst.module_name;
        for (const auto &param_override : inst.parameter_overrides) {
            const string &param_name = param_override.first;
            const string &param_value_str = param_override.second;
            ConfigRealValue param_value = calculateConstexprValue(param_value_str, local_config_lib);
            static_inst.parameter_overrides[param_name] = param_value;
        }
        for (const auto &serv : inst.referenced_services) {
            static_inst.referenced_services.insert(serv);
        }
        instance.instances[inst.name] = static_inst;
    }

    // tick blocks
    for (const auto &tb : temp.tick_blocks) {
        VulErrorContextGuard _err{"Processing tick block"};
        VulTickBlock tick_block;
        tick_block.codelines = tb;
        instance.tick_blocks.push_back(std::move(tick_block));
    }

    // req-serv connections
    instance.req_connections = temp.req_connections;

}

void detectRequestCallInLogicBlocks(VulStaticModuleInstance &module_instance) {
    unordered_map<string, LogicBlockCall> valid_function_names;
    for (const auto &req_entry : module_instance.requests) {
        LogicBlockCall call;
        call.instance = "";
        call.port = req_entry.first;
        valid_function_names[req_entry.first] = call;
    }
    for (const auto &inst_entry : module_instance.instances) {
        const auto &inst = inst_entry.second;
        for (const auto &serv : inst.referenced_services) {
            LogicBlockCall call;
            call.instance = inst_entry.first;
            call.port = serv;
            valid_function_names[inst_entry.first + "_" + serv] = call;
        }
    }
    for (auto &serv_entry: module_instance.serv_logic_blocks) {
        auto &serv_lb = serv_entry.second;
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(serv_lb.codelines, func_entry.first)) {
                serv_lb.call_requests.push_back(func_entry.second);
            }
        }
    }
    for (auto &tb: module_instance.tick_blocks) {
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(tb.codelines, func_entry.first)) {
                tb.call_requests.push_back(func_entry.second);
            }
        }
    }
}

uint64_t findConnectedLogicBlockID(shared_ptr<VulStaticModuleInstance> instance, const LogicBlockCall &call) {
    // 顺着指针关系和req_connections成员找到这个call最终连接到的serv_logic_block的ID，返回这个ID（高32位为instance_id，低32位为block_id）。由于连接的单一性，一个call最多只能连接到一个serv_logic_block，如果找不到连接的serv_logic_block，抛出异常并退出

    auto find_child_ptr_by_name = [](shared_ptr<VulStaticModuleInstance> &mod, const InstanceName &child_name) -> shared_ptr<VulStaticModuleInstance> {
        for (auto &child : mod->children) {
            if (!child->instance_path.empty() && child->instance_path.back() == child_name) {
                return child;
            }
        }
        return nullptr;
    };
    auto pack_lb_id = [](uint32_t instance_id, uint32_t block_id) -> uint64_t {
        return ((uint64_t)instance_id << 32) | block_id;
    };

    shared_ptr<VulStaticModuleInstance> cur = instance;
    ReqServName port;
    bool is_serv_call = false;

    if (call.instance.empty()) {
        // it's a request call
        is_serv_call = false;
        port = call.port;
    } else {
        // it's a service call
        is_serv_call = true;
        port = call.port;
        cur = find_child_ptr_by_name(cur, call.instance);
        if (cur == nullptr) {
            throw VulException("Invalid LogicBlockCall: instance '" + call.instance + "' not found in instance '" + instance->instance_path.back() + "'");
        }
    }

    for (uint32_t hop = 0; hop < 4096; hop ++) {
        VulErrorContextGuard hop_guard("Entering instance '" + cur->simClassName() + "' with port '" + port + "'");
        if (is_serv_call) {
            // impl as code block here, or connected to a child service
            auto lb_iter = cur->serv_logic_blocks.find(port);
            if (lb_iter != cur->serv_logic_blocks.end()) {
                return pack_lb_id(cur->instance_id, lb_iter->second.block_id);
            } else {
                // find connected child service
                bool found_conn = false;
                for (const auto &conn : cur->req_connections) {
                    if (conn.req_instance == "" && conn.req_name == port) {
                        cur = find_child_ptr_by_name(cur, conn.serv_instance);
                        if (cur == nullptr) {
                            throw VulException("Invalid Req-Serv connection: instance '" + conn.serv_instance + "' not found in instance '" + instance->instance_path.back() + "'");
                        }
                        port = conn.serv_name;
                        found_conn = true;
                        break;
                    }
                }
                if (!found_conn) {
                    throw VulException("No connected service found for LogicBlockCall to service '" + port + "' in instance '" + instance->instance_path.back() + "'");
                }
            }
            continue;
        }
        // request call, should be connected at parent level
        auto parent = cur->parent;
        // CR-R: connected to parent's request
        // CR-S: connected to parent's service
        // CR-CS: connected to another child's service
        bool found_conn = false;
        for (const auto &conn : parent->req_connections) {
            if (conn.req_instance == cur->instance_path.back() && conn.req_name == port) {
                if (conn.serv_instance == "") {
                    // connected to parent's service/request
                    cur = parent;
                    port = conn.serv_name;
                    is_serv_call = (parent->requests.find(conn.serv_name) == parent->requests.end());
                } else {
                    // connected to another child's service
                    cur = find_child_ptr_by_name(parent, conn.serv_instance);
                    if (cur == nullptr) {
                        throw VulException("Invalid Req-Serv connection: instance '" + conn.serv_instance + "' not found in instance '" + parent->instance_path.back() + "'");
                    }
                    port = conn.serv_name;
                    is_serv_call = true;
                }
                found_conn = true;
                break;
            }
        }
        if (!found_conn) {
            throw VulException("No connected service found for LogicBlockCall to request '" + port + "' in instance '" + instance->instance_path.back() + "'");
        }
    }
    throw VulException("Exceeded maximum hop count while finding connected logic block for LogicBlockCall to '" + port + "' in instance '" + instance->instance_path.back() + "'. Possible cyclic connections.");
}

void setupUpdateSequence(shared_ptr<VulStaticModuleInstance> &top) {

    VulErrorContextGuard top_guard("Setting up update sequence for instance '" + top->simClassName() + "'");

    unordered_set<uint64_t> all_logic_block_ids;
    unordered_map<uint64_t, unordered_set<uint64_t>> logic_block_call_graph;
    unordered_map<VulInstanceID, shared_ptr<VulStaticModuleInstance>> instance_id_map;

    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        instance_id_map[cur_inst->instance_id] = cur_inst;

        VulErrorContextGuard inst_guard("Parsing connection from instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst->instance_id) + ")");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            VulErrorContextGuard lb_guard("Parsing service logic block '" + serv_lb_entry.first + "' (BID: " + std::to_string(serv_lb.block_id) + ")");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : serv_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
        for (const auto &tick_lb : cur_inst->tick_blocks) {
            VulErrorContextGuard lb_guard("Parsing tick logic block (BID: 0)");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32);
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : tick_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
    }

    auto debug_lb_name_by_id = [&](uint64_t lb_id) -> string {
        VulInstanceID inst_id = lb_id >> 32;
        uint32_t block_id = lb_id & 0xFFFFFFFF;
        auto inst_iter = instance_id_map.find(inst_id);
        if (inst_iter == instance_id_map.end()) {
            return "<unknown instance " + std::to_string(inst_id) + ">";
        }
        auto &inst_ptr = inst_iter->second;
        string instance_path_str;
        for (const auto &path_elem : inst_ptr->instance_path) {
            if (!instance_path_str.empty()) instance_path_str += "::";
            instance_path_str += path_elem;
        }
        if (block_id == 0) {
            return instance_path_str + ".<tick>";
        }
        for (auto &serv_lb_entry : inst_ptr->serv_logic_blocks) {
            auto &serv_lb = serv_lb_entry.second;
            if (serv_lb.block_id == block_id) {
                return instance_path_str + "." + serv_lb_entry.first;
            }
        }
        return instance_path_str + ".<unknown>";
    };

    VulErrorContextGuard graph_guard("Checking logic block call graph");
    unordered_map<VulInstanceID, vector<uint64_t>> instance_tick_to_lb_call_graph;
    // logic_block_call_graph 是 {tick_code_blocks, serv_logic_blocks} 之间的调用关系图，instance_tick_to_lb_call_graph 是从实例的 tick_code_block 到被所有可能被递归调用的 logic_block 的调用关系图
    for (const auto &inst_entry : instance_id_map) {
        const auto &inst_id = inst_entry.first;
        const auto &inst_ptr = inst_entry.second;
        uint64_t tick_lb_id = ((uint64_t)inst_id << 32);
        unordered_set<uint64_t> visited;
        struct CallPathNode {
            uint64_t lb_id;
            vector<uint64_t> call_path; // for debugging
        };
        std::deque<CallPathNode> bfs_queue;
        bfs_queue.push_back({tick_lb_id, {tick_lb_id}});
        while (!bfs_queue.empty()) {
            auto [cur_lb_id, call_path] = bfs_queue.front();
            bfs_queue.pop_front();
            if (visited.find(cur_lb_id) != visited.end()) {
                string loop_str;
                for (const auto &lb_id : call_path) {
                    if (!loop_str.empty()) loop_str += " -> ";
                    loop_str += debug_lb_name_by_id(lb_id);
                }
                throw VulException("Cyclic call or repeated call detected in logic block call graph: " + loop_str);
            }
            visited.insert(cur_lb_id);
            const auto &called_lbs_set = logic_block_call_graph.find(cur_lb_id);
            if (called_lbs_set != logic_block_call_graph.end()) {
                for (const auto &called_lb_id : called_lbs_set->second) {
                    vector<uint64_t> new_call_path = call_path;
                    new_call_path.push_back(called_lb_id);
                    bfs_queue.push_back({called_lb_id, new_call_path});
                }
            }
        }
    }

    unordered_map<uint64_t, VulInstanceID> logic_block_id_to_instance_id;
    // 反向映射一下，同时保证每一个serv codeblock仅会被最多一个tick codeblock调用，否则这个重复调用行为在硬件上是无法实现的
    for (const auto &entry : instance_tick_to_lb_call_graph) {
        const auto &inst_id = entry.first;
        const auto &called_lb_ids = entry.second;
        for (const auto &lb_id : called_lb_ids) {
            auto iter = logic_block_id_to_instance_id.find(lb_id);
            if (iter != logic_block_id_to_instance_id.end() && iter->second != inst_id) {
                string called_inst1 = debug_lb_name_by_id(static_cast<uint64_t>(iter->second) << 32);
                string called_inst2 = debug_lb_name_by_id(static_cast<uint64_t>(inst_id) << 32);
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by multiple instances: '" + called_inst1 + "' and '" + called_inst2 + "'. This is not allowed because it cannot be implemented in hardware.");
            }
            logic_block_id_to_instance_id[lb_id] = inst_id;
        }
    }

    VulErrorContextGuard order_guard("Determining instance update order");

    unordered_map<VulInstanceID, unordered_set<VulInstanceID>> instance_order_graph;
    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();
        VulInstanceID cur_inst_id = cur_inst->instance_id;
        map<int32_t, vector<VulInstanceID>> priority_to_callee_instances;
        priority_to_callee_instances[0].push_back(cur_inst_id); // tick block has default priority 0

        VulErrorContextGuard inst_guard("Processing instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst_id) + ") for update order");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            if (!serv_lb.with_priority) {
                continue;
            }
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            auto callee_inst_iter = logic_block_id_to_instance_id.find(lb_id);
            if (callee_inst_iter == logic_block_id_to_instance_id.end()) {
                // not called by any tick block, skip
                continue;
            }
            VulInstanceID callee_inst_id = callee_inst_iter->second;
            if (callee_inst_id == cur_inst_id) {
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by its own tick block.");
            }
            priority_to_callee_instances[serv_lb.priority].push_back(callee_inst_id);
        }
        
        unordered_set<VulInstanceID> lower_priority_instance_set;
        vector<VulInstanceID> lower_priority_instances;
        for (auto prio_it = priority_to_callee_instances.rbegin(); prio_it != priority_to_callee_instances.rend(); ++prio_it) {
            auto &same_priority_instances = prio_it->second;
            if (same_priority_instances.empty()) {
                continue;
            }
            std::sort(same_priority_instances.begin(), same_priority_instances.end());
            same_priority_instances.erase(std::unique(same_priority_instances.begin(), same_priority_instances.end()), same_priority_instances.end());

            if (!lower_priority_instances.empty()) {
                for (VulInstanceID from_inst_id : same_priority_instances) {
                    auto &out_edges = instance_order_graph[from_inst_id];
                    for (VulInstanceID to_inst_id : lower_priority_instances) {
                        if (from_inst_id != to_inst_id) {
                            out_edges.insert(to_inst_id);
                        }
                    }
                }
            }

            for (VulInstanceID inst_id : same_priority_instances) {
                if (lower_priority_instance_set.insert(inst_id).second) {
                    lower_priority_instances.push_back(inst_id);
                }
            }
        }
    }

    unordered_map<VulInstanceID, unordered_map<VulInstanceID, unordered_set<VulInstanceID>>> child_instance_order_graph; // instance_id -> child_instance_id -> set of child_instance_id that should be updated after this child_instance_id
    for (const auto &entry : instance_order_graph) {
        const auto &former_inst_id = entry.first;
        const auto &latter_inst_ids = entry.second;
        auto former_inst_iter = instance_id_map.find(former_inst_id);
        if (former_inst_iter == instance_id_map.end()) {
            throw VulException("Instance ID " + std::to_string(former_inst_id) + " not found in instance_id_map");
        }
        shared_ptr<VulStaticModuleInstance> former_inst_ptr = former_inst_iter->second;
        for (const auto &latter_inst_id : latter_inst_ids) {
            auto latter_inst_iter = instance_id_map.find(latter_inst_id);
            if (latter_inst_iter == instance_id_map.end()) {
                throw VulException("Instance ID " + std::to_string(latter_inst_id) + " not found in instance_id_map");
            }
            shared_ptr<VulStaticModuleInstance> latter_inst_ptr = latter_inst_iter->second;
            
            size_t former_depth = former_inst_ptr->instance_path.size();
            size_t latter_depth = latter_inst_ptr->instance_path.size();
            shared_ptr<VulStaticModuleInstance> former_ancestor = former_inst_ptr;
            shared_ptr<VulStaticModuleInstance> latter_ancestor = latter_inst_ptr;
            VulInstanceID former_last_id = former_inst_id;
            VulInstanceID latter_last_id = latter_inst_id;

            while (former_depth > latter_depth) {
                former_last_id = former_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                --former_depth;
            }
            while (latter_depth > former_depth) {
                latter_last_id = latter_ancestor->instance_id;
                latter_ancestor = latter_ancestor->parent;
                --latter_depth;
            }
            while (former_ancestor.get() != latter_ancestor.get()) {
                if (former_ancestor == nullptr || latter_ancestor == nullptr) {
                    throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
                }
                former_last_id = former_ancestor->instance_id;
                latter_last_id = latter_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                latter_ancestor = latter_ancestor->parent;
            }
            if (former_ancestor == nullptr) {
                throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
            }

            shared_ptr<VulStaticModuleInstance> common_ancestor = former_ancestor;
            if (former_last_id != latter_last_id) {
                child_instance_order_graph[common_ancestor->instance_id][former_last_id].insert(latter_last_id);
            }
        }
    }

    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        const VulInstanceID cur_inst_id = cur_inst->instance_id;
        vector<VulInstanceID> update_nodes;
        update_nodes.reserve(cur_inst->children.size() + 1);
        update_nodes.push_back(cur_inst_id); // self ID represents local tick
        for (const auto &child : cur_inst->children) {
            update_nodes.push_back(child->instance_id);
            bfs_queue.push_back(child);
        }

        unordered_map<VulInstanceID, int32_t> indegree;
        indegree.reserve(update_nodes.size() * 2);
        for (VulInstanceID node_id : update_nodes) {
            indegree.emplace(node_id, 0);
        }

        unordered_map<VulInstanceID, vector<VulInstanceID>> adj;
        auto order_it = child_instance_order_graph.find(cur_inst_id);
        if (order_it != child_instance_order_graph.end()) {
            adj.reserve(order_it->second.size());
            for (const auto &from_entry : order_it->second) {
                VulInstanceID from_id = from_entry.first;
                if (indegree.find(from_id) == indegree.end()) {
                    continue;
                }
                auto &out_edges = adj[from_id];
                out_edges.reserve(from_entry.second.size());
                for (VulInstanceID to_id : from_entry.second) {
                    auto indeg_it = indegree.find(to_id);
                    if (indeg_it == indegree.end() || to_id == from_id) {
                        continue;
                    }
                    out_edges.push_back(to_id);
                    indeg_it->second += 1;
                }
            }
        }

        std::priority_queue<VulInstanceID, vector<VulInstanceID>, std::greater<VulInstanceID>> ready;
        for (const auto &entry : indegree) {
            if (entry.second == 0) {
                ready.push(entry.first);
            }
        }

        cur_inst->update_seq.clear();
        cur_inst->update_seq.reserve(update_nodes.size());

        while (!ready.empty()) {
            VulInstanceID from_id = ready.top();
            ready.pop();
            cur_inst->update_seq.push_back(from_id);

            auto adj_it = adj.find(from_id);
            if (adj_it == adj.end()) {
                continue;
            }
            for (VulInstanceID to_id : adj_it->second) {
                auto indeg_it = indegree.find(to_id);
                if (indeg_it == indegree.end()) {
                    continue;
                }
                indeg_it->second -= 1;
                if (indeg_it->second == 0) {
                    ready.push(to_id);
                }
            }
        }

        if (cur_inst->update_seq.size() != update_nodes.size()) {
            throw VulException("Cyclic update constraints found in instance update order graph for instance ID " + std::to_string(cur_inst_id));
        }
    }

}

