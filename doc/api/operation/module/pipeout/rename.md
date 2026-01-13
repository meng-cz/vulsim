# module.pipeout.rename - 重命名模块的管道输出端口

## 概述

`module.pipeout.rename` 命令用于重命名指定模块的管道输出端口。

## 参数

- name — 模块名称，字符串类型。
- old_port — 要重命名的管道输出端口的当前名称，字符串类型。
- new_port — 管道输出端口的新名称，字符串类型。
- update_connections — （可选）是否更新与该管道输出端口相关的连接（true/false），默认 true。

## 结果

无。

## 错误码

可能的错误包括：EOPModCommonNotFound、EOPModCommonImport、EOPModCommonMissArg、EOPModPipePortRenameNotFound、EOPModPipePortRenameNameInvalid、EOPModPipePortRenameNameDup

