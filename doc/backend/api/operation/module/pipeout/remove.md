# module.pipeout.remove - 移除模块的管道输出端口

## 概述

`module.pipeout.remove` 命令用于移除指定模块的管道输出端口。

## 参数

- name — 模块名称，字符串类型。
- port — 要移除的管道输出端口的名称，字符串类型。
- force — （可选）是否强制移除已连接的管道输出端口（true/false），默认 false。

## 结果

无。

## 错误码

可能的错误包括：EOPModCommonNotFound、EOPModCommonImport、EOPModCommonMissArg
