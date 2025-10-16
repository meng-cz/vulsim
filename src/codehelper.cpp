
#include "codehelper.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <system_error>

/**
 * @brief Generate C++ struct definition code for a VulBundle.
 * @param vb The VulBundle object to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 *         Returns nullptr if the bundle has no members.
 */
unique_ptr<vector<string>> codeGenerateBundleStruct(VulBundle &vb) {
    if (vb.members.empty()) return nullptr;

    auto code = std::make_unique<vector<string>>();
    code->push_back("typedef struct {");
    for (const VulArgument &m : vb.members) {
        string line = "    " + m.type + " " + m.name + ";";
        if (!m.comment.empty()) line += " // " + m.comment;
        code->push_back(line);
    }
    code->push_back("} " + vb.name + ";");
    return std::move(code);
}


/**
 * @brief Get helper code lines for a combine.
 * This is the local variable/function declarations and initializations needed for user to code a tick/init/service.
 * This includes code for configs, storages, and function signatures.
 * @param vc The VulCombine object to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 *         Returns nullptr if no helper lines are needed.
 */
unique_ptr<vector<string>> codeGenerateHelperLines(VulCombine &vc) {
    auto code = std::make_unique<vector<string>>();

    code->push_back("// Helper code generated for combine: " + vc.name);
    code->push_back("");
    code->push_back("// All local variables and function declarations available for tick/init/service are below");
    code->push_back("// Please do not modify the code outside your function body");
    code->push_back("");

    code->push_back("#include \"common.h\"");
    code->push_back("#include \"global.h\"");
    code->push_back("#include \"bundle.h\"");
    code->push_back("");

    // Config declarations
    // /* $comment$ */
    // const int64 $name$ = $ref$;
    if (!vc.config.empty()) code->push_back("// Config declarations:");
    for (auto &c : vc.config) {
        string cmt = (c.comment.empty() ? (string("// ") + c.name) : (string("// ") + c.comment));
        code->push_back(cmt);
        code->push_back("const int64 " + c.name + " = " + c.value + ";");
        code->push_back("");
    }

    // Pipein declarations
    // /* $comment$ */
    // bool $name$_can_pop();
    // /* $comment$ */
    // $type$ (&) $name$_top();
    // /* $comment$ */
    // void $name$_pop();
    if (!vc.pipein.empty()) code->push_back("// Pipein declarations");
    for (auto &p : vc.pipein) {
        string cmt = (p.comment.empty() ? (string("// ") + p.name) : (string("// ") + p.comment));
        code->push_back(cmt);
        code->push_back("bool " + p.name + "_can_pop();");
        code->push_back(cmt);
        if (isBasicVulType(p.type)) {
            code->push_back(p.type + " " + p.name + "_top();");
        } else {
            code->push_back(p.type + " & " + p.name + "_top();");
        }
        code->push_back(cmt);
        code->push_back("void " + p.name + "_pop();");
        code->push_back("");
    }

    if (!vc.pipeout.empty()) code->push_back("// Pipeout declarations");
    // Pipeout declarations
    // /* $comment$ */
    // bool $name$_can_push();
    // /* $comment$ */
    // void $name$_push($type$ (&) value);
    for (auto &p : vc.pipeout) {
        string cmt = (p.comment.empty() ? (string("// ") + p.name) : (string("// ") + p.comment));
        code->push_back(cmt);
        code->push_back("bool " + p.name + "_can_push();");
        code->push_back(cmt);
        if (isBasicVulType(p.type)) {
            code->push_back("void " + p.name + "_push(" + p.type + " value);");
        } else {
            code->push_back("void " + p.name + "_push(" + p.type + " & value);");
        }
        code->push_back("");
    }

    // Storage declarations
    // /* $comment$ */
    // $type$ $name$( = $value$);
    if (!vc.storage.empty()) code->push_back("// Storage declarations");
    for (auto &s : vc.storage) {
        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        code->push_back(cmt);
        if (s.value.empty() ) {
            code->push_back(s.type + " " + s.name + ";");
        } else {
            code->push_back(s.type + " " + s.name + " = " + s.value + ";");
        }
        code->push_back("");
    }

    // Storagenext declarations
    // /* $comment$ */
    // $type$ (&) $name$_get();
    // /* $comment$ */
    // void $name$_setnext($type$ (&) value, uint8 priority);
    if (!vc.storagenext.empty()) code->push_back("// Storagenext declarations");
    for (auto &s : vc.storagenext) {
        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        code->push_back(cmt);
        if (isBasicVulType(s.type)) {
            code->push_back(s.type + " " + s.name + "_get();");
            code->push_back(cmt);
            code->push_back("void " + s.name + "_setnext(" + s.type + " value, uint8 priority);");
        } else {
            code->push_back(s.type + " & " + s.name + "_get();");
            code->push_back(cmt);
            code->push_back("void " + s.name + "_setnext(" + s.type + " & value, uint8 priority);");
        }
        code->push_back("");
    }

    // Storagetick declarations
    // /* $comment$ */
    // $type$ $name$ = $value$;
    if (!vc.storagetick.empty()) code->push_back("// Storagetick declarations");
    for (auto &s : vc.storagetick) {
        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        code->push_back(cmt);
        code->push_back(s.type + " " + s.name + " = " + s.value + ";");
        code->push_back("");
    }

    // Request/Service function declarations
    // /*
    // * $comment$
    // * @arg $argname$: $argcomment$
    // * @ret $retname$: $retcomment$
    // */
    // void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...);

    auto _genFunc = [&](const string &fname, const vector<VulArgument> &args, const vector<VulArgument> &rets, const string &comment) {
        string sig = codeGenerateFunctionSignature(fname, args, rets);
        string cmtstr = (comment.empty() ? fname : comment);
        code->push_back("/*");
        code->push_back(" * " + cmtstr);
        for (const VulArgument &a : args) {
            string acmt = (a.comment.empty() ? a.name : a.comment);
            code->push_back(" * @arg " + a.name + ": " + acmt);
        }
        for (const VulArgument &ret : rets) {
            string rcmt = (ret.comment.empty() ? ret.name : ret.comment);
            code->push_back(" * @ret " + ret.name + ": " + rcmt);
        }
        code->push_back(" */");
        code->push_back(sig + ";");
    };

    if (!vc.request.empty()) code->push_back("// Request function declarations");
    for (auto &r : vc.request) {
        _genFunc(r.name, r.arg, r.ret, r.comment);
    }

    if (!vc.service.empty()) code->push_back("// Service function declarations");
    for (auto &s : vc.service) {
        _genFunc(s.name, s.arg, s.ret, s.comment);
    }

    // Stallable
    if( vc.stallable) {
        code->push_back("");
        code->push_back("// This combine is marked as stallable");
        code->push_back("// You can check is_stalled() to see if the combine is currently stalled");
        code->push_back("bool is_stalled();");
        code->push_back("// You can call stall() mark the combine as stalled for this tick");
        code->push_back("void stall();");
    }

    code->push_back("");
    code->push_back("// Please do not modify the code outside the target function body");
    code->push_back("// End of helper code");
    code->push_back("");
    
    code->push_back("// Target function starts below:");
    code->push_back("");

    return std::move(code);
}

