# ------------ encoding:utf-8 -----------------

import os
import sys
import argparse
import shutil

from dataclasses import dataclass
from collections import deque

import xml.etree.ElementTree as ET

from clang import cindex

parser = argparse.ArgumentParser(description="VulSim C++ Code Generator")
parser.add_argument("projdir", help="VulSim project directory")
parser.add_argument("output", help="Output C++ code diretory")

args = parser.parse_args()

codetab = '    '

# 检查输入目录是否有效
print("检查输入目录是否有效")

proj_dir = args.projdir
bundle_dir = os.path.join(proj_dir, "bundle")
combine_dir = os.path.join(proj_dir, "combine")
cpp_dir = os.path.join(proj_dir, "cpp")

if (not os.path.isdir(proj_dir)) or \
        (not os.path.isdir(bundle_dir)) or \
        (not os.path.isdir(combine_dir)) or \
        (not os.path.isdir(cpp_dir)):
    print("Project Directory Error")
    sys.exit(1)

output_dir = args.output
os.makedirs(output_dir, exist_ok=True)
if len(os.listdir(output_dir)):
    print("Output Directory %s is NOT EMPTY, Please Clear It" % output_dir)
    exit(1)

cppout_dir = output_dir
os.makedirs(cppout_dir, exist_ok=True)

# 初始化全局数据
print("初始化全局数据")

warnings : list[str] = []
using_sectrl = False


# 加载所有的bundle定义
print("加载所有的bundle定义")

basic_data_type = {"bool", "int8", "int16", "int32", "int64", "int128", "uint8", "uint16", "uint32", "uint64", "uint128"}

@dataclass
class BundleMember:
    name : str 
    type : str
    value : str

bundles : dict[str, list[BundleMember]] = {}
bundle_depend_former : list[tuple[str, str]] = []

bundle_xml_files = [f for f in os.listdir(bundle_dir) if f.endswith(".xml")]
for xml_file in bundle_xml_files:
    tree = ET.parse(os.path.join(bundle_dir, xml_file))
    root = tree.getroot()

    if root.tag != "bundle":
        print("Wrong Format: bundle/%s does not start with tag \"bundle\"" % xml_file)
        sys.exit(1)

    name = root.find("name").text.strip()
    members : list[BundleMember] = []

    print("Loading Bundle %s From bundle/%s" % (name, xml_file))

    for member in root.findall("member"):
        m_name = member.find("name").text.strip()
        m_type = member.find("type").text.strip()
        m_value = "0"
        if m_type in basic_data_type and member.find("value"):
            m_value = member.find("value").text.strip()
        members.append(BundleMember(m_name, m_type, m_value))
        if m_type not in basic_data_type:
            bundle_depend_former.append((m_type, name))

    bundles[name] = members

# 检查bundle依赖关系
print("检查bundle依赖关系")

bundle_graph = {e : list[str]() for e in bundles.keys()}
indegree = {e : 0 for e in bundles.keys()}

for a, b in bundle_depend_former:
    if a not in bundles.keys():
        print("WARNING: Unknown Type String %s in Bundle %s" % (a, b))
        continue;
    bundle_graph[a].append(b)
    indegree[b] += 1

bundle_queue = deque([e for e in bundles.keys() if indegree[e] == 0])
bundle_sequence : list[str] = []

while bundle_queue:
    node = bundle_queue.popleft()
    bundle_sequence.append(node)
    for nei in bundle_graph[node]:
        indegree[nei] -= 1
        if indegree[nei] == 0:
            bundle_queue.append(nei)

if len(bundle_sequence) != len(bundles.keys()):
    print("Bundle Check Failed: Loop Dependency in " + ", ".join([e for e in bundles.keys() if indegree[e] != 0]))
    sys.exit(1)


# 生成bundle.h
print("生成bundle.h")

bundle_h_lines : list[str] = []
bundle_h_lines.append("#pragma once")
bundle_h_lines.append("")
bundle_h_lines.append("#include \"common.h\"")
bundle_h_lines.append("#include \"global.h\"")
bundle_h_lines.append("")
for bundle in bundle_sequence:
    members = bundles[bundle]
    bundle_h_lines.append("typedef struct {")
    for mem in members:
        if mem.type in basic_data_type:
            bundle_h_lines.append("    %s %s = %s;" % (mem.type, mem.name, mem.value))
        else:
            bundle_h_lines.append("    %s %s;" % (mem.type, mem.name))
    bundle_h_lines.append("} %s;" % bundle)
    bundle_h_lines.append("")

