# code.getsymbol - 生成模块辅助 C++ 代码的符号表信息

## 概述

为项目中指定的模块生成辅助 C++ 代码的符号表信息。可选择只返回符号表，或生成完整的帮助代码。

## 参数

- `[0] module_name` — 要生成代码的模块名（字符串），必需。
- `[5] full_helper` — （可选）若设置为 `true` 则生成完整的帮助代码（包含头文件），否则仅返回符号表。

## 结果

- `symbols` — 模块中有效符号的 JSON 字符串表示。

## 列表结果

- `helper_code_lines` — （如适用）完整帮助代码行列表。

## 错误码

- `EOPCodeGetInvalidArg` — 参数无效或缺失。
- `EOPModCommonNotFound` — 指定模块未找到。
- `EOPModCommonImport` — 指定模块为导入模块，无法修改或生成。
- `EOPCodeGetNotFound` — 指定的代码块/服务/请求未找到（按上下文返回）。