/**
 * @brief Generate a C++ function argument signature from arguments and return values.
 * @param args The vector of VulArgument representing the function arguments.
 * @param rets The vector of VulArgument representing the function return values.
 * @return The generated C++ function argument signature as a string.
 */
string codeGenerateFunctionArgumentSignature(const vector<VulArgument> &args, const vector<VulArgument> &rets) {
    std::ostringstream oss;
    bool first = true;
    for (const VulArgument &a : args) {
        if (!first) oss << ", ";
        first = false;
        if (isBasicVulType(a.type)) {
            oss << a.type << " " << a.name;
        } else {
            oss << a.type << " & " << a.name;
        }
    }
    for (const VulArgument &ret : rets) {
        if (!first) oss << ", ";
        first = false;
        if (isBasicVulType(ret.type)) {
            oss << ret.type << " * " << ret.name;
        } else {
            oss << ret.type << " * " << ret.name;
        }
    }
    return oss.str();
}

/**
 * @brief Generate a C++ function signature from function name, arguments, and return values.
 * @param funcname The name of the function.
 * @param args The vector of VulArgument representing the function arguments.
 * @param rets The vector of VulArgument representing the function return values.
 * @return The generated C++ function signature as a string.
 */
string codeGenerateFunctionSignature(const string &funcname, const vector<VulArgument> &args, const vector<VulArgument> &rets) {
    return "void " + funcname + "(" + codeGenerateFunctionArgumentSignature(args, rets) + ")";
}

