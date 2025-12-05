# VulModule 模块类说明

本文档说明 `VulModule`（在代码中定义于 `src/module.hpp`）的数据结构与使用约定。`VulModule` 描述一个模块（Module）的静态定义、其实例、连接关系、存储器以及用于代码生成的元信息。文档风格参考项目中其他设计说明（例如 `configlib.md`、`bundlelib.md`）。

## 命名与约定

- **模块名**：由 `ModuleName` 表示（字符串），在工程中应当以有意义的名称命名。自动生成的模块（例如由 `getModuleDefinitionForPipe` 生成）会以管线名加后缀 `_module` 命名。
- **注释**：每个模块有 `comment` 字段用于人类可读说明。
- **特殊实例**：模块中使用常量 `TopInterface`（值为 `"__top__"`）表示顶层接口（TopInterface）。

## 模块外部端口

一个模块的外形由其请求（Requests）、服务（Services）、管线输入/输出（Pipe I/O）端口定义组成：

- **请求（Requests）**：模块内部主动调用的接口，定义了模块对外部服务的需求。请求通过名称、参数和返回值进行描述。
- **服务（Services）**：模块对外提供的接口，定义了模块能够响应的请求。服务同样通过名称、参数和返回值进行描述。
- **管线输入/输出（Pipe I/O）**：模块与管线（Pipe）之间的数据接口，定义了模块接收和发送数据的端口。

除了时钟、复位、调试组件等自动生成的接口外，模块仅对外暴露出这些接口，模块内部的实现细节（如存储器、实例等）对外部不可见。

### 请求与服务（Requests / Services）：
  - `requests`：以 `RequestName` 为键的映射，值为 `VulRequest`。请求包含名字、参数列表（`VulArg`）、返回值列表（`VulRet`）、可选的 `array_size`（表示数组请求）以及 `has_handshake` 标志。
  - `services`：以 `ServiceName` 为键的映射，值为 `VulService`，字段与 `VulRequest` 对称，表示模块提供的服务。
  - 数组请求/服务：当 `array_size` 非空时，表示该请求/服务为数组形式（有额外的index参数）。

数组形式的请求仅能连接到同样为数组形式的服务，且数组大小必须匹配。可以通过定义数组请求/服务适配器（Array Request/Service Adapter）将数组请求/服务展开并连接到不同的非数组请求/服务。

### 管线输入/输出（Pipe I/O）：
  - `pipe_inputs` / `pipe_outputs`：分别以 `PipeInputName` / `PipeOutputName` 为键，值为 `VulPipeInput` / `VulPipeOutput`，描述模块的管线端口名称与数据类型.

## 模块内部数据结构

模块可以包含其他模块的子实例（子模块），形成层次化的设计。每个子模块实例通过 `instances` 字段进行定义，指定实例名称、所使用的模块名称以及可选的注释。模块内部还可以包含管线子实例（`pipe_instances`）。

一个模块内部的结构由以下主要部分组成：

### 存储（Storages）：
  - `storages`：模块运行状态的持久存储变量（类型 `VulStorage`），包含 `name`、`type`、可选的 `value`、注释和维度 `dims`（数组时）。
  - `storagenexts`：下一时钟周期保存值（`next` 版本），生成时通常用于并行赋值策略。
  - `storagetmp`：运行时临时变量，tick 结束后会被重置。

### 实例与管线实例（Instances / PipeInstances）
  - `instances`：模块内部子模块实例，键为 `InstanceName`，值为 `VulInstance`（包含实例名、模块名、注释）。
  - `pipe_instances`：模块内部的管线实例（`VulPipe`），用于连接模块与管线。

### 连接关系（Connections）
  - `req_connections`：记录请求-服务（request-service）连接信息，键为实例名，值为 `VulReqServConnection`（包含请求方实例、请求名、服务方实例与服务名）。
  - `mod_pipe_connections`：模块与管线的连接（`VulModulePipeConnection`），定义方向与端口映射。
  - `stalled_connections`：模块内部的停滞（stall）连接，用于表示数据流或控制流的阻塞关系（`VulStallConnection` 列表）。
  - `update_constraints`：模块更新顺序约束（`VulSequenceConnection` 列表），用于生成代码时保证更新顺序正确。

### 代码片段

- `user_tick_codelines`：用户为模块 `tick` 函数编写的 C 代码行。
- `user_applytick_codelines`：用户为 `applyTick` 函数提供的代码行。
- `user_header_filed_codelines`：用户在头文件中声明的字段代码行（通常放在私有部分）。
- `serv_codelines` / `req_codelines`：未连接服务或请求对应的函数体代码行（以 `ServiceName` 或 `(InstanceName, RequestName)` 为键）。

