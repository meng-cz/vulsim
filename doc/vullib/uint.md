# `vullib/fixint.hpp` 开发文档

本文档描述当前 `Int<BitWidth>` 实现语义。历史上的 `vullib/uint.hpp` 已被 `vullib/fixint.hpp` 取代；代码应包含 `fixint.hpp` 或通过 `vullib.h` 间接使用。

## 1. 核心类型

```cpp
template <uint32_t BitWidth> class Int;
template <uint32_t ParentBitWidth, uint32_t SliceBitWidth> class IntSliceRef;
template <uint32_t ParentBitWidth, uint32_t SliceBitWidth> class IntConstSliceRef;
template <uint32_t ParentBitWidth> class IntBitRef;
template <uint32_t ParentBitWidth> class IntConstBitRef;
template <typename Operand> class IntSignedView;
template <typename Operand> struct IntOperandTraits;
```

`IntOperandTraits` 是运算符统一推导入口，用于取得：
- 操作数是否是 fixint operand。
- 操作数编译期位宽。
- 操作数是否按 signed view 解释。
- 操作数运行时 `Int<W>` 值。

## 2. 数据模型

`Int<BitWidth>` 满足：
- `BitWidth > 0`。
- `WORD_BITS == 64`。
- `NUM_WORDS == (BitWidth + 63) / 64`。
- `IDX_WIDTH == log2ceil(BitWidth)`。
- 底层 word 小端排列：`data[0]` 保存最低 64 位。

`data` 是私有成员。需要访问位内容应使用公开 API：
- `to<T>()`
- `at<Hi, Lo>()`
- `at<Idx>()`
- `pick<DstWidth>(idx)`
- `pick(idx)`
- `Cat`/`Repeat`/运算符

当前大量自由函数作为 `Int` 友元访问 raw data，以便实现高效运算；调用方不应依赖 raw data 布局作为公开 ABI。

## 3. 构造、赋值与视图转换

公开构造：

```cpp
constexpr Int();
constexpr Int(bool value);

template <typename T> requires std::is_integral_v<T>
constexpr Int(T value);

template <uint32_t OtherBitWidth>
constexpr Int(const Int<OtherBitWidth>& other);

template <typename Operand>
constexpr Int(const Operand& operand); // Int/Ref/SignView operand
```

赋值规则：
- `Int = 标准整数`：按整数 signedness 选择符号扩展或截断。
- `Int = Int<OtherWidth>`：按无符号位向量转换。
- `Int = Ref`：通过 Ref 的 `Int<W>` 值转换。
- `Int = SignView`：按源符号位扩展或截断。

`to<T>()` 支持导出为标准整数类型，按目标类型 signedness 返回低位并在需要时做符号扩展。

`sint()` 返回 `IntSignedView<...>`。signed view 不持有新数据，只改变后续构造、赋值、比较、乘法和右移中对 bit pattern 的解释方式。

## 4. Slice 与 Bit Ref

静态 API：

```cpp
template <uint32_t Hi, uint32_t Lo>
constexpr auto at();

template <uint32_t Idx>
constexpr auto at();
```

动态 API：

```cpp
template <uint32_t DstBitWidth, typename IdxT>
constexpr auto pick(IdxT idx);

template <typename IdxT>
constexpr auto pick(IdxT idx);
```

`const Int` 返回 const Ref。

Slice 赋值：
- 接受同宽 `Int/Ref/SignView`。
- 接受标准整数。
- 不接受不同宽 `Int/Ref` 的隐式赋值；调用方应显式构造目标宽度。

Bit 赋值：
- 接受 `bool`、标准整数、`Int<1>`、其他 Bit/SignView。
- 不接受宽度不为 1 的 operand。

同一 `Int` 内的 Slice/Bit 互相赋值会先经 `Int<DstWidth>` 中转，避免重叠区间边读边写破坏源值。

## 5. 归约、重复与拼接

