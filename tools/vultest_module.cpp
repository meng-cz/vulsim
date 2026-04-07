
#include "vcpp.hpp"

#include "simgen.h"

int main(int argc, char * argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: vultest <header.hpp>" << std::endl;
        return 1;
    }

    std::string header_path = argv[1];

    vector<string> code = _readFileLines(header_path);
    auto strip = _stripComments(code);

    VulModule module = _parseModule(strip.lines, "test_module");

    vector<string> debug_lines = module.debugPrintModule();
    for (const auto &line : debug_lines) {
        std::cout << line;
        if (line.empty() || line.back() != '\n') {
            std::cout << "\n";
        }
    }

    return 0;
}

