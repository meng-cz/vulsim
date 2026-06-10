# `vullib/uint.hpp` 开发文档

本文档描述 `Int<BitWidth>` 及其相关代理、签名视图和运算符的当前实现语义。它是开发文档，不是用户入门手册。

## 1. 核心类型

```cpp
template <uint32_t BitWidth>
class Int;

template <uint32_t BitWidth>
class IntSignedView;

template <uint32_t BitWidth>
class IntRangeProxy;

template <uint32_t BitWidth>
class IntConstRangeProxy;

template <uint32_t BitWidth>
class IntBitProxy;

template <uint32_t BitWidth>
class IntConstBitProxy;
```

## 2. `Int<BitWidth>` 的数据模型

- `BitWidth > 0`。
- 以下成员是公开可见的：

```cpp
static constexpr uint32_t WORD_BITS;
static constexpr uint32_t NUM_WORDS;
static constexpr uint32_t IDX_WIDTH;
std::array<uint64_t, NUM_WORDS> data;
constexpr std::array<uint64_t, NUM_WORDS> get_data() const;
```

- 内部按 64 位 word 存储：

```cpp
static constexpr uint32_t WORD_BITS = 64;
static constexpr uint32_t NUM_WORDS = (BitWidth + 63) / 64;
static constexpr uint32_t IDX_WIDTH = log2ceil(BitWidth);

std::array<uint64_t, NUM_WORDS> data;
```

- 最低有效位在 `data[0]` 的最低位。
- 若 `BitWidth` 不是 64 的整数倍，最高 word 的无效高位会通过 `mask_high_bits()` 清零。
- `data` 是公开成员，因此调用方可以直接改写底层 word；若绕过公开接口手动写入，需要自行保证高位 mask 规则成立。

## 3. 构造与转换

### 3.1 公开接口

```cpp
constexpr Int();
constexpr Int(bool value);

template <typename T> requires std::is_integral_v<T>
constexpr Int(T value);

template <uint32_t OtherBitWidth>
constexpr Int(const Int<OtherBitWidth>& other);

template <uint32_t OtherBitWidth>
constexpr Int(const IntSignedView<OtherBitWidth>& other);

template <typename T> requires std::is_integral_v<T>
constexpr T to() const;

constexpr IntSignedView<BitWidth> sint() const;
```

### 3.2 无符号 `Int -> Int` 转换

- `Int<OtherBitWidth>` 构造到 `Int<BitWidth>` 时：
  - 只复制低位 word。
  - 若目标更宽，高位补零。
  - 若目标更窄，高位截断。

### 3.3 `IntSignedView` 转换

- `sint()` 不修改底层 bit 模式，只创建一个“按有符号解释”的视图。
- 用 `IntSignedView<OtherBitWidth>` 构造 `Int<BitWidth>` 时：
  - 若目标更宽，则按源最高位做符号扩展。
  - 若目标更窄，则截断到低 `BitWidth` 位。

### 3.4 整数构造

- 从原生整型 `T` 构造时：
  - 低 64 位写入 `data[0]`
  - 若 `T` 为有符号且目标宽度超过 64 位，负数会向高 word 填充全 1
  - 最后统一做高位 mask

### 3.5 `to<T>()`

- 返回低 `sizeof(T) * 8` 位。
- 对无符号目标类型：
  - 直接截断/拼接低位。
- 对有符号目标类型：
  - 若 `BitWidth < T` 位宽，则以 `Int` 的最高有效位作为符号位做符号扩展。
  - 若 `BitWidth >= T` 位宽，则直接截断到目标位宽。

### 3.6 待确认项

- `to<T>()` 的模板约束使用 `std::is_integral_v<T>`，通常不把 GCC/Clang 的 `__int128` 视为标准 integral；源码中虽然有大于 64 位的拼接分支，但该接口对 `__int128` 是否打算正式支持，需要确认。

## 4. 切片与 bit 访问

### 4.1 公开接口

```cpp
constexpr IntRangeProxy<BitWidth> operator()(uint32_t hi, uint32_t lo);
constexpr IntConstRangeProxy<BitWidth> operator()(uint32_t hi, uint32_t lo) const;

constexpr IntBitProxy<BitWidth> operator()(uint32_t bit);
constexpr IntConstBitProxy<BitWidth> operator()(uint32_t bit) const;

template <uint32_t DstBitWidth>
constexpr IntRangeProxy<BitWidth> range_at(Int<IDX_WIDTH> idx);

template <uint32_t DstBitWidth>
constexpr IntConstRangeProxy<BitWidth> range_at(Int<IDX_WIDTH> idx) const;

constexpr IntBitProxy<BitWidth> bit_at(Int<IDX_WIDTH> idx);
constexpr IntConstBitProxy<BitWidth> bit_at(Int<IDX_WIDTH> idx) const;
```

### 4.2 规则

- `operator()(hi, lo)`：
  - 要求 `hi >= lo`
  - 要求 `hi < BitWidth`
- `operator()(bit)`：
  - 要求 `bit < BitWidth`
- `range_at<DstBitWidth>(idx)`：
  - 实际返回 `[idx + DstBitWidth - 1 : idx]`
  - 是否越界由代理构造时的 `assert` 检查
- `bit_at(idx)`：
  - 实际返回第 `idx` 位

### 4.3 代理对象赋值语义

