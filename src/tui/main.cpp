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


#include "project.h"

#include "tcpsocket.hpp"
#include "argparse.hpp"

#include <atomic>
#include <mutex>
#include <thread>


bool g_listening_public_address = false;

void println_stdout(const string &msg);

unique_ptr<VulProject> g_vul_project;
std::mutex g_vul_project_mutex;

std::atomic<bool> g_main_socket_terminate_flag;
int32_t g_port_main_socket = 17995;

void main_socket_thread_func() {
    TCPSocket main_socket(g_port_main_socket, g_listening_public_address, println_stdout);
    while (!g_main_socket_terminate_flag.load(std::memory_order_acquire)) {
        string req_json = main_socket.receive_blocked();
        if (req_json.empty()) {
            continue;
        }
        VulOperationPackage op = serializeOperationPackageFromJSON(req_json);
        g_vul_project_mutex.lock();
        VulOperationResponse resp = g_vul_project->doOperation(op);
        g_vul_project_mutex.unlock();
        string logstr = "Operation '" + op.name + "' executed. Response code: " + std::to_string(resp.code) + ", message: " + resp.msg;
        println_stdout(logstr);
        string resp_json = serializeOperationResponseToJSON(resp);
        main_socket.send_blocked(resp_json);
    }
}

void cmd_loop();

int main(int argc, char *argv[]) {
    
    argparse::ArgumentParser parser("vulsim-tui", "VulSim Terminal User Interface V1.0");
    parser.add_argument("-p", "--port")
        .help("sets the TCP port for main socket communication (default: 17995)")
        .default_value(17995)
        .action([](const std::string &value) { return std::stoi(value); });
    parser.add_argument("-l", "--listen")
        .help("listen on public address instead of loopback")
        .default_value(false)
        .implicit_value(true);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << std::endl;
        std::cerr << parser.help().str() << std::endl;
        return 1;
    }
    
    g_port_main_socket = parser.get<int>("-p");
    g_listening_public_address = parser.get<bool>("-l");

    g_vul_project = std::make_unique<VulProject>(vector<ProjectPath>{});

    println_stdout("VulSim TUI started. Listening on port " + std::to_string(g_port_main_socket) + (g_listening_public_address ? " (public)" : " (loopback)"));

    g_main_socket_terminate_flag.store(false, std::memory_order_seq_cst);
    std::thread main_socket_thread(main_socket_thread_func);

    cmd_loop();

    g_main_socket_terminate_flag.store(true, std::memory_order_release);
    main_socket_thread.join();

    return 0;
}

void println_stdout(const string &msg) {
    std::cout << msg << std::endl;
}
void cmd_loop() {
    println_stdout("Type 'exit' or 'quit' to terminate the program.");

    std::string line;
    while (true) {
        std::cout << "vulsim> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line == "exit" || line == "quit") {
            break;
        }
        // Process other commands here
        std::cout << "Unknown command: " << line << std::endl;
    }
}
