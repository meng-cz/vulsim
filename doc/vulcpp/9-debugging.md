# 9. VulCPP 波形追踪与调试

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

### 阵列化子实例的匹配

对于阵列化子实例，可以在**实例路径**中直接写索引：

```text
top::mesh[0][0].datareg
top::mesh[*][0].datareg
top::lane[2].datareg
```

这里需要注意：
- 索引写在实例路径一侧，因此层级分隔符仍然必须使用 `::`
- 信号路径一侧仍然使用 `.`
- 例如应写 `top::mesh[*][0].datareg`，而不能写成 `top.mesh[*][0].datareg`

如果在阵列实例上**没有写索引**，则表示匹配该阵列实例类对应的**全部单元**：

```text
top::mesh.datareg
top::lane.datareg
```

上例分别表示：
- 追踪 `mesh` 阵列中所有节点的 `datareg`
- 追踪 `lane` 阵列中所有节点的 `datareg`

如果写了索引，则索引维数必须与实例声明维数一致，每一维可以是：
- 一个精确常量索引，如 `0`、`2`
- 通配符 `*`，表示该维任意合法索引

例如：
- `top::mesh[0][0].datareg`：仅匹配二维阵列中坐标为 `[0][0]` 的节点
- `top::mesh[*][0].datareg`：匹配第 1 列的所有节点
- `top::lane[2].datareg`：仅匹配一维阵列中索引为 `2` 的节点

非法示例：
- `top.mesh[*][0].datareg`
  这会报错，因为实例路径使用了 `.` 而不是 `::`
- `top::mesh[0].datareg`
  这会报错，因为 `mesh` 是二维阵列，但这里只写了 1 维索引

## 2. 断点

当启用了波形追踪后，可以进一步为仿真加入断点。断点在运行时基于**已被 trace 的具体信号路径和值**进行判断，命中后暂停仿真，并允许用户导出最近若干周期的波形。

### 2.1 命令行参数

断点功能依赖波形追踪，因此必须与 `--trace` 或 `-T/--tracefile` 一起使用。

可用参数如下：
- `--break "EXPR"`：直接在命令行中给出一个断点表达式
- `-B, --breakfile FILE`：从文件中读取断点表达式，文件中每一行代表一个断点
- `--breakcycles N`：在断点模式下缓存最近 `N` 个周期的波形，默认值为 `1024`

示例：

```bash
./vulsimgen -t example/systolic2d/Top.hpp -m example/systolic2d/Main.cpp \
    --trace "top::mesh.datareg" \
    --break "(top::mesh[0][0].datareg == 1) && (top::mesh[1][0].datareg == 10)"
```

或：

```bash
cat > break.txt <<'EOF'
(top::cons.cycle == 1)
(top::cons.cycle == 2)
EOF

./vulsimgen -t example/prodcon/TopModule.hpp -m example/prodcon/Main.cpp \
    --trace "top::cons.*" \
    -B break.txt
```

如果没有启用 trace，却给出了 `--break` 或 `--breakfile`，生成时会直接报错。

### 2.2 断点表达式格式

单个断点表达式目前支持如下形式：
- 由若干个“相等比较”子句组成
- 子句之间用 `&&` 连接
- 每个子句写成 `信号路径 == 常量`

例如：

```text
(top::mesh[0][0].datareg == 0) && (top::mesh[0][1].datareg == 1)
```

这里需要注意：
- 左侧必须是**一个精确的 traced 信号路径**
- 不允许在断点信号路径中使用 `*`
- 实例路径仍然必须使用 `::`
- 阵列化子实例若要指定某个单元，必须把完整索引写全

因此：
- `top::mesh[0][0].datareg` 是合法的
- `top::mesh[*][0].datareg` 在 trace 规则中可以合法，但在断点条件中不允许，因为它不是一个单一精确信号
- `top.mesh[0][0].datareg` 不合法，因为实例路径分隔符错误

右侧常量目前支持：
- 十进制，如 `10`
- 二进制，如 `0b1010`
- 十六进制，如 `0x0A`

生成时会检查该常量是否能放入对应 traced 信号的位宽中，超宽会直接报错。

### 2.3 断点文件的语义

若使用 `--breakfile`，则文件中：
- 每一行表示**一组断点**
- 行内各子句用 `&&` 连接
- 行与行之间按“或”逻辑连接

也就是说：

```text
(top::cons.cycle == 1)
(top::cons.cycle == 2) && (top::cons.sum == 0)
```

等价于：

```text
((top::cons.cycle == 1)) || ((top::cons.cycle == 2) && (top::cons.sum == 0))
```

### 2.4 断点与 trace 的关系

断点只允许引用**本次生成时会被 trace 的信号**。

例如：
- 若 `--trace "top::mesh.datareg"`，则可以在断点中引用 `top::mesh[0][0].datareg`
- 若没有 trace 到 `top::mesh[0][0].datareg`，则该断点会在生成时被判定为非法

这意味着：
- 断点校验发生在 `vulsimgen` 生成阶段，而不是运行后
- 断点引用的路径必须与生成出来的真实 traced 信号路径一一对应

### 2.5 运行时暂停与交互

当断点命中时，仿真会暂停，并在标准输入输出上提示用户选择：
- `d`：导出当前缓存的最近若干周期波形到一个 VCD 文件
- `c`：继续仿真
- `q`：退出仿真

若选择 `d`，程序会继续询问导出文件名：
- 直接回车时，使用默认文件名 `break_<cycle>.vcd`
- 也可以手动输入任意输出路径

### 2.6 波形缓存行为

未启用断点时：
- 所有 traced 波形按通常方式写入 `trace.vcd`

启用断点时：
- 运行时不会持续把全部波形写到 `trace.vcd`
- 而是只在内存中缓存最近 `--breakcycles` 指定数量的周期
- 当断点命中并选择导出时，把这段缓存写出为一个独立的 VCD 文件

因此，断点模式更适合：
- 长时间运行但只关心命中前后若干周期
- 避免在全程记录大体量 VCD 时带来的空间开销

### 2.7 暂停逻辑

多组 OR 断点采用如下规则：
- 每一组断点都按**独立边沿触发**
- 整体上按 OR 组合决定是否暂停
- 用户选择 `continue` 后，只抑制**当前仍为真**的那些组
- 被抑制的组必须先变假，之后才会重新武装
- 重新武装后，需要再次从“假变真”才会再次触发暂停

可以把它理解为：
- 每一行断点都有自己的“上升沿检测器”
- 某一行已经命中且用户选择继续后，只要这一行条件一直保持为真，就不会反复停下
- 但其他尚未命中过、或已经重新变假再变真的行，仍然可以独立触发新的暂停

例如：

```text
(top::cons.cycle == 1)
(top::cons.cycle == 2)
```

运行时会在：
- `cycle == 1` 时暂停一次
- 用户继续后，在 `cycle == 2` 时再暂停一次

而对于持续为真的条件，例如：

```text
(top::mesh[0][0].datareg == 1) && (top::mesh[1][0].datareg == 10)
```

如果该条件从某个周期开始连续多个周期都保持为真，则：
- 第一次变真时会暂停
- 用户选择 `continue` 后，只要该条件没有重新变假，就不会在后续周期反复暂停
