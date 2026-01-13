# bundlelib.rename - 重命名项目的 bundle

## 概述

`bundlelib.rename` 命令用于重命名项目 bundle 库中的一个已存在的 bundle。可选择自动更新所有引用到该 bundle 的地方。

## 参数

- old_name — 要重命名的当前 bundle 名称，字符串类型。
- new_name — 新的 bundle 名称，字符串类型。
- update — （可选）设置为 "true" 时自动更新所有引用，布尔/字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPBundRenameMissArg、EOPBundRenameNameNotFound、EOPBundRenameImport、EOPBundRenameReferenced、EOPBundRenameNameConflict。
