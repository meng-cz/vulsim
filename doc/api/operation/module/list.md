# module.list - 列出模块库中的模块

## 概述

`module.list` 命令用于列出项目模块库中的模块，可按名称、导入/本地、是否精确匹配等筛选，并可选择列出子实例。

## 参数

- name — （可选）名称子串过滤，字符串类型。
- exact — （可选）设为 "true" 则进行精确匹配，布尔/字符串类型。
- imported_only — （可选）设为 "true" 则仅列出导入模块，布尔/字符串类型。
- local_only — （可选）设为 "true" 则仅列出本地模块，布尔/字符串类型。
- list_children — （可选）设为 "true" 则为每个模块额外列出其直接子实例，布尔/字符串类型。

## 结果

- names: 匹配的模块名称列表。
- sources: 对应模块的来源（"imported" 或 "local"）。
- children: 对应每个模块的直接子实例列表（以空格分隔，仅当 `list_children` 为 true 时）。

## 错误码

无特定错误码。
