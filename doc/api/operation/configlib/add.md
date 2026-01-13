# configlib.add - 向项目的配置库添加新的配置项

## 概述

`configlib.add` 命令用于向当前项目的配置库中添加一个新的配置项。添加时会校验名称是否合法并计算表达式的实际值与引用关系。

## 参数

- name — 要添加的配置项名称，字符串类型。
- value — 配置项的表达式或初始值，字符串类型。
- comment — （可选）配置项注释，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPConfAddMissArg、EOPConfAddNameInvalid、EOPConfAddNameConflict、EOPConfAddValueInvalid、EOPConfAddRefNotFound。
