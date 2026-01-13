# module.pipe.set - 在模块中添加/修改/重命名/删除 Pipe 实例

## 概述

`module.pipe.set` 命令用于在模块中添加、设置、重命名或删除 pipe 实例，支持通过 JSON 定义更新 pipe 属性。

## 参数

- name — 要操作的模块名称，字符串类型。
- oldname — （可选）要修改的 pipe 实例旧名称，字符串类型。
- newname — （可选）新的 pipe 实例名称，字符串类型。
- definition — （可选）pipe 的 JSON 定义，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModPipeNotFound、EOPModPipeNameInvalid、EOPModPipeNameDup。
