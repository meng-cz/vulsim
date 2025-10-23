/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "codegen.h"
#include "codehelper.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <cctype>

namespace fs = std::filesystem;

enum class PipeType {
    def_pipe,
    simple_handshake
};

PipeType detectPipeType(const VulPipe &vp) {
    if (vp.inputsize == 1 && vp.outputsize == 1 && vp.buffersize == 0) {
        return PipeType::simple_handshake;
    }
    return PipeType::def_pipe;
}

/**
 * @brief Generate C++ header code for all bundles in the design.
 * @param design The VulDesign object containing the bundles.
 * @param headerlines Output vector to hold the generated header lines.
 * @param with_pragma_once Whether to include #pragma once at the top of the header.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 */
unique_ptr<vector<string>> codegenBundlesHeader(VulDesign &design, vector<string> &headerlines, bool with_pragma_once) {

    headerlines.push_back("// Auto-generated bundle.h");
    headerlines.push_back("// Please do not modify this file directly");
    headerlines.push_back("");
    if (with_pragma_once) {
        headerlines.push_back("#pragma once");
        headerlines.push_back("");
    }
    headerlines.push_back("#include \"common.h\"");
    headerlines.push_back("");
    for (auto &vbentry : design.bundles) {
        VulBundle &vb = vbentry.second;
        headerlines.push_back("#ifndef BUNDLE_" + vb.name + "_H");
        headerlines.push_back("#define BUNDLE_" + vb.name + "_H");
        if (!vb.members.empty()) {
            headerlines.push_back("typedef struct {");
            for (const VulArgument &m : vb.members) {
                string line = "    " + m.type + " " + m.name + ";";
                if (!m.comment.empty()) line += " // " + m.comment;
                headerlines.push_back(line);
            }
            headerlines.push_back("} " + vb.name + ";");
        }
        headerlines.push_back("#endif // BUNDLE_" + vb.name + "_H");
        headerlines.push_back("");
    }

    return nullptr;
}


/**
 * @brief Generate C++ code for simulating a combine.
 * This includes generating the header lines ({combine_name}.h) and the C++ source lines ({combine_name}.cpp).
 * @param design The VulDesign object containing the combine and related definitions.
 * @param combine The name of the combine to generate code for.
 * @param headerlines Output vector to hold the generated header lines.
 * @param cpplines Output vector to hold the generated C++ source lines.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 *        Returns nullptr on success (no errors).
 */
