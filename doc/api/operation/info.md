# info - 获取当前打开的项目信息

## 概述

info命令用于获取当前打开的项目信息，包括项目名称、路径、修改状态。

## 参数

无

## 结果

- name — 项目名称，字符串类型。
- dirpath — 项目路径，字符串类型。
- top_module — 顶层模块名称，字符串类型。
- is_modified — 项目元数据是否被修改，布尔类型，true表示已修改，false表示未修改。
- is_config_modified — 全局配置库是否被修改，布尔类型，true表示已修改，false表示未修改。
- is_bundle_modified — 全局线组库是否被修改，布尔类型，true表示已修改，false表示未修改。

## 列表结果

- modified_modules — 被修改的模块名称列表，字符串数组类型。
- import_names — 已导入的外部项目名称列表，字符串数组类型.
- import_paths — 已导入的外部项目路径列表，字符串数组类型，与import_names一一对应。

## 错误码

- EOPInfoNotOpened — 当前没有打开的项目，无法获取项目信息。

