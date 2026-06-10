# module.config.set - 在模块中设置本地配置项

## 概述

`module.config.set` 命令用于在模块的本地配置中添加、设置、重命名或删除本地配置项。

- 添加：提供 `newname` 和 `value`/`comment` 参数，不提供 `oldname`。
- 修改：提供 `oldname` 和 `value`/`comment` 参数。允许同时进行修改和重命名。
- 重命名：提供 `oldname` 和 `newname` 参数。允许同时进行修改和重命名。
- 删除：仅提供 `oldname` 参数。

## 参数

- name — 要操作的模块名称，字符串类型。
- oldname — （可选）要修改的本地配置项旧名称，字符串类型。
- newname — （可选）新的本地配置项名称，字符串类型。
- value — （可选）新的值或表达式，字符串类型。
- comment — （可选）注释，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModConfNotFound、EOPModConfNameInvalid、EOPModConfNameDup、EOPModConfValueInvalid。