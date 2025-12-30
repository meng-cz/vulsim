# 泺源处理器性能模拟平台

泺源（LuoYuan）处理器性能模拟平台，是一套用于处理器微架构建模、探索与调试的软件平台。基于新型微架构描述模型：可视化统一中间层模型（Visualized Unified Layer，VUL）构建。

基于VUL模型，泺源平台为新型微架构设计者提供了：

1. 统一化的、结构化的通用微架构设计层描述
2. 周期精确的并行模拟，周期行为对齐到最终的RTL实现
3. 内置调试工具和性能统计工具
4. 自动化生成RTL代码框架

## 开发状态

Working in progress.

原型版本v0.1代码已签出到分支legacy_0_0_1_demo，实现基于TUI的单模块Vul模型设计和RTL对齐模拟器代码生成。

v1.0开发中：

1. 重构配置参数库和结构体库 （2025/11完成）
2. 为Vul模型支持模块化设计和预制件导入 （正在进行）
3. 构建VulDesigner GUI v1.0 （预计2025/02）
4. 实现Vul模型到RTL框架的代码自动生成（预计2025/03）
5. 研究基于HLS的从Vul模型到可综合RTL的生成（预计2025/04）
6. 构建VulDesigner v1.1，添加对波形调试、性能统计、快照保存和恢复的支持（预计2025/04）

前期工作：
1. [nullrvsim](https://github.com/meng-cz/nullrvsim): 验证VUL模型生成的模拟器框架的正确性和并行效率
2. [FASE](https://github.com/meng-cz/fase-rv64)：对RTL实现的处理器实现系统调用模拟（SE模式），为VUL模型上的SE模式做准备

## 文档

v1.0版本的Vul模型Module文档：[module.md](./doc/module.md) 

v0.1版本的Vul模型文档：[vularch.md](https://github.com/meng-cz/vulsim/blob/legacy_0_0_1_demo/doc/vularch.md)

TODO: user guide.

## 构建

概述：基于CMake+Qt6构建

TODO：详细构建方式

## “泺源”简介

“泺源”之名，取自济南趵突泉——古泺水的发源地，它是泉城的文化根脉，承载着“源头活水”的含义。

“泺”作为自然源头，代表着创新的起点，就像趵突泉为古泺水注入不竭动能，平台也为处理器研发提供源头性的性能模拟支撑；“源”也代表着处理器设计的源头，强调技术的源头开放、共享协作。“泺源”二字彰显了平台以源头创新驱动芯片技术突破的愿景，成为处理器研发领域的核心引擎。

## 致谢

### 引用开源项目：

- pugixml: [https://github.com/zeux/pugixml](https://github.com/zeux/pugixml)
- json : [https://github.com/nlohmann/json](https://github.com/nlohmann/json)

