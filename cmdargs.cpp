
#include "common.h"

typedef struct {
    array<vector<string>, 26> args;
    vector<string> _default;
} CmdArgs;

static CmdArgs & get_global_cmdargs() {
    static CmdArgs args;
    return args;
}

vector<string> & get_cmdline_args(const char prefix) {
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

    for(int i = 0; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] >= 'a' && argv[i][1] <= 'z') {
            prefix = argv[i][1];
        } else if(argv[i][0] == '-' && argv[i][1] >= 'A' && argv[i][1] <= 'Z') {
            for(int n = i + 1; n < argc; n++) {
                args.args[argv[i][1] - 'A'].push_back(string(argv[n]));
            }
            break;
        } else {
            args.args[prefix - 'a'].push_back(string(argv[i]));
        }
    }

}