- `IntRangeProxy` 支持从：
  - `Int<SrcBitWidth>`
  - 原生整型
  - `IntRangeProxy`
  - `IntConstRangeProxy`
  赋值。
- 当源和目标来自同一个 `Int` 对象并且区间可能重叠时，实现会先创建快照，再赋值。
- 因此重叠范围赋值不会出现“边写边读破坏源值”的问题。

### 4.4 截断规则

- 目标区间宽度若小于源宽度，则仅复制源低位部分。
- 源区间宽度若小于目标区间宽度，则只覆盖低位，其余高位被清零后保持 0。

## 5. 基础逻辑操作

### 5.1 公开接口

```cpp
constexpr bool reduce_or() const;
constexpr bool reduce_and() const;
constexpr bool reduce_xor() const;

template <uint32_t K>
constexpr Int<BitWidth * K> repeat() const;

template <uint32_t OtherBitWidth>
constexpr Int<BitWidth + OtherBitWidth> cat(const Int<OtherBitWidth>& other) const;

template <uint32_t FirstBitWidth, uint32_t... RestBitWidths>
constexpr auto Cat(const Int<FirstBitWidth>& first, const Int<RestBitWidths>&... rest);
```

### 5.2 规则

- `reduce_or()`：任意位为 1 则返回 `true`。
- `reduce_and()`：所有有效位均为 1 才返回 `true`。
- `reduce_xor()`：返回有效位 1 的个数奇偶校验。
- `repeat<K>()`：
  - 把当前 bit 模式重复 `K` 次
  - 返回宽度为 `BitWidth * K`
- `cat(other)`：
  - 当前对象作为高位
  - `other` 作为低位
- `Cat(a, b, c, ...)`：
  - 保持参数书写顺序，高参数在高位，后参数在低位

## 6. 算术运算

### 6.1 加法

```cpp
Int<A> + Int<B>  -> Int<max(A, B) + 1>
Int<W> + T       -> Int<W + 1>
T + Int<W>       -> Int<W + 1>
```

规则：

- 按无符号位向量做加法。
- `Int + signed T` 时，`T` 会先以 `W` 位 two's complement 形式参与运算。

### 6.2 减法

```cpp
Int<A> - Int<B> -> Int<max(A, B)>
Int<W> - T      -> Int<W>
T - Int<W>      -> Int<W>
-Int<W>         -> Int<W>
```

规则：

- 结果位宽不额外扩一位。
- 按固定位宽 two's complement 语义截断。

### 6.3 乘法

```cpp
Int<A> * Int<B>         -> Int<A + B>
IntSignedView<A> * ...  -> Int<A + B>
... * IntSignedView<B>  -> Int<A + B>
```

规则：

- 只要任一操作数是 `IntSignedView`，该操作数按有符号数解释。
- 两个普通 `Int` 相乘按无符号解释。
- 返回位宽恒为两侧位宽之和。

## 7. 位运算

### 7.1 支持的运算

```cpp
operator&
operator|
operator^
operator~
```

### 7.2 规则

- 对等宽 `Int` 的位运算结果仍是相同位宽。
- 代理对象参与位运算时，会先被转换成目标 `Int<BitWidth>`，然后再做运算。
- 不存在自动“取两者较宽位宽”的位运算重载；使用代理时结果位宽通常由另一侧 `Int` 决定。

## 8. 比较运算

### 8.1 支持的运算

```cpp
== != < <= > >=
```

支持以下组合：

- `Int` 与 `Int`
- `Int` 与 `IntSignedView`
- `IntSignedView` 与 `IntSignedView`
- 上述任一类型与原生整型

### 8.2 规则

- 若操作数包含 `IntSignedView`，则该操作数按有符号扩展比较。
- 与原生整型比较时：
  - 若原生整型是有符号类型，则先构造成同位宽 `Int`，再转成 `.sint()` 参与比较。
  - 若原生整型是无符号类型，则按普通 `Int` 参与比较。

## 9. 移位运算

### 9.1 支持的运算

```cpp
Int<W> << Int<S>
Int<W> >> Int<S>
IntSignedView<W> >> Int<S>

Int<W> << integral
Int<W> >> integral
IntSignedView<W> >> integral
```

### 9.2 规则

- 左移与逻辑右移结果位宽保持 `W` 不变。
- 算术右移只对 `IntSignedView` 版本定义。
- 若移位量大于等于 `W`：
  - 左移结果全 0
  - 逻辑右移结果全 0
  - 算术右移结果为：
    - 原值非负则全 0
    - 原值为负则全 1（再按位宽 mask）
- 对原生有符号移位量参数，代码中使用 `assert(rhs >= 0)`；负移位量在 debug 下会触发断言，release 下不应依赖其行为。

## 10. 默认值与有效位

- 默认构造 `Int()` 为全 0。
- 所有公开构造和大多数运算在结尾都会调用 `mask_high_bits()`，保证无效高位为 0。
- 因此比较、切片、repeat、cat、移位等操作默认按“有效位有限、无效高位清零”理解。

## 11. 待确认项

- `sint()` 只是视图，不会改变底层 bit 模式；若需求方把它理解成“转换成更宽的 signed 类型”，需要特别澄清。
- 与原生 signed 整数混合比较/运算时，采用的是“先截断到目标位宽，再按 two's complement 解释”的规则，需要确认这正是团队想要的硬件语义。
- 代理对象参与位运算时的结果位宽不是自动取最大宽度，而由重载形式决定；这点需要在评审中确认是否足够直观。
