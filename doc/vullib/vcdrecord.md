# `vullib/vcdrecord.hpp` 开发文档

本文档描述 `GlobalVCDRecord` 的状态机、记录语义、断点模式以及目前实现中需要确认的地方。

## 1. 组件概览

```cpp
struct TraceBreakConditionSpec {
    std::string signal_name;
    std::string expected_bits;
    std::string expected_display;
};

class GlobalVCDRecord {
public:
    GlobalVCDRecord();
    ~GlobalVCDRecord();

    uint32_t registe(const std::string &signal_name, uint32_t signal_width);
    void set_break_history_cycles(uint64_t cycle_count);
    void add_break_point(const std::vector<TraceBreakConditionSpec> &conditions,
                         const std::string &expr_text);
    void init(const std::string &filename, uint64_t cycle_time, uint64_t write_interval);
    void record(uint32_t signal_id, uint64_t signal_value);
    void record(uint32_t signal_id, const std::vector<uint64_t> &signal_value);
    void commit();
    void close();
};
```

## 2. 状态机

内部状态：

```cpp
enum class State {
    Registering,
    Recording,
    Closed,
};
```

### 2.1 状态约束

- `registe()`、`set_break_history_cycles()`、`add_break_point()`、`init()`
  - 只能在 `Registering`
- `record()`、`commit()`
  - 只能在 `Recording`
- `close()`
  - 任意状态都可调用；若已经 `Closed` 则直接返回

### 2.2 `close()` 之后

- `close()` 会把状态置为 `Closed`。
- 当前实现不会回到 `Registering`。
- 因此同一个对象关闭后不能重新注册信号并再次 `init()` 复用。

## 3. 信号注册

### 3.1 `registe(name, width)`

- 该接口名称拼写就是 `registe`，这是现有 API 名的一部分。
- 要求：
  - `signal_name` 非空
  - `signal_width > 0`
- 返回一个连续分配的 `signal_id`，从 0 开始递增。

### 3.2 VCD 标识符生成

- 内部使用可打印 ASCII `[33, 126]` 做 base-94 编码。
- `signal_id` 越大，生成的 `vcd_id` 越长。

## 4. 初始化与文件输出

### 4.1 `init(filename, cycle_time, write_interval)`

- 要求：
  - 必须至少已注册一个信号
  - `filename` 非空
  - `cycle_time > 0`
- 初始化后：
  - `cycle_count_ = 0`
  - 清空内存缓冲 `buffer_`
  - 清空断点历史 `history_`

### 4.2 普通模式

- 若当前没有断点，则进入普通写文件模式。
- `init()` 会立即打开目标文件，并写入 VCD header 与 `$dumpvars` 初值。
- 初始值写为：
  - 宽度 1：`x`
  - 宽度 > 1：全 `x`

### 4.3 断点模式

- 只要注册过至少一个断点，`init()` 就进入断点模式。
- 在断点模式下：
  - 不会打开 `filename`
  - 不会实时写普通 VCD 文件
  - 仅在命中断点后由用户交互决定是否导出缓冲波形

### 4.4 `write_interval`

- `write_interval == 0`
  - 普通模式下 `commit()` 不自动 flush
  - 数据仅在 `close()` 时 flush
- `write_interval > 0`
  - 普通模式下每逢 `cycle_count % write_interval == 0` 自动 flush

## 5. `record()` 语义

## 5.1 `record(signal_id, uint64_t)`

- 只能用于 `signal_width <= 64` 的信号。
- 若 `signal_width > 64`，抛异常。
- 若 `signal_width < 64`，会检查高位是否越界。
- 若 `signal_width == 64`，不做额外掩码检查。
- 成功后把本 tick 的待提交值写入 `pending_bits`，并设置 `has_pending = true`。

## 5.2 `record(signal_id, vector<uint64_t>)`

- 用于任意宽度，但宽度 `<= 64` 时也允许调用。
- 要求 `vector` 至少提供 `ceil(width / 64)` 个 word。
- 对于宽度 `> 64`：
  - 低 word 在 `words[0]`
  - 高 word 在更高下标
