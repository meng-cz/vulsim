# module.pipe.get - 获取模块中的 pipe 定义

## 概述

`module.pipe.get` 命令用于从模块中获取指定 pipe 实例的定义（以 JSON 格式返回）。

## 参数

- name — 要获取 pipe 的模块名称，字符串类型。
- pipe — 要获取的 pipe 实例名称，字符串类型。

## 结果

- definition: pipe 的 JSON 定义，字符串类型。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModInstanceNotFound。