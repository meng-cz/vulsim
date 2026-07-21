# RTLZZ 全量示例回归问题

## 测试范围

测试日期：2026-07-20
RTLZZ 版本：`f83a301`（`new abi: port decl via global var`）

统一使用以下形式运行：

```bash
build/vulrtlgen -t <TOP> -p <PROJECT> -o <TMP_OUT> --v2
```

覆盖了 `example` 下全部 17 个项目。对于包含多个独立顶层的
`ooo_backend`，额外分别测试了 `ALUPipeline.hpp`、`LSUPipeline.hpp`
和 `BackendCore.hpp`。

## 结果概览

| 项目/顶层 | 结果 | 首个失败阶段 |
|---|---:|---|
| `aes1/AES1.hpp` | 失败 | S2 validate |
| `apiinline_proxy/Top.hpp` | 失败 | S3 statementize |
| `apiinline_register/Top.hpp` | 失败 | S2 validate |
| `array1d/Top.hpp` | 失败 | S3 statementize |
| `cachetest/SimpleCache.hpp` | 失败 | S2 validate |
| `childalias/Top.hpp` | 失败 | S3 statementize |
| `enumdemo/Top.hpp` | 通过 | — |
| `fifotest/Top.hpp` | 失败，非 RTLZZ issue | 项目结构检查 |
| `mulu32/MulU32.hpp` | 失败 | S7 flatten |
| `ooo_backend/core/ALUPipeline.hpp` | 失败，非 RTLZZ issue | 输入 C++ 常量作用域错误 |
| `ooo_backend/core/LSUPipeline.hpp` | 失败 | S3 statementize |
| `ooo_backend/core/BackendCore.hpp` | 失败，非 RTLZZ issue | 输入 C++ 常量作用域错误 |
| `pipeline_holdreset/Top.hpp` | 失败 | S3 statementize |
| `prodcon/TopModule.hpp` | 通过 | — |
| `querydemo/Top.hpp` | 失败 | S3 statementize |
| `queue_mp_demo/Top.hpp` | 失败 | S9 SSA |
| `rv64ima5/Top.hpp` | 失败 | S8 opnorm |
| `systolic2d/Top.hpp` | 通过 | — |
| `verilator_fsm/Top.hpp` | 失败 | S8 opnorm |

下面只整理输入已经是合法 C++、需要 RTLZZ 补齐或修正的行为。

## 1. S3 无法处理 helper 返回的聚合值或作为参数传递的聚合临时值

受影响项目：

- `apiinline_proxy`
- `pipeline_holdreset`
- `ooo_backend/core/LSUPipeline.hpp`

典型输入：

```cpp
pair[1].data =
    __vul_bram_readdata_bank_mem(1, bram_bank_mem__s2_readdata).data
    + ...;

__vul_reg_setnext_pipe(
    2,
    __vul_read_reg_pipe(rdata_pipe__[1]),
    wdata_pipe__,
    wen_pipe__);
```

诊断：

```text
s3statementize: Expression is not a supported lvalue
```

期望：

- 支持从返回 struct 的 helper 调用结果读取成员。
- 支持把返回 struct 的 helper 调用结果按值传给另一个 helper。
- lambda/helper inline 后产生的聚合临时值应先 statementize 为临时变量，
  再执行成员访问或参数绑定。

`LSUPipeline` 的位置映射最终落在 lambda 调用附近，建议同时检查 S6
inline 后的 source location 和聚合/引用实参 lowering。

## 2. S3 在合法 lambda 调用上报告不存在于源码中的 `?` 运算符

受影响项目：

- `array1d`
- `childalias`
- `querydemo`

诊断：

```text
s3statementize: Unsupported binary operator '?'
```

生成的 `.logic.cpp` 对应位置是普通 lambda 调用或紧随其后的代码，例如：

```cpp
tick0__();
```

对应生成文件中不存在字面量 `?`。这说明该运算符很可能来自 S0/S1
对 lambda、宏展开调用或条件值的内部编码，而不是用户代码中的三元表达式。

期望：

- S3 应正确区分 conditional operator 和内部生成的条件/选择节点。
- 若确实存在三元表达式，应按已有 ternary lowering 路径处理。
- 诊断应指向产生该节点的真实表达式，而不是 lambda 调用点。

## 3. S2 将本地 `std::array` 声明误判为未知函数调用

受影响项目：

- `apiinline_register`
- `aes1`

最小触发形态：

```cpp
std::array<uint16_t, 4> array_reg;
for (int i = 0; i < 4; ++i) {
    array_reg[i] = static_cast<uint16_t>(i);
}
```

诊断：

```text
s2validate: Unknown function call 'array'
```

期望：

- `std::array<T, N>` 的局部变量声明不应生成名为 `array` 的 call。
- 与 fixtures 中已经支持的 `std::array` 参数、端口和局部数组保持一致。

## 4. S2 未识别 lambda 中合法的 `Int<N>::at<HI, LO>()`

受影响项目：`cachetest`

触发代码：

```cpp
auto read_s0_impl__ = [&](Int<32> addr) -> void {
    Int<INDEX_WIDTH> index = addr.at<INDEX_WIDTH - 1, 0>();
    // ...
};
```

诊断：

```text
s2validate: Unknown function call 'at'
```

`Int::at` 已被 RTLZZ fixtures 覆盖；此处的差异是调用位于 lambda 内，
且模板上下界引用顶层函数内的 constexpr 配置常量。

