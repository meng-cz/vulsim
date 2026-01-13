# module.instance.get - 获取模块实例信息

## 概述

`module.instance.get` 命令用于获取指定模块实例的信息。

## 参数

- name — 要获取 instance 的模块名称，字符串类型。
- instance — 要获取的本地 instance 名称，字符串类型。

## 结果

- definition: instance 的 JSON 定义，字符串类型。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModInstNotFound。

