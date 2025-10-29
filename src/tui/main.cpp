/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "console.h"

#include <iostream>
#include <string>
#include <cctype>
#include <fstream>

int main(int argc, char *argv[]) {
    VulConsole console;
    std::string line;

    if (argc > 1) {
        // shell file mode: read script file and execute each non-empty line as a console command
        std::ifstream ifs(argv[1]);
        if (!ifs) {
            std::cerr << "Error: cannot open script file: " << argv[1] << std::endl;
            return 1;
        }

        uint64_t lineno = 1;

        while (std::getline(ifs, line)) {
            // trim leading/trailing whitespace
            size_t start = 0;
            while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) ++start;
            size_t end = line.size();
            while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) --end;
            if (start >= end) continue; // empty line

            std::string cmd = line.substr(start, end - start);
            // skip comment lines starting with '#'
            if (!cmd.empty() && cmd[0] == '#') continue;

            // print [lineno], prefix and command
            std::cout << "[" << lineno << "] " << console.getPrefix() << cmd << std::endl;
            std::cout.flush();

            auto out = console.performCommand(cmd);
            if (out) {
                for (const auto &l : *out) std::cout << l << std::endl;
                // check success: first line must start with "OK"
                if (out->empty() || (*out)[0].rfind("OK", 0) != 0) {
                    std::cerr << "Error: command failed at line " << lineno << ": " << cmd << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: command produced no output at line " << lineno << ": " << cmd << std::endl;
                return 1;
            }
        }

        return 0;
    }

    while (true) {
        // print prompt
        std::cout << console.getPrefix();
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            // EOF
            std::cout << std::endl;
            break;
        }

        // trim leading/trailing whitespace
        size_t start = 0;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start])))
            ++start;
        size_t end = line.size();
        while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1])))
            --end;
        if (start >= end)
            continue; // empty line

        std::string cmd = line.substr(start, end - start);
        if (cmd == "exit" || cmd == "quit")
            break;

        auto out = console.performCommand(cmd);
        if (out) {
            for (const auto &l : *out)
                std::cout << l << std::endl;
        }
    }

    return 0;
}
