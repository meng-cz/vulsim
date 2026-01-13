# configlib.update - 更新配置项的值

## 概述

`configlib.update` 命令用于更新已存在配置项的值，会校验新表达式的合法性、引用和循环依赖。

## 参数

- name — 要更新的配置项名称，字符串类型。
- value — 新的配置项值或表达式，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPConfUpdateMissArg、EOPConfUpdateNameNotFound、EOPConfUpdateValueInvalid、EOPConfUpdateRefLoop。