import subprocess
from dataclasses import dataclass
from typing import List


@dataclass
class Func:
    name: str
    ret: str
    args: List[str]
    always: bool = False
    direction: str = "active"
    body: str = ""


def classify_arg(arg: str):
    arg = arg.strip()

    if "*" in arg:
        typ, name = arg.rsplit("*", 1)
        return "out", typ.strip(), name.strip()

    if "&" in arg:
        typ, name = arg.rsplit("&", 1)
        return "in", typ.strip(), name.strip()

    typ, name = arg.rsplit(" ", 1)
    return "in", typ.strip(), name.strip()


# -------------------------------------------------
# tick ports
# -------------------------------------------------

def gen_tick_ports(funcs: List[Func]) -> str:
    ports = []
    ports.append("bool rst")

    for f in funcs:
        if f.direction == "passive":
            ports.append(f"bool {f.name}_valid")
        elif not f.always:
            ports.append(f"bool * {f.name}_valid")

        for arg in f.args:
            direction, typ, name = classify_arg(arg)

            if f.direction == "passive":
                ports.append(f"{typ} {f.name}_{name}")
            else:
                if direction == "in":
                    ports.append(f"{typ} * {f.name}_{name}")
                else:
                    ports.append(f"{typ} {f.name}_{name}")

        if f.ret != "void" and f.direction == "active":
            ports.append(f"bool {f.name}_ready")

    return ",\n    ".join(ports)


def extract_port_names(port_str: str) -> List[str]:
    ports = []
    for p in port_str.split(","):
        p = p.strip()
        name = p.split()[-1].lstrip("*")
        ports.append(name)
    return ports


# -------------------------------------------------
# active lambda
# -------------------------------------------------

def gen_active_lambda(f: Func) -> str:
    sig = [arg.strip() for arg in f.args]
    ret = f" -> {f.ret}" if f.ret != "void" else ""

    captures = []
    if not f.always:
        captures.append(f"{f.name}_valid")

    for arg in f.args:
        _, _, name = classify_arg(arg)
        captures.append(f"{f.name}_{name}")

    if f.ret != "void":
        captures.append(f"{f.name}_ready")

    cap = ", ".join(captures)

    lines = []
    lines.append(f"auto {f.name} = [{cap}]({', '.join(sig)}){ret} {{")

    if not f.always:
        lines.append(f"    *{f.name}_valid = true;")

    for arg in f.args:
        direction, _, name = classify_arg(arg)
        if direction == "in":
            lines.append(f"    *{f.name}_{name} = {name};")
        else:
            lines.append(f"    *{name} = {f.name}_{name};")

    if f.ret != "void":
        lines.append(f"    return {f.name}_ready;")

    lines.append("};")
    return "\n".join(lines)


# -------------------------------------------------
# register generation
# -------------------------------------------------

def gen_reg_state_defs(name: str, width: str, **_):
    return f"{width} {name} = 0;"


def gen_reg_cycle_defs(name: str, width: str, write_ports: List[int]):
    out = []
    for i in write_ports:
        out.append(f"bool {name}_write_en_{i} = false;")
        out.append(f"{width} {name}_write_data_{i} = 0;")
    return "\n".join(out)


def gen_reg_write_lambdas(name: str, width: str, write_ports: List[int]):
    out = []
    for i in write_ports:
        out.append(
f"""auto {name}_write_{i} = [
    &{name}_write_en_{i},
    &{name}_write_data_{i}
]({width} value) {{
    {name}_write_en_{i} = true;
    {name}_write_data_{i} = value;
}};"""
        )
    return "\n".join(out)


def gen_reg_getter_func(name: str, width: str):
    return f"""
{width} {name}_get() {{
    return {name};
}}
""".strip()


def gen_reg_commit(name: str, write_ports: List[int]) -> str:
    lines = []
    for idx, i in enumerate(sorted(write_ports)):
        kw = "if" if idx == 0 else "else if"
        lines.append(
f"""{kw} ({name}_write_en_{i}) {{
    {name} = {name}_write_data_{i};
}}"""
        )
    return "\n".join(lines)


def gen_reg_reset(name: str):
    return f"{name} = 0;"


# -------------------------------------------------
# passive
# -------------------------------------------------

