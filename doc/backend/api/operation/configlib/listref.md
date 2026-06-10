# configlib.listref - 递归列出配置项的引用关系

## 概述

`configlib.listref` 命令用于递归列出某个配置项的全部引用或反向引用，并返回每个项的原始值与计算后的实值。

## 参数

- name — 要列出引用的配置项名称，字符串类型。
- reverse — （可选）设为 "true" 时列出反向引用，布尔/字符串类型。

## 结果

- names: 列表中包含的配置项名称（按遍历顺序）。
- childs: 对应每个名字的引用（或反向引用）列表，项间以空格分隔。
- values: 每个配置项的原始表达式/值。
- realvalues: 每个配置项的计算后的实值（数值字符串）。

## 错误码

可能的错误包括：EOPConfListRefMissArg、EOPConfListRefNameNotFound。