- 若声明宽度之外的高 word 或最高 word 的无效高位存在非零比特，则抛异常。

## 5.3 同一 tick 多次 `record()`

- 当前实现中，同一信号若多次 `record()`，后一次会覆盖前一次的 `pending_bits`。
- 因此是 last-wins。

## 6. `commit()` 语义

### 6.1 周期推进

- 每次 `commit()` 先执行 `++cycle_count_`。
- 随后遍历所有信号：
  - 只有 `has_pending == true` 的信号会参与本次提交
  - 若本次值与 `last_bits` 不同，则生成 VCD value change
  - 然后把 `pending_bits` 清空，`has_pending` 清零

### 6.2 未被 `record()` 的信号

- 本 tick 未 `record()` 的信号不会被改动。
- 它会保持上一次提交后的 `last_bits`。
- 因此该类默认采用“只记录变化信号”的模型。

### 6.3 时间戳

- 若本 cycle 有任何变化，则向缓冲写入：

```text
#(cycle_count * cycle_time)
<changes...>
```

- 若本 cycle 没有变化，则不会写时间戳行。

## 7. 断点机制

## 7.1 注册断点

### 接口

```cpp
void add_break_point(const std::vector<TraceBreakConditionSpec> &conditions,
                     const std::string &expr_text);
```

### 规则

- `conditions` 不能为空。
- 每个条件的 `signal_name` 必须对应已注册信号。
- `expected_bits.size()` 必须与信号宽度完全一致。
- 断点命中条件是：
  - 所有参与信号的 `last_bits` 都等于各自 `expected_bits`

### 说明

- `expr_text` 仅用于命中时打印展示。
- `expected_display` 目前仅被保存，不参与逻辑判断。

## 7.2 命中行为

- 命中断点时会在标准输出打印提示。
- 随后进入交互式循环，允许用户选择：
  - `d`：导出缓冲 VCD
  - `c`：继续运行
  - `q`：调用 `close()` 后 `std::exit(0)`

## 7.3 抑制重复命中

- 若断点当前已为真并继续运行，则会进入 `suppress_until_false` 状态。
- 只有该条件先变假，再次变真，才会再次触发。

## 7.4 断点历史

- `history_` 保存最近若干个 cycle 的：
  - `cycle_index`
  - 该 cycle 提交前各信号快照
  - 该 cycle 的变化串
- 默认历史深度 `break_history_cycles_ = 64`。
- `set_break_history_cycles(0)` 是允许的，结果是历史会被立即裁剪为空。

## 8. VCD header 与作用域生成

### 8.1 路径拆分

- 信号名中的以下分隔符会被解释为层级路径：
  - `::`
  - `.`

例如：

- `top.core.valid`
- `top::core::valid`

都会形成多层 `$scope module ... $end`。

### 8.2 Header 内容

- `$timescale` 固定写成 `<cycle_time>ns`
- 顶层固定写为：

```text
$scope module logic $end
```

- 同一 scope 内信号按名字字典序输出。

## 9. `close()` 语义

- 若处于普通模式且文件已打开：
  - 先 flush `buffer_`
  - 再关闭文件
- 然后清空注册信号、断点、历史、计数器和缓冲
- 最终进入 `Closed`

析构函数会调用 `close()`，并吞掉所有异常。

## 10. 待确认项

- 断点模式下 `init(filename, ...)` 中的 `filename` 实际不会生成正常波形文件；如果需求期望“既能断点又能正常持续写 trace”，当前实现不满足。
- `close()` 后对象不能复用；若需求希望一个 recorder 可重复 `init/close`，当前实现不满足。
- `expected_display` 当前未参与任何逻辑，也未在命中输出中使用，属于“已建模但未使用”字段。
- 断点命中后使用交互式 `stdin/stdout`，并且 `q` 会直接 `std::exit(0)`；若该组件需要作为非交互库或嵌入式仿真框架组件使用，这个行为需要重点确认。
- API 名称 `registe` 为现状拼写，若未来要对外长期暴露，建议尽早确认是否保持兼容还是增加别名接口。