/**
 * @brief Generate C++ header file code bundle.h for multiple VulBundle definitions.
 * @param bundles A vector of VulBundle objects to generate code for.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 */
unique_ptr<vector<string>> codeGenerateBundleHeaderFile(vector<VulBundle> &bundles) {
    auto code = std::make_unique<vector<string>>();
    code->push_back("// Auto-generated bundle.h");
    code->push_back("// Please do not modify this file directly");
    code->push_back("");
    code->push_back("#pragma once");
    code->push_back("");
    code->push_back("#include \"common.h\"");
    code->push_back("");
    for (VulBundle &vb : bundles) {
        auto structcode = codeGenerateBundleStruct(vb);
        if (structcode) {
            for (const string &line : *structcode) {
                code->push_back(line);
            }
            code->push_back("");
        }
    }
    return std::move(code);
}

/**
 * @brief Generate C++ header file code bundle.h for multiple VulBundle definitions.
 * @param bundles A map of VulBundle objects to generate code for, indexed by bundle name.
 * @return A unique_ptr to a vector of strings, each string is a line of C++ code.
 */
unique_ptr<vector<string>> codeGenerateBundleHeaderFile(unordered_map<string, VulBundle> &bundles) {
    vector<VulBundle> vec;
    for (auto &bk : bundles) {
        vec.push_back(bk.second);
    }
    return codeGenerateBundleHeaderFile(vec);
}


/**
 * @brief Extract the body of a function from a C++ source file.
 * The function is identified by its name. The body includes all lines between the opening and closing braces.
 * @param filename The name of the C++ source file to read.
 * @param funcname The name of the function whose body to extract.
 * @param err Error message in case of failure.
 * @return A unique_ptr to a vector of strings, each string is a line of the function body.
 *         Returns nullptr on failure, with err set to an appropriate error message.
 */
