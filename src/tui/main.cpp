/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "console.h"

#include <iostream>
#include <string>
#include <cctype>

int main() {
    VulConsole console;
    std::string line;

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
