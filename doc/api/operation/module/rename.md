# module.rename - 重命名模块

## 概述

`module.rename` 命令用于在项目的模块库中重命名一个模块，可选择是否更新其他模块中对该模块的实例引用。

## 参数

- old_name — 当前模块名称，字符串类型。
- new_name — 新的模块名称，字符串类型。
- update_references — （可选）是否更新其他模块中的引用（true/false），默认 true。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModRenameMissArg、EOPModRenameNameNotFound、EOPModRenameNameInvalid、EOPModRenameNameConflict、EOPModRenameImport、EOPModRenameReferenced。