```cpp
template <typename Operand> constexpr bool ReduceAnd(const Operand& operand);
template <typename Operand> constexpr bool ReduceOr(const Operand& operand);
template <typename Operand> constexpr bool ReduceXor(const Operand& operand);

template <uint32_t N, typename Operand>
constexpr auto Repeat(const Operand& operand);

template <typename... Operands>
constexpr auto Cat(const Operands&... operands);
```

规则：
- 这些函数仅接受无符号 `Int/Ref` operand。
- signed view 被拒绝。
- `Repeat<N>` 要求 `N > 0`，返回 `Int<width(operand) * N>`。
- `Cat(a, b, c)` 中 `a` 位于最高位，`c` 位于最低位，返回总宽度之和的 `Int`。

## 6. 算术运算

加法：

```cpp
Int<A> + Int<B> -> Int<max(A, B) + 1>
Int<W> + integral -> Int<W + 1>
integral + Int<W> -> Int<W + 1>
```

只接受无符号 `Int/Ref`。标准整数会先构造成另一侧同宽 `Int`，再复用主重载。

减法：

```cpp
Int<A> - Int<B> -> Int<max(A, B)>
Int<W> - integral -> Int<W>
integral - Int<W> -> Int<W>
```

只接受无符号 `Int/Ref`，不额外保存借位。

取负：

```cpp
-operand -> Int<width(operand)>
```

接受无符号 `Int/Ref` 和 signed view，按补码规则取负。

乘法：

```cpp
lhs * rhs -> Int<width(lhs) + width(rhs)>
```

接受无符号 `Int/Ref` 和 signed view。任一操作数是 signed view 时，该操作数按有符号补码解释。

## 7. 位运算

```cpp
operator&
operator|
operator^
operator~
operator<<
operator>>
```

规则：
- `& | ^` 要求两侧都是等宽无符号 `Int/Ref`，结果同宽。
- `~` 接受一个无符号 `Int/Ref`，结果同宽。
- `& | ^` 与标准整数混合时，整数先构造成另一侧同宽 `Int`。
- `<<` 左侧接受无符号 `Int/Ref`，右侧接受无符号 `Int/Ref` 或标准整数，结果宽度等于左侧宽度。
- `>>` 左侧接受 `Int/Ref/SignView`，其中 unsigned 视图执行逻辑右移，signed view 执行算术右移。右侧接受无符号 `Int/Ref` 或标准整数。
- signed view 不接受 `& | ^ ~ <<`，也不能作为移位量。
- signed 标准整数作为移位量时，负数会触发 `assert`；移位量大于或等于左侧宽度时返回全 0。

## 8. 比较运算

等值比较：
- `== !=` 只接受等宽无符号 `Int/Ref`。
- 与标准整数混合时，仅接受 unsigned 标准整数。
- 不接受 signed view。

数值比较：
- `< <= > >=` 接受等宽 `Int/Ref/SignView`。
- 每个操作数可独立指定是否 signed view。
- 与 signed 标准整数比较时，整数会构造成同宽 `Int` 并以 `.sint()` 参与比较。
- 与 unsigned 标准整数比较时，整数按无符号视图参与比较。

## 9. 当前未实现的旧 API

旧 `uint.hpp` 中以下 API 不属于当前 `fixint.hpp`：
- `operator()(hi, lo)` / `operator()(bit)`：改用 `at<Hi, Lo>()` / `at<Idx>()`。
- `range_at` / `bit_at`：改用 `pick<DstWidth>(idx)` / `pick(idx)`。
- 成员 `reduce_*`：改用 `ReduceAnd/ReduceOr/ReduceXor`。
- 成员 `repeat<K>()` / `cat(other)`：改用 `Repeat<K>(x)` / `Cat(a, b, ...)`。

## 10. 维护注意事项

- 运算符尽量通过 `IntOperandTraits` 接入，避免为每种 Ref 单独写重载。
- 不同宽度的 Slice 赋值应保持显式转换要求，防止静默截断。
- 涉及跨 64 位边界的逻辑必须覆盖 `BitWidth == 64`、`BitWidth % 64 == 0`、`BitWidth % 64 != 0` 三类路径。
