# bundlelib.list - 列出项目的 bundle 列表

## 概述

`bundlelib.list` 命令用于列出项目 bundle 库中的条目，可根据名称子串、tag、精确匹配等进行过滤，并可选择包含引用或 JSON 定义。

## 参数

- name — （可选）名称子串过滤，字符串类型。
- tag — （可选）按 tag 过滤，字符串类型。
- exact — （可选）设为 "true" 则执行精确匹配，布尔/字符串类型。
- reference — （可选）设为 "true" 则在输出中包含引用和反向引用，布尔/字符串类型。
- definition — （可选）设为 "true" 则在输出中包含 JSON 定义，布尔/字符串类型。

## 结果

- names: bundle 名称列表。
- comments: 每个 bundle 的注释列表。
- tags: 每个 bundle 的 tag 列表（以空格分隔）。
- definitions: 每个 bundle 的 JSON 定义（仅当 `definition` 为 true 时）。
- references: 每个 bundle 的引用列表（以空格分隔，仅当 `reference` 为 true 时）。
- config_references: 每个 bundle 的配置引用列表（以空格分隔，仅当 `reference` 为 true 时）。
- reverse_references: 每个 bundle 的反向引用列表（以空格分隔，仅当 `reference` 为 true 时）。

## 错误码

无特定错误码。
