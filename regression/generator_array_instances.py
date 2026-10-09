#!/usr/bin/env python3
"""Concrete array identities, coordinate binding, generated naming and diagnostics."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SIMGEN = ROOT / "build/vulsimgen"
RTLGEN = ROOT / "build/vulrtlgen"


def run(command):
    return subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def case(name, modules, body="sim_nextcycle();", expected_error=None, check=None, rtl=False, rtl_check=None):
    with tempfile.TemporaryDirectory(prefix=f"array-{name}-") as directory:
        path = Path(directory)
        (path / "header.hpp").write_text("#pragma once\n#include <defhelper.hpp>\n")
        for filename, text in modules.items():
            (path / filename).write_text('#include "header.hpp"\n' + text)
        (path / "Main.cpp").write_text(
            '#include <cstdlib>\n#include <defhelper.hpp>\n#include <run.hpp>\n'
            'TOP("./Top.hpp");\nPROJECT(".");\nQUERY(result, uint32_t);\n'
            'SIMULATION() { ' + body + ' }\n'
        )
        result = run([str(SIMGEN), "-m", str(path / "Main.cpp"), "-f", "-o", str(path / "out")])
        if expected_error:
            if result.returncode == 0 or expected_error not in result.stdout:
                raise RuntimeError(f"{name}: expected {expected_error!r}\n{result.stdout}")
        else:
            if result.returncode:
                raise RuntimeError(f"{name}: generation failed\n{result.stdout}")
            if check:
                check(path / "out", result.stdout)
            simulated = run([str(path / "out/run.sh")])
            if simulated.returncode:
                raise RuntimeError(f"{name}: simulation failed\n{simulated.stdout}")
        if rtl:
            generated = run([str(RTLGEN), "-m", str(path / "Main.cpp"), "-f", "-o", str(path / "rtl")])
            if generated.returncode:
                raise RuntimeError(f"{name}: RTL generation failed\n{generated.stdout}")
            if rtl_check:
                rtl_check(path / "rtl", generated.stdout)
            simulated = run([str(path / "rtl/run.sh")])
            if simulated.returncode:
                raise RuntimeError(f"{name}: RTL simulation failed\n{simulated.stdout}")
        print(f"generator array check PASS: {name}")


def names_check(out, log):
    expected = {"sim/top", "sim/top/arr1", "sim/top/arr1/fpu", "sim/top/arr2", "sim/top/arr2/fpu"}
    # Include the top module, which is beside the top/ directory.
    actual = {str(p.relative_to(out))[:-9] for p in out.rglob("*.decl.hpp") if str(p.relative_to(out)).startswith("sim/top")}
    implementations = {str(p.relative_to(out))[:-9] for p in out.rglob("*.impl.hpp") if str(p.relative_to(out)).startswith("sim/top")}
    if actual != expected or implementations != expected or "Total instances: 25" not in log:
        raise RuntimeError(f"incorrect generated identities: {actual}\n{log}")
    for name in ("arr1", "arr2"):
        pe = (out / f"sim/top/{name}.decl.hpp").read_text()
        fpu = (out / f"sim/top/{name}/fpu.decl.hpp").read_text()
        if f"class PE_sim_top_{name};" not in pe or f"class FPU_sim_top_{name}_fpu;" not in fpu:
            raise RuntimeError("missing shared template declaration")
        if f"PE_sim_top_{name}<" not in fpu or "class Parent" in fpu:
            raise RuntimeError("incorrect concrete parent type")


def rtl_names_check(out, log):
    expected = {"sim/top", "sim/top/arr1", "sim/top/arr1/fpu", "sim/top/arr2", "sim/top/arr2/fpu"}
    actual = {str(p.relative_to(out))[:-3] for p in (out / "sim").rglob("*.sv") if "__v" not in p.stem}
    if actual != expected or "Total instances: 25" not in log:
        raise RuntimeError(f"incorrect public RTL identities: {actual}\n{log}")
    for name in ("arr1", "arr2"):
        text = (out / f"sim/top/{name}/fpu.sv").read_text()
        if f"module FPU_sim_top_{name}_fpu #(" not in text or "__vul_idx_0" not in text:
            raise RuntimeError("missing ancestor coordinate context in RTL module")
        # All FPU elements here have identical behavior: one shared implementation per path.
        if len(list((out / f"sim/top/{name}").glob("fpu__v*.sv"))) != 1:
            raise RuntimeError("identical RTL implementations were not deduplicated")


case("five-names", {
    "Top.hpp": '''CHILD_INSTANCE(PE, arr1, dims=[8]); CHILD_INSTANCE(PE, arr2, dims=[4]); QUERY(result, uint32_t) { return 0; }''',
    "PE.hpp": "CHILD_INSTANCE(FPU, fpu);",
    "FPU.hpp": "REGISTER(value, uint32_t);",
}, check=names_check, rtl=True, rtl_check=rtl_names_check)

base = {"PE.hpp": "INTERFACE() { PARAMETER(ROW, 3); PARAMETER(COL, 5); } QUERY(result, uint32_t) { return ROW*10+COL; }"}
for name, decl, error in [
    ("scalar-binding", "COORD(0)=ROW", "COORD dimension out of range"),
    ("out-of-range", "dims=[2], COORD(1)=ROW", "COORD dimension out of range"),
    ("duplicate-axis", "dims=[2], COORD(0)=ROW, COORD(0)=COL", "Duplicate COORD binding"),
    ("duplicate-param", "dims=[2,2], COORD(0)=ROW, COORD(1)=ROW", "Duplicate COORD binding"),
    ("unknown-param", "dims=[2], COORD(0)=MISSING", "Unknown child PARAMETER"),
    ("override-after", "dims=[2], COORD(0)=ROW, PARAM(ROW)=7", "cannot be overridden"),
    ("override-before", "dims=[2], PARAM(ROW)=7, COORD(0)=ROW", "cannot be overridden"),
]:
    case(name, dict(base, **{"Top.hpp": f"CHILD_INSTANCE(PE, a, {decl}); QUERY(result, uint32_t) {{ return 0; }}"}), expected_error=error)

case("partial-binding", dict(base, **{"Top.hpp": '''
CHILD_INSTANCE(PE, a, dims=[2,3], COORD(1)=COL);
USE_CHILD_QUERY(a[1][2], result, child_result, uint32_t);
QUERY(result, uint32_t) { return child_result(); }
'''}), body="if (result() != 32) std::exit(1); sim_nextcycle(); if (result() != 32) std::exit(2);")

case("legacy-binding", dict(base, **{"Top.hpp": '''
CHILD_INSTANCE_ARRAY2(PE, a, 1, 3, COORD(1)=COL);
USE_CHILD_QUERY(a[0][2], result, child_result, uint32_t);
QUERY(result, uint32_t) { return child_result(); }
'''}), body="if (result() != 32) std::exit(1); sim_nextcycle();")

case("full-non-square-binding", dict(base, **{"Top.hpp": '''
CHILD_INSTANCE(PE, a, dims=[2,3], COORD(0)=ROW, COORD(1)=COL);
USE_CHILD_QUERY(a[1][2], result, child_result, uint32_t);
QUERY(result, uint32_t) { return child_result(); }
'''}), body="if (result() != 12) std::exit(1); sim_nextcycle(); if (result() != 12) std::exit(2);", rtl=True)

case("nested-structure", {
    "Top.hpp": '''CHILD_INSTANCE(PE, a, dims=[2], COORD(0)=ROW);
USE_CHILD_QUERY(a[1], result, child_result, uint32_t);
QUERY(result, uint32_t) { return child_result(); }''',
    "PE.hpp": '''INTERFACE() { PARAMETER(ROW, 9); }
CHILD_INSTANCE(FPU, fpu, dims=[ROW+1], PARAM(TAG)=ROW+7);
USE_CHILD_QUERY(fpu[ROW], result, child_result, uint32_t);
QUERY(result, uint32_t) { return child_result(); }''',
    "FPU.hpp": '''INTERFACE() { PARAMETER(TAG, 2); PARAMETER(NEXT, TAG+1); }
REGISTER(value, Int<NEXT>) { value = TAG; }
QUERY(result, uint32_t) { return value.get().to<uint32_t>() + NEXT*100; }''',
}, body="if (result() != 908) std::exit(1); sim_nextcycle(); if (result() != 908) std::exit(2);", rtl=True)

case("registered-chain", {
    "Top.hpp": '''CHILD_INSTANCE(Node, lane, dims=[3]);
CONNECT_CR_CS(lane[$], send, lane[$+1], receive);
CONNECT_CR_S(lane[2], send, finish);
SERVICE(finish, ARG(uint32_t) value) { }
QUERY(result, uint32_t) { return 0; }''',
    "Node.hpp": '''REQUEST(send, ARG(uint32_t) value); REGISTER(value, uint32_t);
SERVICE(receive, ARG(uint32_t) data) { value.setnext(data); }
TICK_IMPL() { send(value.get()+1); }''',
})

case("true-array-cycle", {
    "Top.hpp": '''CHILD_INSTANCE(Node, lane, dims=[2]);
CONNECT_CR_CS(lane[$], send, lane[1-$], receive);
USE_CHILD_SERVICE(lane[0], receive, enter);
TICK_IMPL() { enter(); } QUERY(result, uint32_t) { return 0; }''',
    "Node.hpp": "REQUEST(send); SERVICE(receive) { send(); }",
}, expected_error="Cyclic call")

case("array-multiple-sources", {
    "Top.hpp": '''CHILD_INSTANCE(Node, lane, dims=[2]); CHILD_INSTANCE(Sink, sink);
CONNECT_CR_CS(lane[$], send, sink, receive); QUERY(result, uint32_t) { return 0; }''',
    "Node.hpp": "REQUEST(send); TICK_IMPL() { send(); }",
    "Sink.hpp": "SERVICE(receive) { }",
}, expected_error="called by multiple instances")

case("array-priority", {
    "Top.hpp": '''WIRE(value, uint32_t) { value = 0; } REGISTER(saved, uint32_t);
CHILD_INSTANCE(Node, lane, dims=[2], COORD(0)=ROLE);
CONNECT_CR_S(lane[1], high, high); CONNECT_CR_S(lane[0], low, low);
SERVICE(high, priority=20) { value = 7; }
SERVICE(low, priority=-20) { saved.setnext(value); }
TICK_IMPL() { value = value+10; }
QUERY(result, uint32_t) { return saved.get(); }''',
    "Node.hpp": '''INTERFACE() { PARAMETER(ROLE, 0); REQUEST(high); REQUEST(low); }
TICK_IMPL() { if constexpr (ROLE == 1) high(); else low(); }''',
}, expected_error="is not connected")
# Each declared request still needs a connection, even when its call is statically inactive.
case("array-priority-connected", {
    "Top.hpp": '''WIRE(value, uint32_t) { value = 0; } REGISTER(saved, uint32_t);
CHILD_INSTANCE(Node, lane, dims=[2], COORD(0)=ROLE);
CONNECT_CR_S(lane[*], high, high); CONNECT_CR_S(lane[*], low, low);
SERVICE(high, array=2, priority=20) { value = 7; }
SERVICE(low, array=2, priority=-20) { saved.setnext(value); }
TICK_IMPL() { value = value+10; }
QUERY(result, uint32_t) { return saved.get(); }''',
    "Node.hpp": '''INTERFACE() { PARAMETER(ROLE, 0); REQUEST(high); REQUEST(low); }
TICK_IMPL() { if constexpr (ROLE == 1) high(); else low(); }''',
}, body="sim_nextcycle(); if (result() != 17) std::exit(1);", rtl=True)

case("no-user-parameter-leak", {
    "Top.hpp": "CHILD_INSTANCE(PE, a, dims=[2], COORD(0)=ROW); USE_CHILD_QUERY(a[1], result, child, uint32_t); QUERY(result, uint32_t) { return child(); }",
    "PE.hpp": "INTERFACE() { PARAMETER(ROW, 0); } CHILD_INSTANCE(FPU, fpu); USE_CHILD_QUERY(fpu, result, child, uint32_t); QUERY(result, uint32_t) { return child(); }",
    "FPU.hpp": "INTERFACE() { PARAMETER(ROW, 99); } QUERY(result, uint32_t) { return ROW; }",
}, body="if (result() != 99) std::exit(1); sim_nextcycle();", rtl=True)

case("internal-coordinate-reference", {
    "Top.hpp": "CHILD_INSTANCE(PE, a, dims=[2]); QUERY(result, uint32_t) { return 0; }",
    "PE.hpp": "QUERY(result, uint32_t) { return __vul_idx_0; }",
}, expected_error="not a user API")

case("coordinate-dependent-query-width", {
    "Top.hpp": "CHILD_INSTANCE(PE, a, dims=[2], COORD(0)=ROW); USE_CHILD_QUERY(a[1], result, child, uint32_t); QUERY(result, uint32_t) { return child(); }",
    "PE.hpp": "INTERFACE() { PARAMETER(ROW, 0); } CHILD_INSTANCE(FPU, fpu, PARAM(WIDTH)=ROW+1); USE_CHILD_QUERY(fpu, result, child, Int<ROW+1>); QUERY(result, uint32_t) { return child().to<uint32_t>(); }",
    "FPU.hpp": "INTERFACE() { PARAMETER(WIDTH, 9); } REGISTER(value, Int<WIDTH>) { value = 1; } QUERY(result, Int<WIDTH>) { return value.get(); }",
}, body="if (result() != 1) std::exit(1); sim_nextcycle(); if (result() != 1) std::exit(2);", rtl=True)

case("comparison-in-call-index", {
    "Top.hpp": "REGISTER(saved, uint32_t); CHILD_INSTANCE(Node, n, PARAM(ROLE)=1); CONNECT_CR_S(n, send, capture); SERVICE(capture, array=2, ARG(uint32_t) data) { if constexpr (IDX == 1) { saved.setnext(data); } } QUERY(result, uint32_t) { return saved.get(); }",
    "Node.hpp": "INTERFACE() { PARAMETER(ROLE, 0); REQUEST(send, array=2, ARG(uint32_t) data); } TICK_IMPL() { send<((ROLE > 0) ? 1 : 0)>(9); }",
}, body="sim_nextcycle(); if (result() != 9) std::exit(1);", rtl=True)

case("concrete-name-collision", {
    "Top.hpp": "CHILD_INSTANCE(PE, arr, dims=[1], COORD(0)=ROW); CHILD_INSTANCE(PE, arr__0, PARAM(ROW)=7); USE_CHILD_QUERY(arr[0], result, first, uint32_t); USE_CHILD_QUERY(arr__0, result, second, uint32_t); QUERY(result, uint32_t) { return first()+second()*100; }",
    "PE.hpp": "INTERFACE() { PARAMETER(ROW, 3); } REGISTER(value, uint32_t) { value = ROW+1; } QUERY(result, uint32_t) { return value.get(); }",
}, body="if (result() != 801) std::exit(1); sim_nextcycle(); if (result() != 801) std::exit(2);", rtl=True)

case("ignore-constexpr-in-literal", {
    "Top.hpp": 'QUERY(result, uint32_t) { return 0; } TICK_IMPL() { const char* text = "if constexpr (unknown) call();"; }',
})

case("query-alias-later-coordinate-mismatch", {
    "Top.hpp": "CHILD_INSTANCE(PE, arr, dims=[2], COORD(0)=ROW); USE_CHILD_QUERY(arr[*], result, child, Int<1>); QUERY(result, uint32_t) { return child<0>().to<uint32_t>(); }",
    "PE.hpp": "INTERFACE() { PARAMETER(ROW, 0); } QUERY(result, Int<ROW+1>) { return 0; }",
}, expected_error="USE_CHILD_QUERY return type mismatch")

case("service-alias-later-coordinate-mismatch", {
    "Top.hpp": "CHILD_INSTANCE(PE, arr, dims=[2], COORD(0)=ROW); USE_CHILD_SERVICE(arr[*], load, child); QUERY(result, uint32_t) { return 0; }",
    "PE.hpp": "INTERFACE() { PARAMETER(ROW, 0); SERVICE(load, ARG(Int<ROW+1>) value); } SERVICE(load, ARG(Int<ROW+1>) value) { }",
}, expected_error="USE_CHILD_SERVICE signature mismatch")

# Keep the original example unchanged and assert its actual observable behavior.
with tempfile.TemporaryDirectory(prefix="array-systolic2d-") as directory:
    expected = ["systolic2d tick=0 spill=11", "systolic2d tick=1 row=0 data=0",
                "systolic2d tick=1 row=1 data=3"]
    for generator, target in ((SIMGEN, "sim"), (RTLGEN, "rtl")):
        out = Path(directory) / target
        generated = run([str(generator), "-m", "example/systolic2d/Main.cpp",
                         "-t", "example/systolic2d/Top.hpp", "-p", "example/systolic2d",
                         "-f", "-o", str(out)])
        if generated.returncode:
            raise RuntimeError(f"systolic2d {target}: generation failed\n{generated.stdout}")
        simulated = run([str(out / "run.sh")])
        actual = [line for line in simulated.stdout.splitlines() if line.startswith("systolic2d ")]
        if simulated.returncode or actual != expected:
            raise RuntimeError(f"systolic2d {target}: incorrect output {actual}\n{simulated.stdout}")
    print("generator array check PASS: original-systolic2d-both-paths")

with tempfile.TemporaryDirectory(prefix="array-trace-") as directory:
    out = Path(directory)
    generated = run([str(SIMGEN), "-m", "regression/array_instances/Main.cpp", "-f",
                     "-o", str(out), "--trace", "top::arr1[1]::fpu.value,top::arr2[*]::fpu.value"])
    if generated.returncode:
        raise RuntimeError(f"concrete trace: generation failed\n{generated.stdout}")
    simulated = run([str(out / "run.sh")])
    if simulated.returncode:
        raise RuntimeError(f"concrete trace: simulation failed\n{simulated.stdout}")
    trace = (out / "trace.vcd").read_text()
    scopes = [line for line in trace.splitlines() if "$scope module arr" in line]
    expected = {f"$scope module {name} $end" for name in ["arr1[1]", "arr2[0]", "arr2[1]", "arr2[2]", "arr2[3]"]}
    if len(scopes) != 5 or set(scopes) != expected:
        raise RuntimeError(f"concrete trace: incorrect coordinate paths {scopes}")
    print("generator array check PASS: concrete-trace-paths")
