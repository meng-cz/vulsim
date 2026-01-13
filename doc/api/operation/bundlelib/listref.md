# bundlelib.listref - 递归列出 bundle 的引用关系

## 概述

`bundlelib.listref` 命令用于递归列出某个 bundle 的全部引用（或反向引用）。

## 参数

- name — 要列出引用的 bundle 名称，字符串类型。
- reverse — （可选）设为 "true" 时列出反向引用，布尔/字符串类型。

## 结果

- names: 列表中包含的 bundle 名称（按遍历顺序）。
- childs: 对应每个名字的引用（或反向引用）列表，项间以空格分隔。

## 错误码

可能的错误包括：EOPBundListRefMissArg、EOPBundListRefNameNotFound。
