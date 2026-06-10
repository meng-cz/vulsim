# module.bundle.set - 在模块中添加/修改/重命名/删除本地 bundle

## 概述

`module.bundle.set` 命令用于在模块中添加、设置、重命名或删除本地 bundle，会验证定义与引用并检测循环依赖。

- 添加：提供 `newname` 和 `definition` 参数，不提供 `oldname`。
- 修改：提供 `oldname` 和 `definition` 参数。允许同时进行修改和重命名。
- 重命名：提供 `oldname` 和 `newname` 参数。允许同时进行修改和重命名。
- 删除：仅提供 `oldname` 参数。

## 参数

- name — 要操作的模块名称，字符串类型。
- oldname — （可选）要修改的本地 bundle 旧名称，字符串类型。
- newname — （可选）新的本地 bundle 名称，字符串类型。
- definition — （可选）bundle 的 JSON 定义，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModBundNotFound、EOPModBundNameInvalid、EOPModBundNameDup、EOPModBundDefinitionInvalid、EOPModBundRefLoop。