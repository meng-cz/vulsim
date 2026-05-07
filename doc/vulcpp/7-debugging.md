# 7. VulCPP 波形追踪与调试

在运行 `vulsimgen` 时可以通过命令行参数启用波形追踪功能，通过字符串匹配指定的实例名和信号名，将选中的信号波形记录到 `trace.vcd` 文件中。如：

```bash
./vulsimgen -t example/prodcon/TopModule.hpp -m example/prodcon/Main.hpp --trace "top::conn.*"
```

或是：

```bash
echo "top::conn.*" > trace_config.txt
./vulsimgen -t example/prodcon/TopModule.hpp -m example/prodcon/Main.hpp -T trace_config.txt
```

上述命令将在运行时记录 `top::conn` 实例中所有寄存器信号。

## 1. 波形匹配字符串

波形匹配字符串的基本格式为 `实例路径.信号路径`。其中实例路径用 `::` 分层（如 `top::u1::alu`），信号路径用 `.` 分层（如 `state.valid`）。

解析时按第一个 `.` 分割：左边是实例匹配规则，右边是信号匹配规则。若不写 `.`，则表示“该实例下所有信号”，例如 `top::conn` 等价于 `top::conn.*`。

特殊的，若匹配字符串仅为 `*` 或空字符串，则等价于 `*.*`，即追踪所有实例的所有信号。

匹配中唯一的通配符是 `*`，其匹配任意个（非0个）分层。普通分层必须完全相等，`*` 可以吞并后续的 1 层或多层。如实例规则 `top::*` 既能匹配 `top::conn`，也能匹配 `top::a::conn`；信号规则 `state.*` 既能匹配 `state.valid`，也能匹配 `state.pipe.ready`。但通配符不能跨越实例-信号分界匹配，例如 `top.*.valid` 不能匹配 `top::conn.state.valid`，必须写成 `top::*.*.valid`。

对于数组信号，可以仅使用名称来匹配所有数组元素，例如 `regfile` 可以匹配 `regfile[0]`、`regfile[1]` 等等；也可以使用 `regfile[3]` 来精确匹配 `regfile[3]` 这个元素。

常见写法示例：`top::conn.*` 表示追踪 `top::conn` 实例下所有层级的信号路径；`top::*.*` 表示追踪 `top` 下任意深度子实例中的所有信号；`top::cpu.regfile.*` 表示追踪 `top::cpu` 实例中 `regfile` 下所有层级子信号；`*.valid` 表示任意实例中最终变量名恰好为 `valid` 的信号。

## 2. 断点

TODO：暂未实现