def gen_passive_block(f: Func) -> str:
    lines = []
    lines.append(f"if ({f.name}_valid) {{")

    for arg in f.args:
        _, _, name = classify_arg(arg)
        lines.append(f"    auto {name} = {f.name}_{name};")

    for line in f.body.strip().splitlines():
        lines.append("    " + line)

    lines.append("}")
    return "\n".join(lines)


# -------------------------------------------------
# top generator（新增 headers / struct_defs）
# -------------------------------------------------

def generate(
    regs,
    funcs,
    main_body: str,
    headers: List[str],
    struct_defs: List[str],
) -> str:
    out = []

    # headers
    for h in headers:
        out.append(h)
    out.append("")

    # struct definitions
    for s in struct_defs:
        out.append(s)
        out.append("")

    # register state
    for r in regs:
        out.append(gen_reg_state_defs(**r))
    out.append("")

    # getters
    for r in regs:
        out.append(gen_reg_getter_func(r["name"], r["width"]))
        out.append("")

    # tick
    out.append("void tick(")
    out.append("    " + gen_tick_ports(funcs))
    out.append(") {")
    out.append("")

    for r in regs:
        out.append(gen_reg_cycle_defs(**r))
        out.append("")

    for r in regs:
        out.append(gen_reg_write_lambdas(**r))
        out.append("")

    for f in funcs:
        if f.direction == "active":
            out.append(gen_active_lambda(f))
            out.append("")

    for f in funcs:
        if f.direction == "passive":
            out.append(gen_passive_block(f))
            out.append("")

    out.append("{")
    out.append(main_body.strip())
    out.append("}")
    out.append("")

    out.append("// ---- register commit ----")
    out.append("if (rst) {")
    for r in regs:
        out.append(f"    {gen_reg_reset(r['name'])}")
    out.append("} else {")
    for r in regs:
        out.append(gen_reg_commit(r["name"], r["write_ports"]))
    out.append("}")

    out.append("}")
    return "\n".join(out)


# -------------------------------------------------
# directives.tcl
# -------------------------------------------------

def gen_directives_tcl(funcs: List[Func]) -> str:
    ports = extract_port_names(gen_tick_ports(funcs))

    lines = []
    lines.append("set_directive_interface -mode ap_ctrl_none tick")
    lines.append("set_directive_pipeline tick")
    lines.append("set_directive_pipeline -II 1 tick\n")

    for p in ports:
        lines.append(f"set_directive_interface -mode ap_none tick {p}")

    return "\n".join(lines)


def gen_run_hls_tcl() -> str:
    return """open_component -reset auto_integrated_module
set_top tick
add_files top.cpp
set_part {xa7a12tcsg325-1q}
create_clock -period 10 -name default
source "./directives.tcl"
csynth_design
exit
"""


def write_file(path: str, content: str):
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


# -------------------------------------------------
# 测试用例
# -------------------------------------------------

headers = [
    "#include <stdint.h>",
]

struct_defs = [
    """struct FetchInst {
    uint64_t pc;
    uint32_t inst;
};"""
]

regs = [
    dict(name="pc", width="uint64_t", write_ports=[0, 1])
]

funcs = [
    Func("icachereq", "bool", ["uint64_t addr", "uint32_t * data", "bool * hit"]),
    Func("fetchres_canpush", "bool", [], always=True),
    Func("fetchres_push", "bool", ["FetchInst & data"]),
    Func("redirect", "void", ["uint64_t nextpc"], direction="passive", body="pc_write_0(nextpc);"),
]

main_body = """
if (fetchres_canpush()) {
    uint64_t curpc = pc_get();
    uint32_t inst;
    bool hit;
    if (icachereq(curpc, &inst, &hit) && hit) {
        FetchInst finst;
        finst.pc = curpc;
        finst.inst = inst;
        if (fetchres_push(finst)) {
            pc_write_1(curpc + 4);
        }
    }
}
"""

if __name__ == "__main__":
    write_file(
        "top.cpp",
        generate(regs, funcs, main_body, headers, struct_defs),
    )
    write_file("directives.tcl", gen_directives_tcl(funcs))
    write_file("run_hls.tcl", gen_run_hls_tcl())

    print("Generated top.cpp, directives.tcl, run_hls.tcl")


    subprocess.run(
        ["vitis-run", "--mode", "hls", "--tcl", "run_hls.tcl"],
        check=True
    )
