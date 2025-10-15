
#include "codegen.h"
#include "codehelper.h"

#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

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
        // PipeInputPort<$type$> * _pipein_$name$;
        // /* $comment$ */
        // bool $name$_can_pop() { return _pipein_$name$->can_pop(); };
        // /* $comment$ */
        // $type$ (&) $name$_top() { return _pipein_$name$->top(); };
        // /* $comment$ */
        // void $name$_pop() { _pipein_$name$->pop(); };

        // Constructor Arguments Field +:
        // PipeInputPort<$type$> * _pipein_$name$;

        // Constructor Field +:
        // this->_pipein_$name$ = arg._pipein_$name$;

        string cmtstr = (pp.comment.empty() ? (pp.name) : (pp.comment));
        string typestr = (isBasicVulType(pp.type) ? pp.type : (pp.type + " &"));
        member_field.push_back("PipeInputPort<" + pp.type + "> * _pipein_" + pp.name + ";");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("bool " + pp.name + "_can_pop() { return _pipein_" + pp.name + "->can_pop(); };");
        member_field.push_back("// " + cmtstr);
        member_field.push_back(typestr + " " + pp.name + "_top() { return _pipein_" + pp.name + "->top(); };");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("void " + pp.name + "_pop() { _pipein_" + pp.name + "->pop(); };");
        member_field.push_back("");
        constructor_args_field.push_back("PipeInputPort<" + pp.type + "> * _pipein_" + pp.name + ";");
        constructor_field.push_back("this->_pipein_" + pp.name + " = arg._pipein_" + pp.name + ";");
    }

    // generate pipeout
    for (const VulPipePort &pp : vc.pipeout) {
        // Member Field +:
        // PipeOutputPort<$type$> * _pipeout_$name$;
        // /* $comment$ */
        // bool $name$_can_push() { return _pipeout_$name$->can_push(); } ;
        // /* $comment$ */
        // void $name$_push($type$ (&) value) { _pipeout_$name$->push(value) } ;

        // Constructor Arguments Field +:
        // PipeOutputPort<$type$> * _pipeout_$name$;

        // Constructor Field +:
        // this->_pipeout_$name$ = arg._pipeout_$name$;

        string cmtstr = (pp.comment.empty() ? (pp.name) : (pp.comment));
        string typestr = (isBasicVulType(pp.type) ? pp.type : (pp.type + " &"));
        member_field.push_back("PipeOutputPort<" + pp.type + "> * _pipeout_" + pp.name + ";");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("bool " + pp.name + "_can_push() { return _pipeout_" + pp.name + "->can_push(); } ;");
        member_field.push_back("// " + cmtstr);
        member_field.push_back("void " + pp.name + "_push(" + typestr + " value) { _pipeout_" + pp.name + "->push(value); } ;");
        member_field.push_back("// " + cmtstr);
        constructor_args_field.push_back("PipeOutputPort<" + pp.type + "> * _pipeout_" + pp.name + ";");
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

    // generate config
    for (const VulConfig &c : vc.config) {
        // Member Field +：
        // /* $comment$ */
        // const int64 $name$ = $ref.value$;
        string cmt = (c.comment.empty() ? (string("// ") + c.name) : (string("// ") + c.comment));
        string valuestr = "0";
        if (!c.ref.empty()) {
            auto it = design.config_lib.find(c.ref);
            if (it != design.config_lib.end()) {
                valuestr = it->second.value;
            } else {
                err->push_back(string("Error: config reference not found for combine '") + combine + "' config '" + c.name + "': " + c.ref);
                return err;
            }
        } else {
            err->push_back(string("Warning: config reference empty for combine '") + combine + "' config '" + c.name + "'");
        }
        member_field.push_back(cmt);
        member_field.push_back("const int64 " + c.name + " = " + valuestr + ";");
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
    _extractCodeIntoField(vc.init, constructor_field);

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
    headerlines.push_back("    " + vc.name + "(ConstructorArguments & arg) {");
    for (const string &line : constructor_field) headerlines.push_back("        " + line);
    headerlines.push_back("    };");
    headerlines.push_back("    ~" + vc.name + "() {};");
    headerlines.push_back("");
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
        unique_ptr<vector<string>> bundlelines = codeGenerateBundleHeaderFile(design.bundles);
        // write to outdir/bundle.h
        fs::path bundlefile = outpath / "bundle.h";
        std::ofstream ofs(bundlefile);
        if (!ofs.is_open()) {
            err->push_back(string("Error: failed to open output file for writing: ") + bundlefile.string());
            return err;
        }
        for (const string &line : *bundlelines) {
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

    // copy vulsimlib.h and common.h from libdir to outdir
    {
        fs::path vulsimlibsrc = libpath / "vulsimlib.h";
        fs::path vulsimlibdst = outpath / "vulsimlib.h";
        std::error_code ec;
        fs::copy_file(vulsimlibsrc, vulsimlibdst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err->push_back(string("Error: failed to copy vulsimlib.h from ") + vulsimlibsrc.string() + " to " + vulsimlibdst.string() + " : " + ec.message());
            return err;
        }
    }
    {
        fs::path commonsrc = libpath / "common.h";
        fs::path commondst = outpath / "common.h";
        std::error_code ec;
        fs::copy_file(commonsrc, commondst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err->push_back(string("Error: failed to copy common.h from ") + commonsrc.string() + " to " + commondst.string() + " : " + ec.message());
            return err;
        }
    }

    // copy pipe.hpp and storage.hpp from libdir to outdir
    {
        std::error_code ec;
        fs::path pipesrc = libpath / "pipe.hpp";
        fs::path pipedst = outpath / "pipe.hpp";
        fs::copy_file(pipesrc, pipedst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err->push_back(string("Error: failed to copy pipe.hpp from ") + pipesrc.string() + " to " + pipedst.string() + " : " + ec.message());
            return err;
        }
        fs::path storagesrc = libpath / "storage.hpp";
        fs::path storagedst = outpath / "storage.hpp";
        fs::copy_file(storagesrc, storagedst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err->push_back(string("Error: failed to copy storage.hpp from ") + storagesrc.string() + " to " + storagedst.string() + " : " + ec.message());
            return err;
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

    // done
    return nullptr;
}