unique_ptr<vector<string>> codegenCombine(VulDesign &design, const string &combine, vector<string> &headerlines, vector<string> &cpplines) {
    unique_ptr<vector<string>> err = std::make_unique<vector<string>>();
    headerlines.clear();
    cpplines.clear();

    // init code fields
    vector<string> constructor_args_field;
    vector<string> constructor_field;
    vector<string> deconstructor_field;
    vector<string> init_field;
    vector<string> member_field;
    vector<string> function_field;
    vector<string> tick_field;
    vector<string> applytick_field;
    vector<string> user_tick_field;
    vector<string> user_applytick_field;

    // get combine
    auto it = design.combines.find(combine);
    if (it == design.combines.end()) {
        err->push_back(string("Error: combine not found: ") + combine);
        return err;
    }

    // prepare pathes
    fs::path project_dir = design.project_dir;
    fs::path cpp_dir = project_dir / "cpp";

    VulCombine &vc = it->second;

    // generate pipein
    for (const VulPipePort &pp : vc.pipein) {
        // Member Field +：
        // PipePopPort<$type$> * _pipein_$name$;
        // /* $comment$ */
        // bool $name$_can_pop() { return _pipein_$name$->can_pop(); };
        // /* $comment$ */
        // $type$ (&) $name$_top() { return _pipein_$name$->top(); };
        // /* $comment$ */
        // void $name$_pop() { _pipein_$name$->pop(); };

        // Constructor Arguments Field +:
        // PipePopPort<$type$> * _pipein_$name$;

        // Constructor Field +:
        // this->_pipein_$name$ = arg._pipein_$name$;

        string cmtstr = (pp.comment.empty() ? (pp.name) : (pp.comment));
        string typestr = (isBasicVulType(pp.type) ? pp.type : (pp.type + " &"));
        member_field.push_back("PipePopPort<" + pp.type + "> * _pipein_" + pp.name + ";");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("bool " + pp.name + "_can_pop() { return _pipein_" + pp.name + "->can_pop(); };");
        member_field.push_back("// " + cmtstr);
        member_field.push_back(typestr + " " + pp.name + "_top() { return _pipein_" + pp.name + "->top(); };");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("void " + pp.name + "_pop() { _pipein_" + pp.name + "->pop(); };");
        member_field.push_back("");
        constructor_args_field.push_back("PipePopPort<" + pp.type + "> * _pipein_" + pp.name + ";");
        constructor_field.push_back("this->_pipein_" + pp.name + " = arg._pipein_" + pp.name + ";");
    }

    // generate pipeout
    for (const VulPipePort &pp : vc.pipeout) {
        // Member Field +:
        // PipePushPort<$type$> * _pipeout_$name$;
        // /* $comment$ */
        // bool $name$_can_push() { return _pipeout_$name$->can_push(); } ;
        // /* $comment$ */
        // void $name$_push($type$ (&) value) { _pipeout_$name$->push(value) } ;

        // Constructor Arguments Field +:
        // PipePushPort<$type$> * _pipeout_$name$;

        // Constructor Field +:
        // this->_pipeout_$name$ = arg._pipeout_$name$;

        string cmtstr = (pp.comment.empty() ? (pp.name) : (pp.comment));
        string typestr = (isBasicVulType(pp.type) ? pp.type : (pp.type + " &"));
        member_field.push_back("PipePushPort<" + pp.type + "> * _pipeout_" + pp.name + ";");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("bool " + pp.name + "_can_push() { return _pipeout_" + pp.name + "->can_push(); } ;");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("void " + pp.name + "_push(" + typestr + " value) { _pipeout_" + pp.name + "->push(value); } ;");
        member_field.push_back("// " + cmtstr);
        constructor_args_field.push_back("PipePushPort<" + pp.type + "> * _pipeout_" + pp.name + ";");
        constructor_field.push_back("this->_pipeout_" + pp.name + " = arg._pipeout_" + pp.name + ";");
    }

    auto _pushArgComments = [&](const vector<VulArgument> &args, vector<string> &field, string prefix) {
        for (const VulArgument &a : args) {
            string cmt = (a.comment.empty() ? a.name : a.comment);
            field.push_back(prefix + "<" + a.name + "> " + cmt);
        }
    };

    // generate request
    for (const VulRequest &r : vc.request) {
        // Member Field +：
        // void (*_request_$name$)($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
        // /*
        // * $comment$
        // * @arg $argname$: $argcomment$
        // * @ret $retname$: $retcomment$
        // */
        // void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...) { _request_$name($argname$, $retname$, ...); };
        
        // Constructor Arguments Field +:
        // void (*_request_$name$)($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
        
        // Constructor Field +:
        // this->_request_$name$ = arg._request_$name$;
        string cmtstr = (r.comment.empty() ? (r.name) : (r.comment));
        string argsig = codeGenerateFunctionArgumentSignature(r.arg, r.ret);
        member_field.push_back("void (*_request_" + r.name + ")(" + argsig + ");");
        member_field.push_back("/*");
        member_field.push_back(" * " + cmtstr);
        _pushArgComments(r.arg, member_field, " * @arg ");
        _pushArgComments(r.ret, member_field, " * @ret ");
        member_field.push_back(" */");
        string localcall = "_request_" + r.name + "(";
        {
            bool first = true;
            for (const VulArgument &a : r.arg) {
                if (!first) localcall += ", ";
                first = false;
                localcall += a.name;
            }
            for (const VulArgument &ret : r.ret) {
                if (!first) localcall += ", ";
                first = false;
                localcall += "&" + ret.name;
            }
            localcall += ");";
        }
        member_field.push_back("void " + r.name + "(" + argsig + ") { " + localcall + " };");
        member_field.push_back("");
        constructor_args_field.push_back("void (*_request_" + r.name + ")(" + argsig + ");");
        constructor_field.push_back("this->_request_" + r.name + " = arg._request_" + r.name + ";");
    }

    // generate service
    for (const VulService &s : vc.service) {
        // Member Field +：
        // /*
        //  * $comment$
        //  * @arg $argname$: $argcomment$
        //  * @ret $retname$: $retcomment$
        // */
        // void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...);

        // Function Field +:
        // void $cname$::$name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...) {
        //     $cppfunc$
        // };

        string cmtstr = (s.comment.empty() ? (s.name) : (s.comment));
        string argsig = codeGenerateFunctionArgumentSignature(s.arg, s.ret);
        string localerr;
        string cppfilepath = (cpp_dir / s.cppfunc.file).string();
        auto funcbody = codeExtractFunctionBodyFromFile(cppfilepath, s.cppfunc.name, localerr);
        if (!funcbody) {
            if(s.cppfunc.code.empty()) {
                err->push_back(string("Error: failed to extract service function body for combine '") + combine + "' service '" + s.name + "': " + localerr);
                return err;
            } else {
                funcbody = std::make_unique<vector<string>>();
                funcbody->push_back(s.cppfunc.code);
            }
        }
        member_field.push_back("/*");
        member_field.push_back(" * " + cmtstr);
        _pushArgComments(s.arg, member_field, " * @arg ");
        _pushArgComments(s.ret, member_field, " * @ret ");
        member_field.push_back(" */");
        member_field.push_back("void " + s.name + "(" + argsig + ");");
        member_field.push_back("");
        function_field.push_back("void " + combine + "::" + s.name + "(" + argsig + ") {");
        for (const string &line : *funcbody) {
            function_field.push_back(line);
        }
        function_field.push_back("}");
        function_field.push_back("");
    }

    // generate storage 
    for (const VulStorage &s : vc.storage) {
        // Member Field +：
        // /* $comment$ */
        // $type$ $name$( = $value$);

        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        string valuestr = s.value.empty() ? "" : (" = " + s.value);
        member_field.push_back(cmt);
        member_field.push_back(s.type + " " + s.name + valuestr + ";");
    }

    // generate storagenext
    for (const VulStorage &s : vc.storagenext) {
        // Member Field +：
        // StorageNext<$type$> _storagenext_$name$( = StorageNext<$type$>($value$));
        // /* $comment$ */
        // $type$ (&) $name$_get() { return _storagenext_$name$.get(); };
        // /* $comment$ */
        // void $name$_setnext($type$ (&) value, uint8 priority) { _storagenext_$name$.setnext(value, priority); } ;
        
        // Applytick Field +:
        // _storagenext_$name$.apply_tick();

        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        string valuestr = s.value.empty() ? "" : (" = StorageNext<" + s.type + ">(" + s.value + ")");
        string typestr = (isBasicVulType(s.type) ? s.type : (s.type + " &"));
        member_field.push_back("StorageNext<" + s.type + "> _storagenext_" + s.name + valuestr + ";");
        member_field.push_back("// " + cmt);
        member_field.push_back(typestr + " " + s.name + "_get() { return _storagenext_" + s.name + ".get(); };");
        member_field.push_back("// " + cmt);
        member_field.push_back("void " + s.name + "_setnext(" + typestr + " value, uint8 priority) { _storagenext_" + s.name + ".setnext(value, priority); } ;");
        member_field.push_back("");
        applytick_field.push_back("_storagenext_" + s.name + ".apply_tick();");
    }

    // generate storagetick
    for (const VulStorage &s : vc.storagetick) {
        // Member Field +：
        // /* $comment$ */
        // $type$ $name$ = $value$;

        // Applytick Field +:
        // $name$ = $value$;
        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        member_field.push_back(cmt);
        member_field.push_back(s.type + " " + s.name + " = " + s.value + ";");
        member_field.push_back("");
        applytick_field.push_back(s.name + " = " + s.value + ";");
    }

    // generate storagenextarray
    for (const VulStorageArray &s : vc.storagenextarray) {
        // Member Field +：
        // StorageNextArray<$type$> _storagenextarray_$name$;
        // /* $comment$ */
        // $type$ (&) $name$_get(int64 index) { return _storagenextarray_$name$.get(index); };
        // /* $comment$ */
        // void $name$_setnext(int64 index, $type$ (&) value, uint8 priority) { _storagenextarray_$name$.setnext(index, value, priority); } ;
        
        // Constructor Field +:
        // _storagenextarray_$name$ = StorageNextArray<$type$>(size (, value));
        
        // Applytick Field +:
        // _storagenextarray_$name$.apply_tick();
        string cmt = (s.comment.empty() ? (string("// ") + s.name) : (string("// ") + s.comment));
        string typestr = (isBasicVulType(s.type) ? s.type : (s.type + " &"));
        member_field.push_back("StorageNextArray<" + s.type + "> _storagenextarray_" + s.name + ";");
        member_field.push_back("// " + cmt);
        member_field.push_back(typestr + " " + s.name + "_get(int64 index) { return _storagenextarray_" + s.name + ".get(index); };");
        member_field.push_back("// " + cmt);
        member_field.push_back("void " + s.name + "_setnext(int64 index, " + typestr + " value, uint8 priority) { _storagenextarray_" + s.name + ".setnext(index, value, priority); } ;");
        member_field.push_back("");
        if (s.value.empty()) {
            constructor_field.push_back("this->_storagenextarray_" + s.name + " = StorageNextArray<" + s.type + ">(" + s.size + ");");
        } else {
            constructor_field.push_back("this->_storagenextarray_" + s.name + " = StorageNextArray<" + s.type + ">(" + s.size + ", " + s.value + ");");
        }
        applytick_field.push_back("_storagenextarray_" + s.name + ".apply_tick();");
    }

    // generate config
    for (const VulConfig &c : vc.config) {
        // Member Field +：
        // /* $comment$ */
        // const int64 $name$;
        string cmt = (c.comment.empty() ? (string("// ") + c.name) : (string("// ") + c.comment));
        member_field.push_back(cmt);
        member_field.push_back("int64 " + c.name + ";");
        // Constructor Arguments Field +:
        // int64 $name$;
        constructor_args_field.push_back("int64 " + c.name + ";");
        // Constructor Field +:
        // this->$name$ = arg.$name$;
        constructor_field.push_back("this->" + c.name + " = arg." + c.name + ";");
    }

    auto _extractCodeIntoField = [&](const VulCppFunc &cf, vector<string> &field) {
        if (cf.code.empty() && cf.file.empty() && cf.name.empty()) return;
        string localerr;
        string cppfilepath = (cpp_dir / cf.file).string();
        auto funcbody = codeExtractFunctionBodyFromFile(cppfilepath, cf.name, localerr);
        if (!funcbody) {
            if(cf.code.empty()) {
                err->push_back(string("Error: failed to extract function body for combine '") + combine + "': " + localerr);
                return;
            } else {
                funcbody = std::make_unique<vector<string>>();
                funcbody->push_back(cf.code);
            }
        }
        for (const string &line : *funcbody) {
            field.push_back(line);
        }
    };

    // generate tick
    _extractCodeIntoField(vc.tick, user_tick_field);

    // generate applytick
    _extractCodeIntoField(vc.applytick, user_applytick_field);

    // generate init
    _extractCodeIntoField(vc.init, init_field);

    // generate stallable
    if (vc.stallable) {
        // Member Field +：
        // bool _stalled = false;
        // void (*_external_stall)();
        // void stall() { _stalled = true; if(_external_stall) _external_stall(); }
        // bool is_stalled() { return _stalled; }

        // Constructor Arguments Field +:
        // void (*_external_stall)();

        // Constructor Field +:
        // this->_external_stall = arg._external_stall;

        // Applytick Field +:
        // stalled = false;

        member_field.push_back("bool _stalled = false;");
        member_field.push_back("void (*_external_stall)();");
        member_field.push_back("void stall() { _stalled = true; if(_external_stall) _external_stall(); }");
        member_field.push_back("bool is_stalled() { return _stalled; }");
        member_field.push_back("");
        constructor_args_field.push_back("void (*_external_stall)();");
        constructor_field.push_back("this->_external_stall = arg._external_stall;");
        applytick_field.push_back("_stalled = false;");
    }

    // all fields generated, now output to headerlines and cpplines
    // headerlines
    headerlines.push_back("#pragma once");
    headerlines.push_back("");
    headerlines.push_back("#include \"common.h\"");
    headerlines.push_back("#include \"global.h\"");
    headerlines.push_back("#include \"bundle.h\"");
    headerlines.push_back("#include \"vulsimlib.h\"");
    headerlines.push_back("");
    headerlines.push_back("// Combine: " + vc.name);
    headerlines.push_back("// " + vc.comment);
    headerlines.push_back("class " + vc.name + " {");
    headerlines.push_back("public:");
    headerlines.push_back("    class ConstructorArguments {");
    headerlines.push_back("    public:");
    for (const string &line : constructor_args_field) headerlines.push_back("        " + line);
    headerlines.push_back("        int32 _dummy = 0; // to avoid empty class");
    headerlines.push_back("    };");
    headerlines.push_back("");
    headerlines.push_back("    " + vc.name + "(ConstructorArguments & arg);");
    headerlines.push_back("    ~" + vc.name + "();");
    headerlines.push_back("");
    headerlines.push_back("    void init();");
    headerlines.push_back("    void all_current_tick();");
    headerlines.push_back("    void all_current_applytick();");
    headerlines.push_back("    void user_current_tick();");
    headerlines.push_back("    void user_current_applytick();");
    headerlines.push_back("");
    for (const string &line : member_field) headerlines.push_back("    " + line);
    headerlines.push_back("");
    headerlines.push_back("};");
    headerlines.push_back("");

    // cpplines
    cpplines.push_back("#include \"" + vc.name + ".h\"");
    cpplines.push_back("");
    cpplines.push_back(vc.name + "::" + vc.name + "(ConstructorArguments & arg) {");
    for (const string &line : constructor_field) cpplines.push_back("    " + line);
    cpplines.push_back("    init();");
    cpplines.push_back("}");
    cpplines.push_back("");
    cpplines.push_back(vc.name + "::~" + vc.name + "() {");
    for (const string &line : deconstructor_field) cpplines.push_back("    " + line);
    cpplines.push_back("}");
    cpplines.push_back("");
    cpplines.push_back("void " + vc.name + "::init() {");
    for (const string &line : init_field) cpplines.push_back("    " + line);
    cpplines.push_back("}");
    cpplines.push_back("");
    for (const string &line : function_field) cpplines.push_back(line);
    cpplines.push_back("void " + vc.name + "::all_current_tick() {");
    for (const string &line : tick_field) cpplines.push_back("    " + line);
    cpplines.push_back("    user_current_tick();");
    cpplines.push_back("}");
    cpplines.push_back("");
    cpplines.push_back("void " + vc.name + "::all_current_applytick() {");
    for (const string &line : applytick_field) cpplines.push_back("    " + line);
    cpplines.push_back("    user_current_applytick();");
    cpplines.push_back("}");
    cpplines.push_back("");
    cpplines.push_back("void " + vc.name + "::user_current_tick() {");
    for (const string &line : user_tick_field) cpplines.push_back("    " + line);
    cpplines.push_back("}");
    cpplines.push_back("");
    cpplines.push_back("void " + vc.name + "::user_current_applytick() {");
    for (const string &line : user_applytick_field) cpplines.push_back("    " + line);
    cpplines.push_back("}");
    cpplines.push_back("");

    return nullptr;
}

