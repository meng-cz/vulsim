
#include "common.h"
#include "simulation.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <locale>

typedef struct {
    array<vector<string>, 26> args;
    vector<string> _default;
} CmdArgs;

static CmdArgs & get_global_cmdargs() {
    static CmdArgs args;
    return args;
}

vector<string> get_cmdline_args(const char prefix) {
    auto &args = get_global_cmdargs();
    if(prefix >= 'a' && prefix <= 'z') {
        return args.args[prefix - 'a'];
    } else if(prefix >= 'A' && prefix <= 'Z') {
        return args.args[prefix - 'A'];
    } else {
        return args._default;
    }
}

void init_cmdline_args(int argc, char ** argv) {
    auto &args = get_global_cmdargs();

    char prefix = 0;

    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] >= 'a' && argv[i][1] <= 'z') {
            prefix = argv[i][1];
        } else if(argv[i][0] == '-' && argv[i][1] >= 'A' && argv[i][1] <= 'Z') {
            for(int n = i + 1; n < argc; n++) {
                args.args[argv[i][1] - 'A'].push_back(string(argv[n]));
            }
            break;
        } else if (prefix >= 'a' && prefix <= 'z') {
            args.args[prefix - 'a'].push_back(string(argv[i]));
        }
    }

}

uint64 g_current_tick = 0;

uint64 get_current_tick() {
    return g_current_tick;
}

// get_current_time_us implementation on windows
#ifdef _WIN32
#include <windows.h>
uint64 get_current_time_us() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64 time = ((uint64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return time / 10;
}
#endif
// get_current_time_us implementation on linux
#ifdef __linux__
#include <time.h>
uint64 get_current_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64)ts.tv_sec * 1000000 + (uint64)(ts.tv_nsec / 1000);
}
#endif

static unordered_map<string, int64> & get_design_config_map() {
    static unordered_map<string, int64> design_config_map;
    return design_config_map;
}

int64 get_design_config_item(const string key) {
    auto &design_config_map = get_design_config_map();
    auto it = design_config_map.find(key);
    if(it != design_config_map.end()) {
        return it->second;
    } else {
        return 0;
    }
}

void load_config_item_from_inifile(const string filename) {
    std::ifstream ifs(filename);
    if(!ifs.is_open()) {
        return;
    }

    auto ltrim = [](std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    };
    auto rtrim = [](std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    };
    auto trim = [&](std::string &s) {
        ltrim(s);
        rtrim(s);
    };

    string line;
    auto &m = get_design_config_map();
    while(std::getline(ifs, line)) {
        // remove any comment starting with # or ; or /
        size_t comment_pos = line.find_first_of("#;/");
        if(comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }

        trim(line);
        if(line.empty()) continue;

        size_t eq = line.find('=');
        if(eq == string::npos) continue;

        string name = line.substr(0, eq);
        string valstr = line.substr(eq + 1);
        trim(name);
        trim(valstr);

        if(name.empty()) continue;

        int64 value = 0;
        try {
            // parse as base 0 to allow 0x hex
            size_t idx = 0;
            long long tmp = std::stoll(valstr, &idx, 0);
            (void)idx;
            value = (int64)tmp;
        } catch(...) {
            value = 0;
        }

        m[name] = value;
    }
}

bool parse_target_tick(const string &str, uint64 &target_tick) {
    string s = str;
    if(s.empty()) {
        return false;
    }

    // detect suffix
    char suffix = s.back();
    uint64 multiplier = 1;
    bool has_suffix = false;
    if((suffix >= '0' && suffix <= '9')) {
        has_suffix = false;
    } else {
        has_suffix = true;
        s.pop_back();
        switch(suffix) {
            case 'k': multiplier = 1000ULL; break;
            case 'm': multiplier = 1000000ULL; break;
            case 'g': multiplier = 1000000000ULL; break;
            case 't': multiplier = 1000000000000ULL; break;
            case 'K': multiplier = (1ULL<<10); break;
            case 'M': multiplier = (1ULL<<20); break;
            case 'G': multiplier = (1ULL<<30); break;
            case 'T': multiplier = (1ULL<<40); break;
            default: return false;
        }
    }

    // parse number part: support decimal or hex (0x)
    try {
        size_t idx = 0;
        // allow hex if starts with 0x or 0X
        int base = 10;
        if(s.size() > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
        unsigned long long val = std::stoull(s, &idx, base);
        if(idx != s.size()) {
            return false;
        }
        // check overflow when multiplying
        if(val != 0 && multiplier > (uint64(-1) / val)) {
            return false;
        }
        target_tick = (uint64)val * multiplier;
    } catch(...) {
        return false;
    }
    return true;
}

void exit_simulation(int32 code) {
    finalize_simulation();
    exit(code);
}

int main(int argc, char ** argv) {
    // initialize command line arguments
    init_cmdline_args(argc, argv);

    // load design config from inifile if specified
    auto inifiles = get_cmdline_args('I');
    if(inifiles.empty()) {
        inifiles.push_back("config.ini");
    }
    for(auto &inifile : inifiles) {
        load_config_item_from_inifile(inifile);
    }

    uint64 target_tick = 0;
    auto ticks = get_cmdline_args('t');
    if(!ticks.empty()) {
        if(!parse_target_tick(ticks[0], target_tick)) {
            fprintf(stderr, "invalid tick value: %s\n", ticks[0].c_str());
            printf("tick format: <number>[k|m|g|t|K|M|G|T], e.g. 1000, 1k, 1M, 2G\n");
            return 1;
        }
    }

    if(target_tick > 0) {
        printf("Running simulation for %llu ticks...\n", (unsigned long long)target_tick);
    } else {
        printf("Running simulation until interrupted...\n");
    }

    // initialize simulation
    init_simulation();

    // main simulation loop
    if(target_tick > 0) {
        while(g_current_tick < target_tick) {
            run_simulation_tick();
            g_current_tick++;
        }
        return 0;
    } else {
        while(true) {
            run_simulation_tick();
            g_current_tick++;
        }
    }

    finalize_simulation();

    return 0;
}



