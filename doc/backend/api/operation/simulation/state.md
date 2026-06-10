# simulation.state - 查询仿真任务状态

## 概述

获取当前项目的仿真任务状态信息，包括当前运行的任务 ID、当前阶段以及各阶段的开始/结束时间戳等。

## 参数

- 无

## 结果

- `runid` — 当前正在运行的任务 ID，若无运行任务则为空字符串。
- `stage` — 当前仿真阶段，取值可能为 `generation`、`compilation`、`simulation` 或 `idle`（如在运行则会带进度信息）。
- `errored` — 是否发生错误，True/False。
- `gen_start_us` — 生成开始时间（微秒，自纪元）。
- `gen_finish_us` — 生成完成时间（微秒，自纪元）。
- `comp_start_us` — 编译开始时间（微秒，自纪元）。
- `comp_finish_us` — 编译完成时间（微秒，自纪元）。
- `sim_start_us` — 仿真开始时间（微秒，自纪元）。
- `sim_finish_us` — 仿真完成时间（微秒，自纪元）。

## 错误码

- 无（若无运行任务则返回默认空/0 值）。
