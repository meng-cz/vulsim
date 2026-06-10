# configlib.rename - 重命名项目的配置项

## 概述

`configlib.rename` 命令用于重命名配置库中的一个已存在配置项。可选择自动更新对该配置项的所有引用。

## 参数

- old_name — 当前配置项名称，字符串类型。
- new_name — 新的配置项名称，字符串类型。
- update — （可选）设置为 "true" 时自动更新所有引用，布尔/字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPConfRenameMissArg、EOPConfRenameNameNotFound、EOPConfRenameNameInvalid、EOPConfRenameNameConflict、EOPConfRenameImport、EOPConfRenameReferenced。