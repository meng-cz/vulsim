# module.remove - 从项目模块库删除模块

## 概述

`module.remove` 命令用于从项目模块库中删除一个已存在且为空的模块（非导入模块且无内容）。

## 参数

- name — 要删除的模块名称，字符串类型。

## 结果

无（成功返回空结果或成功消息）。

## 错误码

可能的错误包括：EOPModRemoveMissArg、EOPModRemoveNameNotFound、EOPModRemoveImport、EOPModRemoveNotEmpty。