unique_ptr<vector<string>> codeExtractFunctionBodyFromFile(const string &filename, const string &funcname, string &err) {
    err.clear();
    if (filename.empty()) { err = "empty filename"; return nullptr; }
    if (funcname.empty()) { err = "empty function name"; return nullptr; }

    std::ifstream ifs(filename);
    if (!ifs) { err = string("cannot open file: ") + filename; return nullptr; }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    size_t n = content.size();

    // Build a cleaned buffer where comments and string/char literals are replaced by spaces
    std::string cleaned(n, ' ');
    bool in_scomment = false;
    bool in_mcomment = false;
    bool in_string = false;
    bool in_char = false;
    for (size_t i = 0; i < n; ++i) {
        char c = content[i];
        if (in_scomment) {
            cleaned[i] = (c == '\n') ? '\n' : ' ';
            if (c == '\n') in_scomment = false;
            continue;
        }
        if (in_mcomment) {
            cleaned[i] = ' ';
            if (c == '*' && i + 1 < n && content[i+1] == '/') {
                cleaned[i+1] = ' ';
                in_mcomment = false;
                ++i;
            }
            continue;
        }
        if (in_string) {
            cleaned[i] = ' ';
            if (c == '\\' && i + 1 < n) { cleaned[i+1] = ' '; ++i; continue; }
            if (c == '"') { in_string = false; }
            continue;
        }
        if (in_char) {
            cleaned[i] = ' ';
            if (c == '\\' && i + 1 < n) { cleaned[i+1] = ' '; ++i; continue; }
            if (c == '\'') { in_char = false; }
            continue;
        }

        // not inside any special region
        if (c == '/' && i + 1 < n && content[i+1] == '/') {
            cleaned[i] = ' '; cleaned[i+1] = ' ';
            in_scomment = true; ++i; continue;
        }
        if (c == '/' && i + 1 < n && content[i+1] == '*') {
            cleaned[i] = ' '; cleaned[i+1] = ' ';
            in_mcomment = true; ++i; continue;
        }
        if (c == '"') { cleaned[i] = ' '; in_string = true; continue; }
        if (c == '\'') { cleaned[i] = ' '; in_char = true; continue; }

        cleaned[i] = c;
    }

    size_t pos = 0;
    const size_t flen = funcname.size();
    while (true) {
        pos = cleaned.find(funcname, pos);
        if (pos == std::string::npos) break;
        // ensure token boundaries
        bool left_ok = (pos == 0) || !(std::isalnum((unsigned char)cleaned[pos-1]) || cleaned[pos-1] == '_');
        bool right_ok = (pos + flen >= n) || !(std::isalnum((unsigned char)cleaned[pos+flen]) || cleaned[pos+flen] == '_');
        if (!left_ok || !right_ok) { pos += flen; continue; }

        // find '(' after function name
        size_t idx = pos + flen;
        while (idx < n && std::isspace((unsigned char)cleaned[idx])) ++idx;
        if (idx >= n || cleaned[idx] != '(') { pos += flen; continue; }

        // parse balanced parentheses to find closing ')'
        size_t p = idx;
        int depth = 0;
        bool paren_ok = false;
        while (p < n) {
            char cc = cleaned[p];
            if (cc == '(') { ++depth; }
            else if (cc == ')') { --depth; if (depth == 0) { paren_ok = true; ++p; break; } }
            ++p;
        }
        if (!paren_ok) { pos += flen; continue; }

        // from p onward, skip whitespace and tokens until we hit '{' (definition) or ';' (declaration)
        size_t q = p;
        bool foundBrace = false;
        while (q < n) {
            char cc = cleaned[q];
            if (std::isspace((unsigned char)cc)) { ++q; continue; }
            if (cc == ';') { break; } // declaration
            if (cc == '{') { foundBrace = true; break; }
            if (cc == '(') {
                // skip balanced parentheses
                int d2 = 1; ++q;
                while (q < n && d2 > 0) { if (cleaned[q] == '(') ++d2; else if (cleaned[q] == ')') --d2; ++q; }
                continue;
            }
            if (cc == '[') {
                // skip attribute bracket
                ++q; while (q < n && cleaned[q] != ']') ++q; if (q < n) ++q; continue;
            }
            if (cc == '-') {
                // possibly '->' trailing return; skip the '->' and subsequent identifier/token
                if (q + 1 < n && cleaned[q+1] == '>') { q += 2; continue; }
            }
            // other tokens: just advance
            ++q;
        }

        if (!foundBrace) { pos += flen; continue; }

        size_t brace_open = q;
        // find matching closing brace
        size_t r = brace_open + 1;
        int bdepth = 1;
        while (r < n && bdepth > 0) {
            if (cleaned[r] == '{') ++bdepth;
            else if (cleaned[r] == '}') --bdepth;
            ++r;
        }
        if (bdepth != 0) { err = string("unmatched braces for function '") + funcname + "' in file " + filename; return nullptr; }

        size_t body_start = brace_open + 1;
        size_t body_end_exclusive = r - 1; // r-1 is the closing '}' index, exclusive end is r-1
        if (body_start > body_end_exclusive) {
            return std::make_unique<vector<string>>();
        }

        std::string body = content.substr(body_start, body_end_exclusive - body_start);
        auto out = std::make_unique<vector<string>>();
        std::istringstream iss(body);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            out->push_back(line);
        }
        return out;
    }

    err = string("function '") + funcname + "' definition not found in file " + filename;
    return nullptr;
}