bundle_h_path = os.path.join(cppout_dir, "bundle.h")
with open(bundle_h_path, "w") as ofile:
    for line in bundle_h_lines:
        ofile.write(line + "\n")

# 复制头文件
print("复制头文件")

shutil.copy(os.path.join(cpp_dir, "global.h"), cppout_dir)
print("copy global.h")
shutil.copy("common.h", cppout_dir)
print("copy common.h")
shutil.copy("vulsimlib.h", cppout_dir)
print("copy vulsimlib.h")
libout_dir = os.path.join(cppout_dir, "lib")
os.makedirs(libout_dir, exist_ok=True)
for filename in [f for f in os.listdir("lib") if f.endswith(".h") or f.endswith(".hpp")]:
    filepath = os.path.join("lib", filename)
    shutil.copy(filepath, libout_dir)
    print("copy %s" % filepath)

# 生成组合逻辑块代码
print("生成组合逻辑块代码")

@dataclass
class Argument:
    type : str
    name : str 

@dataclass
class Function:
    name : str
    arg : list[Argument]
    ret : list[Argument]

@dataclass
class Combine:
    name : str 
    request : dict[str, Function]
    service : dict[str, Function]
    function : dict[str, Function]
    pipein : dict[str, str]
    pipeout : dict[str, str]
    stallable : bool

combines : dict[str, Combine] = {}

def extract_cpp_function_body(filename:str, func_name:str) -> str:
    index = cindex.Index.create()
    tu = index.parse(filename)

    for node in tu.cursor.get_children():
        if node.kind == cindex.CursorKind.FUNCTION_DECL and node.spelling == func_name:
            if node.extent.start.file is None:
                continue
            
            start = node.extent.start
            end = node.extent.end

            with open(filename, "r", encoding="utf-8") as f:
                lines = f.readlines()

            func_code = "".join(lines[start.line - 1:end.line])
            brace_start = func_code.find("{")
            brace_end = func_code.rfind("}")
            if brace_start != -1 and brace_end != -1:
                return func_code[brace_start + 1:brace_end].strip()
    return ""