这些代码片段被代码生成器原样嵌入到生成的 `.hpp` / `.cpp`（或内联实现）中。请确保所写代码与生成环境（包含头文件、命名空间）兼容。

### 代码生成相关标志

- `is_hpp_generated`：指示是否生成 `.hpp` 头文件（当前默认 `true`）。
- `is_inline_generated`：指示是否生成内联实现（当前默认 `true`）。
- `_is_external`（继承自 `VulModuleBase`）：标记模块是否为外部定义（例如自动生成的 pipe-module），外部模块的代码通常不由项目直接生成或修改。

## 连接语义与一致性要求

本章节讨论模块内部对于`本模块外部端口定义的内侧`和`子模块外部端口定义的外侧`之间的连接关系的语义要求。相应的，模块的外部端口的外侧则需要更外层的父模块来进行连接；子模块外部端口的内侧则需要子模块自身来进行连接。

### 请求-服务连接

对于外部端口中定义的请求，没有必须的连接要求，可以悬空不连接（模块可以选择不使用某些请求）。

对于外部端口中定义的服务，则必须进行以下处理之一：
1. 被直接连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被用户代码实现（在 `serv_codelines` 中提供实现代码）。

对于子模块的外部端口中定义的请求，则必须进行以下处理之一：
1. 被直接连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被直接连接到本模块的外部端口中的某个请求，且必须具有相同的参数与返回值定义。
3. 被用户代码实现（在 `req_codelines` 中提供实现代码）。

如果一个请求包含返回值或包含握手信号（`has_handshake` 为 `true`），则其仅能被连接到一个服务或代码实现，不允许一对多连接。
反之，如果一个请求不包含返回值且不包含握手信号，则允许一对多连接（广播连接），包括同时给出用户代码实现。

对于子模块的外部端口中定义的服务，则没有必须的连接要求，可以悬空不连接（模块可以选择不调用子模块的某些服务）。
这些服务也可以在本模块的用户代码中被调用。

### 管线连接

对于外部端口中定义的管线输入端口，仅能被动的被连接或在本模块的用户代码中调用。

对于子模块的外部端口中定义的管线输出端口，必须进行以下处理之一：
1. 被直接连接到某个管线实例的Push/Enq端口，且必须具有相同的数据类型。
2. 被直接连接到本模块的外部端口中的某个管线输出端口，且必须具有相同的数据类型。

对于子模块的外部端口中定义的管线输入端口，必须进行以下处理之一：
1. 被直接连接到某个管线实例的Pop/Deq端口，且必须具有相同的数据类型。
2. 被直接连接到本模块的外部端口中的某个管线输入端口，且必须具有相同的数据类型。

### 阻塞传递连接

模块定义了两个特殊的实例名常量：`TopStallInput` 和 `TopStallOutput`，分别表示模块的顶层阻塞输入和输出端口。

阻塞传递连接表为所有子实例+两个特殊实例构成的有向无环图，其中`TopStallInput`仅有出边，`TopStallOutput`仅有入边。每条边表示阻塞传递关系。当一个子实例调用其内部的`stall`函数时，阻塞信号会沿着有向边传递到后续所有的子实例并调用它们的`stall`函数，当传递到`TopStallOutput`时会调用本模块自身的`stall`函数并在父模块中继续传递。

如果`TopStallInput`没有被间接连接到`TopStallOutput`，其默认会被直接连接到`TopStallOutput`。

本实例内部的用户代码可以通过`is_stalled`函数查询当前模块是否处于阻塞状态。即当前周期是否有阻塞信号传递到`TopStallOutput`。

阻塞传递连接隐含了更新顺序约束，确保在一个周期内，所有阻塞信号的传递和查询都能正确反映当前周期的状态。

`TopStallInput`的连接不会被反映到更新次序约束中，传递到`TopStallInput`的阻塞信号也不会触发本模块的`stall`函数调用。

### 更新顺序约束

模块允许用户自定义子模块之间的更新顺序约束，以确保在一个周期内，某些子模块的`tick`函数调用发生在其他子模块之前。

本模块自身的`tick`函数也能被加入到更新顺序约束中，通过特殊的实例名常量`TopUpdateNode`表示。

所有的更新顺序约束（包括隐含的阻塞传递约束）必须构成一个有向无环图，，最终本模块内所有`tick`函数的执行顺序为该有向无环图的拓扑排序。


## 注意事项

- `VulModule` 的许多字段（如 `requests`、`services`、`storages`）会直接影响后续代码生成与接口契约，请严格遵守类型与命名规则。



