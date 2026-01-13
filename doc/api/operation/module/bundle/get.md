# module.bundle.get - 获取模块本地 bundle 定义

## 概述

`module.bundle.get` 命令用于从模块中获取指定本地 bundle 的 JSON 定义。

## 参数

- name — 要获取 bundle 的模块名称，字符串类型。
- bundle — 要获取的本地 bundle 名称，字符串类型。

## 结果

- definition: bundle 的 JSON 定义，字符串类型。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModBundNotFound。