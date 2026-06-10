# bundlelib.add - 向项目的 bundle 库添加新的 bundle

## 概述

`bundlelib.add` 命令用于向当前项目的 bundle 库中添加一个新的 bundle，可以只提供名称和注释，也可以提供完整的 JSON 定义。

## 参数

- name — 要添加的 bundle 名称，字符串类型。
- comment — （可选）bundle 的注释，字符串类型。
- definition — （可选）bundle 的 JSON 定义，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPBundAddMissArg、EOPBundAddNameInvalid、EOPBundAddNameConflict、EOPBundAddDefinitionInvalid。
