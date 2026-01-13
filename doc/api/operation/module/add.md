# module.add - 向项目模块库添加新模块

## 概述

`module.add` 命令用于向项目的模块库添加一个新的空模块。

## 参数

- name — 要添加的模块名称，字符串类型。
- comment — （可选）模块注释，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModAddMissArg、EOPModAddNameInvalid、EOPModAddNameConflict。