/**
 * @brief Generate a C++ value statement from a VulDesign context and a value string.
 * @param design The VulDesign object containing the context.
 * @param value The value string to convert.
 * @param out The output string to hold the generated C++ value statement.
 * @return The error message string if any error occurs, empty string on success.
 */
string _codegenConfigValueStatement(const VulDesign &design, const string &value, string &out) {
    // Simple lexer: split into identifiers, numbers, operators, and parentheses
    out.clear();
    size_t i = 0;
    while (i < value.size()) {
        char c = value[i];
        if (std::isspace((unsigned char)c)) { out.push_back(c); i++; continue; }
        if (std::isalpha((unsigned char)c) || c == '_') {
            // identifier
            size_t j = i + 1;
            while (j < value.size() && (std::isalnum((unsigned char)value[j]) || value[j] == '_')) j++;
            string ident = value.substr(i, j - i);
            // if ident is in design config lib
            if (design.config_lib.find(ident) != design.config_lib.end()) {
                // design config, replace with get_design_config_item("ident")
                out += string("get_design_config_item(\"") + ident + string("\")");
                i = j;
                continue;
            }
            // else, unknown ident, report error
            return string("Error: unknown identifier in config value: ") + ident;
        }
        if (std::isdigit((unsigned char)c)) {
            // number literal (integer)
            size_t j = i + 1;
            while (j < value.size() && (
                std::isdigit((unsigned char)value[j]) ||
                value[j] == 'x' || value[j] == 'X' ||
                value[j] == 'u' || value[j] == 'U' ||
                value[j] == 'l' || value[j] == 'L'
            )) j++; // allow hex (0x), unsigned (u), long (l) suffixes
            out += value.substr(i, j - i);
            i = j;
            continue;
        }
        // operators or punctuation: copy as-is (handles + - * / % ( ) etc.)
        out.push_back(c);
        i++;
    }
    return "";
}

