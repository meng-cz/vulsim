# configlib.list - 列出项目的配置项

## 概述

`configlib.list` 命令用于列出项目配置库中的配置项，可按名称子串、组别或精确匹配过滤，并可选择包含引用信息或原始定义。

## 参数

- name — （可选）名称子串过滤，字符串类型。
- group — （可选）按组过滤，字符串类型。
- exact — （可选）设为 "true" 则执行精确匹配，布尔/字符串类型。
- reference — （可选）设为 "true" 则在输出中包含引用和反向引用，布尔/字符串类型。

## 结果

- names: 配置项名称列表。
- groups: 每个配置项所属组。
- realvalues: 每个配置项计算后的实值（数值字符串）。
- comments: 每个配置项的注释。
- values: 每个配置项的原始表达式/值。
- references: 每个配置项的引用列表（以空格分隔，仅当 `reference` 为 true 时）。
- reverse_references: 每个配置项被引用的项列表（以空格分隔，仅当 `reference` 为 true 时）。

## 错误码

无特定错误码。