期望：

- lambda 内的 `Int::at` 与顶层函数中的 `Int::at` 使用同一规范化路径。
- 捕获的 constexpr 整数可以用于 `at<HI, LO>` 的模板实参求值。

## 5. S7 聚合值 flatten 时误入标量运算路径

受影响项目：`mulu32`

诊断：

```text
s7flatten: Non-ternary operation target is aggregate
```

该项目包含 struct 寄存器读写、聚合初始化和聚合 helper 参数，例如：

```cpp
__vul_reg_setnext_s1reg(
    S0S1RegData{a, b},
    wdata_s1reg__,
    wen_s1reg__);
```

期望：

- 聚合构造值传入 helper、helper inline、字段 pack/unpack 后应逐字段 flatten。
- 不应把整个 struct 作为普通一元/二元标量运算的 target。
- 建议在 S7 报错中输出具体 operation kind、target type 和源表达式，
  以便区分 aggregate init、copy 和 cast。

## 6. QueueMP 返回数组经过 helper/lambda inline 后产生不完整 SSA 合流

受影响项目：`queue_mp_demo`

诊断：

```text
s9ssa: Missing incoming SSA value at CFG merge
stage=s9ssa
bb50
symbol=...__vul_queue_front_q...__vul_queue_values____idx_0
```

相关 helper 会初始化返回数组，并按 valid 数量条件写入各元素：

```cpp
std::array<T, N> values = {};
if (deqvalid > 0) values[0] = ...;
if (deqvalid > 1) values[1] = ...;
return values;
```

期望：

- 聚合数组逐元素写入在每个 CFG merge 上都保留初始化路径的 incoming value。
- helper 和 lambda inline 后的数组元素 symbol 应沿用正常的 phi placement，
  不应丢失未进入条件分支时的初始版本。

## 7. S8 对等宽 1-bit cast/赋值报告类型不匹配

受影响项目：

- `rv64ima5`
- `verilator_fsm`

诊断：

```text
s8opnorm: S8 assign value type does not match target type for
'__s8_norm_cast_*': target width 1, value width 1
```

目标和值的位宽均为 1，当前诊断没有展示 signedness、bool/`Int<1>`
类别或原始表达式，因此无法从日志判断具体不兼容维度。

期望：

- 合法的 `bool`、比较结果和 `Int<1>` 之间的规范化转换可以完成。
- 如果两者确实不兼容，诊断需同时打印完整 target/value 类型、
  signedness、hardware kind 和产生 cast 的原始位置。

## 8. 文件级 helper 无法直接访问新 ABI 的全局端口

受影响项目包括：

- `systolic2d`
- `apiinline_proxy`

新 ABI 将端口声明为带 pragma 的文件级变量，因此普通文件级 helper
应当能够像标准 C++ 函数一样直接访问这些变量：

```cpp
#pragma output_port wdata_cycle__
Int<32> wdata_cycle__;
#pragma output_port wen_cycle__
bool wen_cycle__;

void __vul_reg_setnext_cycle(uint32_t value) {
    wdata_cycle__ = Int<32>(value);
    wen_cycle__ = true;
}
```

当前诊断：

```text
s2validate: Use of undeclared variable 'wdata_cycle__'
```

期望：

- S0/S2 在分析文件级 helper 时，将 pragma 全局端口加入可见符号表。
- helper 对 output port 的写入和对 input port 的读取，在 inline 后应映射到
  顶层函数对应的 port symbol。
- API helper 不应为了绕过该限制而把所有端口重复声明为函数参数。

## 9. S2 未识别带非类型模板参数的文件级 helper

API 中的端口编号和 arrayed Request 槽位具有编译期硬件语义，生成代码会
使用文件级函数模板：

```cpp
template <uint32_t P = 0>
void __vul_reg_setnext_cycle(uint32_t value) {
    wdata_cycle__[P] = Int<32>(value);
    wen_cycle__[P] = true;
}

__vul_reg_setnext_cycle<1>(value);
```

BRAM/ROM helper 和 arrayed Request helper 使用相同形式。当前 S2 会把合法
调用报告为：

```text
s2validate: Unknown function call '__vul_reg_setnext_cycle'
```

期望：

- S0 收集文件级函数模板及其非类型模板参数。
- S2/S6 能按调用点常量实例化并 inline 对应 helper。
- 模板参数必须保留为 compile-time constant，不能降级为普通运行期参数，
  否则会把静态端口选择错误地变成动态硬件 mux。

## 未列为 RTLZZ issue 的失败

以下问题在进入 RTLZZ 后首先暴露为无效输入或项目配置问题，应先在
`rtlgen/apiinline` 或示例项目侧修复：

### `ooo_backend/ALUPipeline` 和 `BackendCore`

文件级 helper 使用 `OP_MUL`、`OP_STORE` 等配置常量，但这些 constexpr
目前被生成在顶层函数内部，位于 helper 的作用域之外：

```text
s0ast: use of undeclared identifier 'OP_MUL'
s0ast: use of undeclared identifier 'OP_STORE'
```

### `fifotest`

项目根目录缺少解析器要求的 `header/` 或 `header.hpp/header.h`，尚未进入
RTL 生成和 RTLZZ：

```text
Project root must contain either a header directory or a header.hpp/header.h file
```