/**
 * @brief Generate C++ code for simulating an entire design.
 * @param design The VulDesign object containing the entire design.
 * @param cpplines Output vector to hold the generated C++ source lines for simulation.
 * @return A unique_ptr to a vector of strings containing any error messages encountered during generation.
 *        Returns nullptr on success (no errors).
 */
unique_ptr<vector<string>> codegenSimulation(VulDesign &design, vector<string> &cpplines) {
    unique_ptr<vector<string>> err = std::make_unique<vector<string>>();
    cpplines.clear();

    // headers
    cpplines.push_back("#include \"simulation.h\"");
    cpplines.push_back("#include \"common.h\"");
    cpplines.push_back("#include \"global.h\"");
    cpplines.push_back("#include \"bundle.h\"");
    cpplines.push_back("#include \"vulsimlib.h\"");
    cpplines.push_back("#include <memory>");
    cpplines.push_back("using std::make_unique;");
    cpplines.push_back("using std::unique_ptr;");
    cpplines.push_back("");
    for (const auto &combinepair : design.combines) {
        cpplines.push_back("#include \"" + combinepair.first + ".h\"");
    }
    cpplines.push_back("");
    // prefab headers
    for (const auto &prefabpair : design.prefabs) {
        cpplines.push_back("#include \"" + prefabpair.first + ".h\"");
    }

    // instance pointer declarations
    // unique_ptr<$combine$> _instance_$name$;
    for (const auto &instancepair : design.instances) {
        const string &instname = instancepair.first;
        const VulInstance &vi = instancepair.second;
        cpplines.push_back("unique_ptr<" + vi.combine + "> _instance_" + instname + ";");
    }
    cpplines.push_back("");

    // pipe pointer declarations
    // unique_ptr<Pipe<$type$, $buf$, $in$, $out$>> _pipe_$name$;
    // unique_ptr<SimpleHandshakePipe<$type$> _pipe_$name$;
    for (const auto &pipepair : design.pipes) {
        const string &pipename = pipepair.first;
        const VulPipe &vp = pipepair.second;
        PipeType type = detectPipeType(vp);
        if (type == PipeType::simple_handshake) {
            cpplines.push_back("unique_ptr<SimpleHandshakePipe<" + vp.type + ">> _pipe_" + pipename + ";");
        } else {
            // default pipe
            cpplines.push_back("unique_ptr<Pipe<" +
                vp.type + ", "
                + std::to_string(vp.buffersize) + ", "
                + std::to_string(vp.inputsize) + ", "
                + std::to_string(vp.outputsize)
                + ">> _pipe_" + pipename + ";");
        }
    }
    cpplines.push_back("");

    // Req Connection Field
    // For each instance.request a global function is setup to call all connected service
    // void _request_$instname$_$reqname$($argtype$ (&) $argname$, ... , $rettype$ * $retname$, ... ) {
    //     _instance_$servinstname$->$servname$($argname$, ... , $retname$, ...);
    //     ......
    // }
    unordered_map<string, unordered_map<string, vector<pair<string, string>>>> req_connect_map;
    for (const auto &conn : design.req_connections) {
        auto iter1 = req_connect_map.find(conn.req_instance);
        if (iter1 == req_connect_map.end()) {
            req_connect_map[conn.req_instance] = unordered_map<string, vector<pair<string, string>>>();
            iter1 = req_connect_map.find(conn.req_instance);
        }
        auto iter2 = iter1->second.find(conn.req_name);
        if (iter2 == iter1->second.end()) {
            iter1->second[conn.req_name] = vector<pair<string, string>>();
            iter2 = iter1->second.find(conn.req_name);
        }
        iter2->second.push_back(make_pair(conn.serv_instance, conn.serv_name));
    }
    auto _genArgNames = [](const vector<VulArgument> &args, const vector<VulArgument> &rets) {
        string argnames;
        bool first = true;
        for (const VulArgument &a : args) {
            if (!first) argnames += ", ";
            first = false;
            argnames += a.name;
        }
        for (const VulArgument &a : rets) {
            if (!first) argnames += ", ";
            first = false;
            argnames += a.name;
        }
        return argnames;
    };
    for (const auto &vipair : design.instances) {
        const string &instname = vipair.first;
        const VulInstance &vi = vipair.second;
        unique_ptr<VulCombine> combineptr = design.getOrFakeCombineReadonly(vi.combine);
        if (!combineptr) {
            err->push_back(string("Error: combine not found for instance '") + instname + "': " + vi.combine);
            return err;
        }
        const VulCombine &vc = *combineptr;
        for (const VulRequest &vr : vc.request) {
            string argsig = codeGenerateFunctionArgumentSignature(vr.arg, vr.ret);
            string argnames = _genArgNames(vr.arg, vr.ret);
            cpplines.push_back("void _request_" + instname + "_" + vr.name + "(" + argsig + ") {");
            auto connit = req_connect_map.find(instname);
            if (connit != req_connect_map.end()) {
                auto reqit = connit->second.find(vr.name);
                if (reqit != connit->second.end()) {
                    for (const auto &pair : reqit->second) {
                        string servinst = pair.first;
                        string servname = pair.second;
                        if (design.instances.find(servinst) == design.instances.end()) {
                            if (design.pipes.find(servinst) != design.pipes.end() && servname == "clear") {
                                cpplines.push_back("    _pipe_" + servinst + "->clear();");
                            } else {
                                err->push_back(string("Error: combine not found for instance '") + servinst + "': " + design.instances.at(servinst).combine);
                                return err;
                            }
                        } else {
                            unique_ptr<VulCombine> servcombineptr = design.getOrFakeCombineReadonly(design.instances.at(servinst).combine);
                            VulService *vserv = nullptr;
                            for (const VulService &s : servcombineptr->service) {
                                if (s.name == servname) {
                                    vserv = const_cast<VulService *>(&s);
                                    break;
                                }
                            }
                            if (vserv) {
                                if (vserv->arg.empty() && vserv->ret.empty()) {
                                    cpplines.push_back("    _instance_" + servinst + "->" + servname + "();");
                                } else {
                                    cpplines.push_back("    _instance_" + servinst + "->" + servname + "(" + argnames + ");");
                                }
                            } else {
                                err->push_back(string("Error: service not found for combine '") + servcombineptr->name + "' service '" + servname + "'");
                                return err;
                            }
                        }
                    }
                }
            }
            cpplines.push_back("}");
            cpplines.push_back("");
        }
    }

    // Stalled Connection Field
    // For each stallable instance a global function for _external_stall() is setup to call all connected stall()
    // void _stall_$instname$() {
    //     _instance_$dstinstname$->stall();
    // }
    unordered_map<string, vector<string>> stalled_connect_map;
    for (const auto &conn : design.stalled_connections) {
        auto iter = stalled_connect_map.find(conn.src_instance);
        if (iter == stalled_connect_map.end()) {
            stalled_connect_map[conn.src_instance] = vector<string>();
            iter = stalled_connect_map.find(conn.src_instance);
        }
        iter->second.push_back(conn.dest_instance);
    }
    for (const auto &vipair : design.instances) {
        const string &instname = vipair.first;
        const VulInstance &vi = vipair.second;
        unique_ptr<VulCombine> combineptr = design.getOrFakeCombineReadonly(vi.combine);
        if (!combineptr) {
            err->push_back(string("Error: combine not found for instance '") + instname + "': " + vi.combine);
            return err;
        }
        const VulCombine &vc = *combineptr;
        if (vc.stallable) {
            cpplines.push_back("void _stall_" + instname + "() {");
            auto connit = stalled_connect_map.find(instname);
            if (connit != stalled_connect_map.end()) {
                for (const string &dstinst : connit->second) {
                    cpplines.push_back("    _instance_" + dstinst + "->stall();");
                }
            }
            cpplines.push_back("}");
            cpplines.push_back("");
        }
    }

    // init function
    cpplines.push_back("void init_simulation() {");
    // Pipe Init Field
    // Call constructors for pipes
    // _pipe_$name$ = make_unique<Pipe<$type$, $buf$, $in$, $out$>>();
    // _pipe_$name$ = make_unique<SimpleHandshakePipe<$type$>();
    for (const auto &pipepair : design.pipes) {
        const string &pipename = pipepair.first;
        const VulPipe &vp = pipepair.second;
        PipeType type = detectPipeType(vp);
        if (type == PipeType::simple_handshake) {
            cpplines.push_back("    _pipe_" + pipename + " = make_unique<SimpleHandshakePipe<" + vp.type + ">>();");
        } else {
            // default pipe
            cpplines.push_back("    _pipe_" + pipename + " = make_unique<Pipe<" +
                vp.type + ", "
                + std::to_string(vp.buffersize) + ", "
                + std::to_string(vp.inputsize) + ", "
                + std::to_string(vp.outputsize)
                + ">>();");
        }
    }
    // Instance Init Field
    // Call constructor for instance
    // {
    //     $combine$::ConstructorArguments args;
    //     // for pipein: args._pipein_$name$ = &(_pipe_$copipename$->outputs[$copipeport]);
    //     // for pipeout: args._pipeout_$name$ = &(_pipe_$copipename$->inputs[$copipeport]);
    //     // for request: args._request_$name$ = _request_$instname$_$name$;
    //     // for stallable: args._external_stall = _stall_$instname$;
    //     // for config: args.$name$ = $GEN_CONFIG_STATEMENT$;
    //     _instance_$name$ = make_unique<$combine$>(args);
    // }
    for (const auto &vipair : design.instances) {
        const string &instname = vipair.first;
        const VulInstance &vi = vipair.second;
        unique_ptr<VulCombine> combineptr = design.getOrFakeCombineReadonly(vi.combine);
        if (!combineptr) {
            err->push_back(string("Error: combine not found for instance '") + instname + "': " + vi.combine);
            return err;
        }
        const VulCombine &vc = *combineptr;
        cpplines.push_back("    {");
        cpplines.push_back("        " + vc.name + "::ConstructorArguments args;");
        for (const VulPipePort &pp : vc.pipein) {
            auto copit = std::find_if(design.pipe_mod_connections.begin(), design.pipe_mod_connections.end(),
                [&](const VulPipeModuleConnection &c) { return c.instance == instname && c.pipeinport == pp.name; });
            if (copit == design.pipe_mod_connections.end()) {
                err->push_back(string("Error: pipein connection not found for instance '") + instname + "' pipein '" + pp.name + "'");
                return err;
            }
            cpplines.push_back("        args._pipein_" + pp.name + " = &(_pipe_" + copit->pipe + "->outputs[" + std::to_string(copit->portindex) + "]);");
        }
        for (const VulPipePort &pp : vc.pipeout) {
            auto copit = std::find_if(design.mod_pipe_connections.begin(), design.mod_pipe_connections.end(),
                [&](const VulModulePipeConnection &c) { return c.instance == instname && c.pipeoutport == pp.name; });
            if (copit == design.mod_pipe_connections.end()) {
                err->push_back(string("Error: pipeout connection not found for instance '") + instname + "' pipeout '" + pp.name + "'");
                return err;
            }
            cpplines.push_back("        args._pipeout_" + pp.name + " = &(_pipe_" + copit->pipe + "->inputs[" + std::to_string(copit->portindex) + "]);");
        }
        for (const VulRequest &vr : vc.request) {
            cpplines.push_back("        args._request_" + vr.name + " = _request_" + instname + "_" + vr.name + ";");
        }
        if (vc.stallable) {
            cpplines.push_back("        args._external_stall = _stall_" + instname + ";");
        }
        for (const VulConfig &vcfg : vc.config) {
            string configstmt;
            string localerr = _codegenConfigValueStatement(design, vcfg.value, configstmt);
            if(!localerr.empty()) {
                err->push_back(string("Error: failed to generate config value statement for instance '") + instname + "' config '" + vcfg.name + "': " + localerr);
                return err;
            }
            cpplines.push_back("        args." + vcfg.name + " = " + configstmt + ";");
        }
        for (auto &override : vi.config_override) {
            string configstmt;
            string localerr = _codegenConfigValueStatement(design, override.second, configstmt);
            if(!localerr.empty()) {
                err->push_back(string("Error: failed to generate config override value statement for instance '") + instname + "' config '" + override.first + "': " + localerr);
                return err;
            }
            cpplines.push_back("        args." + override.first + " = " + configstmt + ";");
        }
        cpplines.push_back("        _instance_" + instname + " = make_unique<" + vc.name + ">(args);");
        cpplines.push_back("    }");
    }

    // end of init function
    cpplines.push_back("}");
    cpplines.push_back("");

    vector<std::pair<string, string>> update_sequence; // (former_instance, later_instance)
    for (const auto &seq : design.update_constraints) {
        update_sequence.push_back(make_pair(seq.former_instance, seq.latter_instance));
    }
    for (const auto &stl : design.stalled_connections) {
        update_sequence.push_back(make_pair(stl.src_instance, stl.dest_instance));
    }
    vector<string> sorted_instances;
    // Topological sort (Kahn's algorithm)
    {
        // collect all instance names as nodes
        std::unordered_set<string> nodes;
        for (const auto &vipair : design.instances) {
            nodes.insert(vipair.first);
        }

        // adjacency list and indegree map
        std::unordered_map<string, vector<string>> adj;
        std::unordered_map<string, int> indeg;
        for (const string &n : nodes) indeg[n] = 0;

        for (const auto &p : update_sequence) {
            const string &u = p.first;
            const string &v = p.second;
            // ignore edges referring to unknown instances
            if (nodes.find(u) == nodes.end() || nodes.find(v) == nodes.end()) continue;
            adj[u].push_back(v);
            indeg[v]++;
        }

        // start with all zero-indegree nodes (deterministically sorted)
        std::deque<string> q;
        vector<string> zeros;
        for (const string &n : nodes) if (indeg[n] == 0) zeros.push_back(n);
        sort(zeros.begin(), zeros.end());
        for (const string &s : zeros) q.push_back(s);

        while (!q.empty()) {
            string u = q.front(); q.pop_front();
            sorted_instances.push_back(u);
            auto it = adj.find(u);
            if (it == adj.end()) continue;
            for (const string &v : it->second) {
                indeg[v]--;
                if (indeg[v] == 0) q.push_back(v);
            }
        }

        // if not all nodes are sorted, there is a cycle
        if (sorted_instances.size() != nodes.size()) {
            std::unordered_set<string> remaining(nodes.begin(), nodes.end());
            for (const string &s : sorted_instances) remaining.erase(s);
            string cyc = string("Error: update sequence has cycle involving:");
            for (const string &n : remaining) cyc += string(" ") + n;
            err->push_back(cyc);
            return err;
        }
    }
    
    // void _sim_tick() function
    cpplines.push_back("void _sim_tick() {");
    // Tick Field
    // Update each instance according to update sequence
    // _instance_$name$->all_current_tick();
    for (const string &instname : sorted_instances) {
        cpplines.push_back("    _instance_" + instname + "->all_current_tick();");
    }
    cpplines.push_back("}");
    cpplines.push_back("");

    // void _sim_applytick() function
    cpplines.push_back("void _sim_applytick() {");
    // ApplyTick Field
    // Update each instance according to update sequence
    // _instance_$name$->all_current_applytick();
    for (const string &instname : sorted_instances) {
        cpplines.push_back("    _instance_" + instname + "->all_current_applytick();");
    }
    // ApplyTick Pipe Field
    // Update each pipe
    // _pipe_$name$->apply_tick();
    for (const auto &pipepair : design.pipes) {
        const string &pipename = pipepair.first;
        cpplines.push_back("    _pipe_" + pipename + "->apply_tick();");
    }
    cpplines.push_back("}");
    cpplines.push_back("");

    // void run_simulation_tick() {
    //     _sim_tick();
    //     _sim_applytick();
    // }
    cpplines.push_back("void run_simulation_tick() {");
    cpplines.push_back("    _sim_tick();");
    cpplines.push_back("    _sim_applytick();");
    cpplines.push_back("}");
    cpplines.push_back("");

    // void finalize_simulation()
    cpplines.push_back("void finalize_simulation() {");
    // Finalize Field
    // Reset all instance pointer
    // _instance_$name$.reset();
    for (const auto &instancepair : design.instances) {
        const string &instname = instancepair.first;
        cpplines.push_back("    _instance_" + instname + ".reset();");
    }
    // Finalize Pipe Field
    // Reset all pipe pointer
    // _pipe_$name$.reset();
    for (const auto &pipepair : design.pipes) {
        const string &pipename = pipepair.first;
        cpplines.push_back("    _pipe_" + pipename + ".reset();");
    }
    cpplines.push_back("}");
    cpplines.push_back("");

    return nullptr;
}