for xml_file in [f for f in os.listdir(combine_dir) if f.endswith(".xml")]:
    tree = ET.parse(os.path.join(combine_dir, xml_file))
    root = tree.getroot()

    if root.tag != "combine":
        print("Wrong Format: combine/%s does not start with tag \"combine\"" % xml_file)
        sys.exit(1)

    cname = root.find("name").text.strip()
    print("Loading Combine %s From combine/%s" % (cname, xml_file))

    info = Combine(cname, {}, {}, {}, {}, {}, False)

    constructor_arguments_field : list[str] = []
    constructor_field : list[str] = []
    deconstructor_field : list[str] = []
    member_field : list[str] = []
    function_field : list[str] = []
    tick_field : list[str] = []
    applytick_field : list[str] = []
    user_tick_field : list[str] = []
    user_applytick_field : list[str] = []

    for pipein in root.findall("pipein"):
        name = pipein.find("name").text.strip()
        type = pipein.find("type").text.strip()
        argtype = type + " &" if type not in basic_data_type else type
        comment = pipein.find("comment").text.strip() if pipein.find("comment") is not None else ""
        member_field.append("PipeInputPort<%s> * _pipein_%s;" % (type, name))
        member_field.append("/* %s */" % comment)
        member_field.append("bool %s_can_pop() { return _pipein_%s->can_pop(); };" % (name, name))
        member_field.append("/* %s */" % comment)
        member_field.append("%s %s_top() { return _pipein_%s->top(); };" % (argtype, name, name))
        member_field.append("/* %s */" % comment)
        member_field.append("void %s_pop() { _pipein_%s->pop(); };" % (name, name))
        constructor_arguments_field.append("PipeInputPort<%s> * pipein_%s;" % (type, name))
        constructor_field.append("this->_pipein_%s = arg.pipein_%s;" % (name, name))
        print("parsing pipein %s" % name)
        info.pipein[name] = type

    for pipeout in root.findall("pipeout"):
        name = pipeout.find("name").text.strip()
        type = pipeout.find("type").text.strip()
        argtype = type + " &" if type not in basic_data_type else type
        comment = pipeout.find("comment").text.strip() if pipeout.find("comment") is not None else ""
        member_field.append("PipeOutputPort<%s> * _pipeout_%s;" % (type, name))
        member_field.append("/* %s */" % comment)
        member_field.append("bool %s_can_push() { return _pipeout_%s->can_push(); };" % (name, name))
        member_field.append("/* %s */" % comment)
        member_field.append("void %s_push(%s value) { _pipeout_%s->push(value); };" % (name, argtype, name))
        constructor_arguments_field.append("PipeOutputPort<%s> * pipeout_%s;" % (type, name))
        constructor_field.append("this->_pipeout_%s = arg.pipeout_%s;" % (name, name))
        print("parsing pipeout %s" % name)
        info.pipeout[name] = type

    for request in root.findall("request"):
        name = request.find("name").text.strip()
        arg_statements : list[str] = []
        arg_comments : list[str] = []
        arg_names : list[str] = [] 
        func = Function(name, [], [])
        for arg in request.findall("arg"):
            arg_type = arg.find("type").text.strip()
            arg_name = arg.find("name").text.strip()
            func.arg.append(Argument(arg_type, arg_name))
            if arg_type not in basic_data_type:
                arg_type += " &"
            arg_statements.append("%s %s" % (arg_type, arg_name))
            arg_names.append(arg_name)
            arg_comments.append((arg_name, arg.find("comment").text.strip() if arg.find("comment") is not None else ""))
        for ret in request.findall("return"):
            ret_type = ret.find("type").text.strip()
            ret_name = ret.find("name").text.strip()
            func.ret.append(Argument(arg_type, arg_name))
            arg_statements.append("%s * %s" % (ret_type, ret_name))
            arg_names.append(ret_name)
            arg_comments.append((ret_name, ret.find("comment").text.strip() if ret.find("comment") is not None else ""))
        member_field.append("void (*_request_%s)(%s);" % (name, ", ".join(arg_statements)))
        member_field.append("/*")
        member_field.append(" * %s" % (request.find("comment").text.strip() if request.find("comment") is not None else "Request %s" % name))
        for arg, comment in zip(arg_names, arg_comments):
            member_field.append(" * @param %s: %s" % (arg, comment))
        member_field.append(" */")
        member_field.append("void %s(%s) { _request_%s(%s); };" % (name, ", ".join(arg_statements), name, ", ".join(arg_names)))
        constructor_arguments_field.append("void (*request_%s)(%s);" % (name, ", ".join(arg_statements)))
        constructor_field.append("this->_request_%s = arg.request_%s;" % (name, name))
        print("parsing request %s" % name)
        info.request[name] = func
        

    for service in root.findall("service") + root.findall("function"):
        name = service.find("name").text.strip()
        arg_statements : list[str] = []
        arg_comments : list[str] = []
        arg_names : list[str] = [] 
        func = Function(name, [], [])
        for arg in service.findall("arg"):
            arg_type = arg.find("type").text.strip()
            arg_name = arg.find("name").text.strip()
            func.arg.append(Argument(arg_type, arg_name))
            if arg_type not in basic_data_type:
                arg_type += " &"
            arg_statements.append("%s %s" % (arg_type, arg_name))
            arg_names.append(arg_name)
            arg_comments.append((arg_name, arg.find("comment").text.strip() if arg.find("comment") is not None else ""))
        for ret in service.findall("return"):
            ret_type = ret.find("type").text.strip()
            ret_name = ret.find("name").text.strip()
            func.ret.append(Argument(arg_type, arg_name))
            arg_statements.append("%s * %s" % (ret_type, ret_name))
            arg_names.append(ret_name)
            arg_comments.append((ret_name, ret.find("comment").text.strip() if ret.find("comment") is not None else ""))
        member_field.append("/*")
        member_field.append(" * %s" % (service.find("comment").text.strip() if service.find("comment") is not None else "Service %s" % name))
        for arg, comment in zip(arg_names, arg_comments):
            member_field.append(" * @param %s: %s" % (arg, comment))
        member_field.append(" */")
        member_field.append("void %s(%s);" % (name, ", ".join(arg_statements)))
        cppfunc = service.find("cppfunc")
        cppfunclines : list[str] = []
        if cppfunc.find("file") is not None:
            cppfuncfile = cppfunc.find("file").text.strip()
            cppfuncname = cppfunc.find("name").text.strip()
            cppfuncline = extract_cpp_function_body(os.path.join(cpp_dir, cppfuncfile), cppfuncname)
            cppfunclines = cppfuncline.split("\n")
        for code in cppfunc.findall("code"):
            cppfunclines.append(code.text.strip())
        function_field.append("void %s::%s(%s) {" % (cname, name, ", ".join(arg_statements)))
        for line in cppfunclines:
            function_field.append(line)
        function_field.append("}")
        function_field.append("")
        if service.tag == "service":
            info.service[name] = func
            print("parsing service %s" % name)
        else:
            info.function[name] = func
            print("parsing function %s" % name)

    for storage in root.findall("storage"):
        name = storage.find("name").text.strip()
        type = storage.find("type").text.strip()
        comment = storage.find("comment").text.strip() if storage.find("comment") is not None else ""
        member_field.append("/* %s */" % comment)
        if storage.find("value"):
            value = storage.find("value").text.strip()
            member_field.append("%s %s = %s;" % (type, name, value))
        elif type in basic_data_type:
            member_field.append("%s %s = 0;" % (type, name))
        else:
            member_field.append("%s %s;" % (type, name))
        print("parsing storage %s" % name)

    for storagenext in root.findall("storagenext"):
        name = storagenext.find("name").text.strip()
        type = storagenext.find("type").text.strip()
        argtype = type + " &" if type not in basic_data_type else type
        comment = storagenext.find("comment").text.strip() if storagenext.find("comment") is not None else ""
        if storage.find("value"):
            value = storage.find("value").text.strip()
            member_field.append("StorageNext<%s> _storagenext_%s = StorageNext<%s>(%s);" % (type, name, type, value))
        else:
            member_field.append("StorageNext<%s> _storagenext_%s;" % (type, name))
        member_field.append("/* %s */" % comment)
        member_field.append("%s %s_get() { return _storagenext_%s.get(); };" % (argtype, name, name))
        member_field.append("/* %s */" % comment)
        member_field.append("void %s_setnext(%s value, uint8 priority) { _storagenext_%s.setnext(value, priority); } ;" % (name, argtype, name))
        applytick_field.append("_storagenext_%s.apply_tick();" % name)
        print("parsing storagenext %s" % name)

    for storagetick in root.findall("storagetick"):
        name = storagetick.find("name").text.strip()
        type = storagetick.find("type").text.strip()
        value = storagetick.find("value").text.strip() if storagetick.find("value") is not None else "0"
        assert(type in basic_data_type)
        comment = storagetick.find("comment").text.strip() if storagetick.find("comment") is not None else ""
        member_field.append("%s %s = %s;" % (type, name, value))
        applytick_field.append("%s = %s;" % (name, value))
        print("parsing storagetick %s" % name)

    for config in root.findall("config"):
        name = config.find("name").text.strip()
        value = config.find("value").text.strip()
        comment = config.find("comment").text.strip() if config.find("comment") is not None else ""
        member_field.append("const int64 %s = %s;" % (name, value))
        print("parsing config %s" % name)

    for init in root.findall("init"):
        cppfunc = init.find("cppfunc")
        cppfunclines : list[str] = []
        if cppfunc.find("file") is not None:
            cppfuncfile = cppfunc.find("file").text.strip()
            cppfuncname = cppfunc.find("name").text.strip()
            cppfuncline = extract_cpp_function_body(os.path.join(cpp_dir, cppfuncfile), cppfuncname)
            cppfunclines = cppfuncline.split("\n")
        for code in cppfunc.findall("code"):
            cppfunclines.append(code.text.strip())
        for line in cppfunclines:
            constructor_field.append(line)
        print("parsing init")


    ticks = root.findall("tick")
    if len(ticks) > 1:
        warnings.append("Combine %s has multiple tick(), only the first one is used" % cname)
    if len(ticks):
        tick = ticks[0]
        cppfunc = tick.find("cppfunc")
        cppfunclines : list[str] = []
        if cppfunc.find("file") is not None:
            cppfuncfile = cppfunc.find("file").text.strip()
            cppfuncname = cppfunc.find("name").text.strip()
            cppfuncline = extract_cpp_function_body(os.path.join(cpp_dir, cppfuncfile), cppfuncname)
            cppfunclines = cppfuncline.split("\n")
        for code in cppfunc.findall("code"):
            cppfunclines.append(code.text.strip())
        for line in cppfunclines:
            user_tick_field.append(line)
        print("parsing tick")

    applyticks = root.findall("applytick")
    if len(applyticks) > 1:
        warnings.append("Combine %s has multiple applytick(), only the first one is used" % cname)
    if len(applyticks):
        applytick = applyticks[0]
        cppfunc = applytick.find("cppfunc")
        cppfunclines : list[str] = []
        if cppfunc.find("file") is not None:
            cppfuncfile = cppfunc.find("file").text.strip()
            cppfuncname = cppfunc.find("name").text.strip()
            cppfuncline = extract_cpp_function_body(os.path.join(cpp_dir, cppfuncfile), cppfuncname)
            cppfunclines = cppfuncline.split("\n")
        for code in cppfunc.findall("code"):
            cppfunclines.append(code.text.strip())
        for line in cppfunclines:
            user_applytick_field.append(line)
        print("parsing applytick")

    if root.find("stallable") is not None:
        member_field.append("bool stalled = false;")
        member_field.append("void (*_external_stall)();")
        member_field.append("void stall() { stalled = true; _external_stall(); }")
        constructor_arguments_field.append("void (*external_stall)();")
        constructor_field.append("this->_external_stall = arg.external_stall;")
        applytick_field.append("stalled = false;")
        print("parsing stallable")
        info.stallable = True
    
    combines[cname] = info

    header_lines : list[str] = []
    header_lines.append("#pragma once")
    header_lines.append("")
    header_lines.append("#include \"common.h\"")
    header_lines.append("#include \"global.h\"")
    header_lines.append("#include \"bundle.h\"")
    header_lines.append("#include \"vulsimlib.h\"")
    header_lines.append("")
    header_lines.append("/* %s */" % (root.find("comment").text.strip() if root.find("comment") is not None else "Combine %s" % cname))
    header_lines.append("class %s {" % cname)
    header_lines.append("public:")
    header_lines.append(codetab + "class ConstructorArguments {")
    header_lines.append(codetab + "public:")
    for line in constructor_arguments_field:
        header_lines.append(codetab*2 + line)
    header_lines.append(codetab*2 + "int32 __dummy = 0;")
    header_lines.append(codetab + "};")
    header_lines.append("")

    header_lines.append(codetab + "%s(ConstructorArguments & arg) {" % cname)
    for line in constructor_field:
        header_lines.append(codetab*2 + line)
    header_lines.append(codetab + "}")
    header_lines.append("")

    header_lines.append(codetab + "~%s() {" % cname)
    for line in deconstructor_field:
        header_lines.append(codetab*2 + line)
    header_lines.append(codetab + "}")
    header_lines.append("")

    header_lines.append(codetab + "void all_current_tick();")
    header_lines.append(codetab + "void all_current_applytick();")
    header_lines.append(codetab + "void user_current_tick();")
    header_lines.append(codetab + "void user_current_applytick();")
    header_lines.append("")

    for line in member_field:
        header_lines.append(codetab + line)
    header_lines.append("};")

    cpp_lines : list[str] = []
    cpp_lines.append("#include \"%s.h\"" % cname)
    cpp_lines.append("")
    for line in function_field:
        cpp_lines.append(line)
    cpp_lines.append("void %s::all_current_tick() {" % cname)
    for line in tick_field:
        cpp_lines.append(codetab + line)
    cpp_lines.append(codetab + "user_current_tick();")
    cpp_lines.append("}")
    cpp_lines.append("void %s::all_current_applytick() {" % cname)
    for line in applytick_field:
        cpp_lines.append(codetab + line)
    cpp_lines.append(codetab + "user_current_applytick();")
    cpp_lines.append("}")
    cpp_lines.append("void %s::user_current_tick() {" % cname)
    for line in user_tick_field:
        cpp_lines.append(codetab + line)
    cpp_lines.append("}")
    cpp_lines.append("void %s::user_current_applytick() {" % cname)
    for line in user_applytick_field:
        cpp_lines.append(codetab + line)
    cpp_lines.append("}")

    header_file = os.path.join(cppout_dir, "%s.h" % cname)
    with open(header_file, "w") as ofile:
        for line in header_lines:
            ofile.write(line + "\n")
    cpp_file = os.path.join(cppout_dir, "%s.cpp" % cname)
    with open(cpp_file, "w") as ofile:
        for line in cpp_lines:
            ofile.write(line + "\n")




# 清理
print("清理")

for w,idx in zip(warnings, range(len(warnings))):
    print("WARN %d: %s" % (idx, w))

print("Finished")
print("Generated code in %s" % output_dir)
