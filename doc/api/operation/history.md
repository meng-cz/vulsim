# history - 获取操作历史，撤销和重做列表

## 概述

history命令用于获取当前项目的操作历史记录，包括可撤销操作列表和可重做操作列表。

## 参数

无

## 结果

无

## 列表结果

- undolist_names — 可撤销操作名称列表，按时间顺序排列，最新的操作在最后。
- undolist_timestamps — 可撤销操作时间戳列表，与undolist_names一一对应。
- undolist_args — 可撤销操作参数字符串，与undolist_names一一对应。
- redolist_names — 可重做操作名称列表，按时间顺序排列，最新的操作在最后。
- redolist_timestamps — 可重做操作时间戳列表，与redolist_names一一对应。
- redolist_args — 可重做操作参数字符串，与redolist_names一一对应。

## 错误码

无