/**
 * @brief Generate C++ code for simulating an entire design.
 * This includes generating the header files and C++ source files for all combines in the design.
 * @param design The VulDesign object containing the entire design.
 * @param libdir The directory where the vulsim core lib is located.
 * @param outdir The directory where the generated output files will be placed.
 */
unique_ptr<vector<string>> codegenProject(VulDesign &design, const string &libdir, const string &outdir) {
    unique_ptr<vector<string>> err = std::make_unique<vector<string>>();

    // outdir should be empty or not exist
    if (fs::exists(outdir)) {
        if (!fs::is_empty(outdir)) {
            err->push_back(string("Error: output directory is not empty: ") + outdir);
            return err;
        }
    } else {
        // create outdir
        std::error_code ec;
        if (!fs::create_directories(outdir, ec)) {
            err->push_back(string("Error: failed to create output directory: ") + outdir + " : " + ec.message());
            return err;
        }
    }

    fs::path outpath(outdir);
    fs::path libpath(libdir);

    // generate bundle.h
    {
        vector<string> bundlelines;
        unique_ptr<vector<string>> localerr = codegenBundlesHeader(design, bundlelines);
        if (localerr && !localerr->empty()) {
            err->push_back(string("Error: failed to generate bundle.h"));
            for (const string &e : *localerr) {
                err->push_back("  " + e);
            }
            return err;
        }
        // write to outdir/bundle.h
        fs::path bundlefile = outpath / "bundle.h";
        std::ofstream ofs(bundlefile);
        if (!ofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + bundlefile.string());
            return err;
        }
        for (const string &line : bundlelines) {
            ofs << line << std::endl;
        }
        ofs.close();
    }

    // generate combines
    for (const auto &combinepair : design.combines) {
        const string &combinename = combinepair.first;
        vector<string> headerlines;
        vector<string> cpplines;
        unique_ptr<vector<string>> localerr = codegenCombine(design, combinename, headerlines, cpplines);
        if (localerr && !localerr->empty()) {
            err->push_back(string("Error: failed to generate code for combine: ") + combinename);
            for (const string &e : *localerr) {
                err->push_back("  " + e);
            }
            return err;
        }
        // write to outdir/{combinename}.h and outdir/{combinename}.cpp
        fs::path headerfile = outpath / (combinename + ".h");
        fs::path cppfile = outpath / (combinename + ".cpp");
        std::ofstream hofs(headerfile);
        if (!hofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + headerfile.string());
            return err;
        }
        for (const string &line : headerlines) {
            hofs << line << std::endl;
        }
        hofs.close();
        std::ofstream cofs(cppfile);
        if (!cofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + cppfile.string());
            return err;
        }
        for (const string &line : cpplines) {
            cofs << line << std::endl;
        }
        cofs.close();
    }
    
    // generate simulation.cpp
    {
        vector<string> simlines;
        unique_ptr<vector<string>> localerr = codegenSimulation(design, simlines);
        if (localerr && !localerr->empty()) {
            err->push_back(string("Error: failed to generate simulation.cpp"));
            for (const string &e : *localerr) {
                err->push_back("  " + e);
            }
            return err;
        }
        fs::path simfile = outpath / "simulation.cpp";
        std::ofstream sofs(simfile);
        if (!sofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + simfile.string());
            return err;
        }
        for (const string &line : simlines) {
            sofs << line << std::endl;
        }
        sofs.close();
    }

    // generate config.ini
    {
        vector<string> configstrs;
        for (const auto &configpair : design.config_lib) {
            const VulConfigItem &vc = configpair.second;
            configstrs.push_back(vc.name + " = " + vc.value);
        }
        // sort configstrs lexicographically
        sort(configstrs.begin(), configstrs.end());

        fs::path configfile = outpath / "config.ini";
        std::ofstream cofs(configfile);
        if (!cofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + configfile.string());
            return err;
        }
        for (const string &line : configstrs) {
            cofs << line << std::endl;
        }
        cofs.close();
    }

    // copy everything in libdir to outdir (regular files only, error on duplicate)
    {
        std::error_code ec;
        if (!fs::exists(libpath)) {
            err->push_back(string("Error: libdir does not exist: ") + libpath.string());
            return err;
        }

        for (auto it = fs::recursive_directory_iterator(libpath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                err->push_back(string("Error: failed while iterating libdir: ") + ec.message());
                return err;
            }

            const fs::path src = it->path();
            if (!fs::is_regular_file(src, ec)) {
                if (ec) {
                    err->push_back(string("Error: failed to stat path: ") + src.string() + " : " + ec.message());
                    return err;
                }
                // skip non-regular files
                continue;
            }

            fs::path rel = fs::relative(src, libpath, ec);
            if (ec) {
                err->push_back(string("Error: failed to compute relative path for: ") + src.string() + " : " + ec.message());
                return err;
            }
            fs::path dst = outpath / rel;

            // if destination exists, error (no duplicates allowed)
            if (fs::exists(dst)) {
                err->push_back(string("Error: destination file already exists (duplicate): ") + dst.string());
                return err;
            }

            // ensure parent directory exists
            fs::path parent = dst.parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                if (!fs::create_directories(parent, ec)) {
                    err->push_back(string("Error: failed to create directory: ") + parent.string() + " : " + ec.message());
                    return err;
                }
            }

            // copy file
            fs::copy_file(src, dst, fs::copy_options::none, ec);
            if (ec) {
                err->push_back(string("Error: failed to copy file from ") + src.string() + " to " + dst.string() + " : " + ec.message());
                return err;
            }
        }
    }

    // copy global.h from projdir/cpp to outdir
    {
        fs::path globalsrc = fs::path(design.project_dir) / "cpp" / "global.h";
        fs::path globaldst = outpath / "global.h";
        std::error_code ec;
        fs::copy_file(globalsrc, globaldst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err->push_back(string("Error: failed to copy global.h from ") + globalsrc.string() + " to " + globaldst.string() + " : " + ec.message());
            return err;
        }
    }

    // copy all prefab_dir/{prefabname}.h and prefab_dir/source/*.cpp to outdir
    {
        for (const auto &prefabpair : design.prefabs) {
            const string &prefabname = prefabpair.first;
            const VulPrefab &vp = prefabpair.second;
            // copy prefab header
            fs::path prefabsrc = fs::path(vp.path) / (prefabname + ".h");
            fs::path prefabdst = outpath / (prefabname + ".h");
            std::error_code ec;
            fs::copy_file(prefabsrc, prefabdst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                err->push_back(string("Error: failed to copy prefab file from ") + prefabsrc.string() + " to " + prefabdst.string() + " : " + ec.message());
                return err;
            }
            // copy prefab_dir/source/*.cpp unrecursively
            fs::path sourcesrcdir = fs::path(vp.path) / "source";
            fs::path sourcedstd = outpath;
            for (auto it = fs::directory_iterator(sourcesrcdir, fs::directory_options::skip_permission_denied, ec);
                 it != fs::directory_iterator(); it.increment(ec)) {
                if (ec) {
                    err->push_back(string("Error: failed while iterating prefab source dir: ") + ec.message());
                    return err;
                }

                const fs::path src = it->path();
                if (!fs::is_regular_file(src, ec)) {
                    if (ec) {
                        err->push_back(string("Error: failed to stat path: ") + src.string() + " : " + ec.message());
                        return err;
                    }
                    // skip non-regular files
                    continue;
                }
                if (src.extension() != ".cpp") {
                    // skip non-cpp files
                    continue;
                }

                fs::path dst = sourcedstd / src.filename();

                // copy file
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    err->push_back(string("Error: failed to copy prefab source file from ") + src.string() + " to " + dst.string() + " : " + ec.message());
                    return err;
                }
            }
        }
    }

    // done
    return nullptr;
}




