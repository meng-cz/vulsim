# VulModule 模块类说明

本文档说明 `VulModule`（在代码中定义于 `src/module.hpp`）的数据结构与使用约定。`VulModule` 描述一个模块（Module）的静态定义、其实例、连接关系、存储器以及用于代码生成的元信息。文档风格参考项目中其他设计说明（例如 `configlib.md`、`bundlelib.md`）。

## 命名与约定

- **模块名**：由 `ModuleName` 表示（字符串），在工程中应当以有意义的名称命名。
- **注释**：每个模块有 `comment` 字段用于说明。

命名冲突检查分为全局命名空间检查和本地命名空间。除了每个命名空间内不能存在冲突之外，每一个本地命名空间内的名称也不能与全局命名空间冲突：

### 全局命名空间：

包含整个项目实例中的：
1. 所有配置项名称（`ConfigName`）。
2. 所有线组名称（`BundleName`）。
3. 所有模块名称（`ModuleName`）。

### 本地命名空间：

包含本模块内部的：
1. 所有局部配置项名称（`ConfigName`）。
2. 所有局部线组名称（`BundleName`）。
3. 所有请求/服务名称（`ReqServName`）。
4. 所有管线输入/输出名称（`PipeInputName` / `PipeOutputName`）。
5. 所有存储名称（`StorageName`）。
6. 所有实例名称（`InstanceName`）。

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

- `user_tick_codeblocks`：用户为模块 `tick` 函数编写的 C 代码行。
- `user_header_filed_codelines`：用户在头文件中声明的字段代码行（各种帮助函数）。
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

模块定义了一个特殊的实例名常量：`TopInterface`，表示模块的顶层接口。

阻塞传递连接表为所有子实例+特殊实例构成的有向无环图，每条边表示阻塞传递关系。当一个子实例调用其内部的`stall`函数时，阻塞信号会沿着有向边传递到后续所有的子实例并调用它们的`stall`函数。

当本模块的`stall`函数被父模块中的其他模块调用时，模块内的阻塞信号从`TopInterface`开始向子模块传递。当子模块唤起一个阻塞信号时，该信号可以通过阻塞连接传递到其他模块或传递到`TopInterface`，从而传递到本模块的父模块。

模块内的阻塞连接不能存在从 `TopInterface` 到 `TopInterface` 的环路。允许部分子模块响应 `TopInterface` 的阻塞信号，另一部分子模块会将阻塞信号传递到 `TopInterface`。

阻塞连接隐含了子模块之间的更新顺序约束：如果子模块A的阻塞信号传递到子模块B，则A的`tick`函数必须在B的`tick`函数之前调用。如果一个子模块的阻塞信号传递到`TopInterface`，则该子模块的`tick`函数必须在本模块的所有`tick_codeblocks`之前调用。

### 更新顺序约束

模块允许用户自定义子模块之间的更新顺序约束，以确保在一个周期内，某些子模块的`tick`函数调用发生在其他子模块之前。

本模块自身的`tick`函数也能被加入到更新顺序约束中，通过特殊的实例名常量`TopUpdateNode`表示。

所有的更新顺序约束（包括隐含的阻塞传递约束）必须构成一个有向无环图，，最终本模块内所有`tick`函数的执行顺序为该有向无环图的拓扑排序。


## 局部配置项

模块类支持局部配置项的定义与使用。局部配置项是模块内部使用的配置参数，不会暴露给外部模块或全局配置系统。局部配置项通过 `local_configs` 字段进行定义。

模块类定义中仅包含局部配置项的初始值，实例化时可以对不同的实例进行不同的局部配置项覆盖，定义在 `VulInstance` 的 `local_config_overrides` 字段中。

局部配置项仅能在模块内部访问：
1. 本模块的用户代码中可直接通过 `配置项名称` 访问。
2. 在子模块实例的局部配置项覆盖中通过 `配置项名称` 访问。

局部配置项的赋值表达式中允许引用全局配置库中的配置项，但不会随着全局配置项的变化而自动更新。仅在最终生成代码的时候这些值会被解析。

| 比较 | 局部配置项 | 全局配置项 |
| ---- | ---------- | ---------- |
| 定义位置 | 模块类定义中 | 全局配置库 |
| 赋值位置 | 模块实例化中 | 全局配置库 |
| 作用范围 | 模块内部 | 全局可见 |
| C++元素 | 模板类参数 | constexpr编译时常量 |
| RTL（SV）元素 | module parameter | package parameter |

## 局部线组

模块类支持局部线组（Local Bundle）的定义与使用。局部线组是模块内部使用的线组类型，不会暴露给外部模块（仅直接包含此模块实例的父模块可以引用，以允许在request或service中使用局部线组）。局部线组通过 `local_bundles` 字段进行定义。

局部线组中可以使用局部配置项作为常量表达式的一部分，用于数组或位宽定义。

局部线组名必须在模块局部唯一，且不能与全局命名空间冲突。

## 行为代码编写

模块类中有三类代码需要用户通过C语言语法编写，并最终嵌入生成的后端代码中：

1. tickblocks: 一个模块可以有很多个tick代码块，每个代码块本质上是一个子实例，可以被自定义的子实例更新顺序约束。用户需要在 `user_tick_codeblocks` 字段中为每个代码块编写C代码行，这些代码行会被按照顺序嵌入到生成的模块类的 `tick` 函数中。

2. 本模块定义的服务: 每一个未连接的服务都需要用户在 `serv_codelines` 字段中为其编写C代码行，这些代码行会被嵌入到生成的模块类的对应服务函数中。

3. 子模块定义的请求: 每一个未连接的请求都需要用户在 `req_codelines` 字段中为其编写C代码行，这些代码行会被嵌入到生成的模块类的对应请求的wrapper函数中。

在这些代码中，可以访问到的资源包括：
1. 所有全局Config和全局Bundle，直接以名字访问。
2. 本模块的所有局部Config和局部Bundle，直接以名字访问。
3. 本模块定义的所有request和service函数，直接以函数名字访问。
4. 本模块定义的所有pipe input端口，通过端口名加后缀 `_can_pop()` / `_pop()` / `_top()` 访问。
5. 本模块定义的所有pipe output端口，通过端口名加后缀 `_can_push()` / `_push()` 访问。
6. 本模块定义的所有storage和storagetmp变量，直接以变量名字访问。
7. 本模块定义的所有storagenext，通过变量名加后缀 `_get()` / `_setnext()` 访问。
8. 所有子模块实例的service函数，通过 `实例名_服务名()` 访问。
9. 所有管道子实例的端口，通过实例名加后缀 `_can_pop()` / `_pop()` / `_top()` / `_can_push()` / `_push()` 访问。
10. 所有子模块实例内部定义的局部Bundle类型，通过 `实例名_局部Bundle名` 访问。
11. 本模块的 `is_stalled()` 函数和 `stall()` 函数。
12. 本模块的 `user_header_field` 域内自定义的helper function和成员变量。



# VulModule 代码生成细节

## 模块类外形定义

```cpp
template<int64 /*local_config_name*/ = /*DefaultValue*/, ...>
class /*ModuleName*/ {
public:
    typedef struct {
        // for local configs
        int64_t /*config_name*/;
        // for requests
        /*HandshakeType*/ (* /*request_name*/)(void*, /*ArgumentSig*/);
        // for pipe inputs
        PipePopPort</*DataType*/> * /*pipe_input_name*/;
        // for pipe outputs
        PipePushPort</*DataType*/> * /*pipe_output_name*/;
        string __instance_name;
        void *__parent_module;
        void *(__stall)(void*);
    } ConstructorParams;

    /*ModuleName*/(const ConstructorParams &params);

    void tick();
    void applyTick();
    void stall();

    // for services
    /*HandsakeType*/ /*service_name*/(/*ArgumentSig*/);
};
```

## hpp，全inline模式

```cpp
// <Header Field>

// <Comment Field>
template<int64 /*local_config_name*/, ...>
class /*ModuleName*/ {
public:

typedef struct {
    // <Constructor Params Field>
    string __instance_name;
    void *__parent_module;
    void *(__stall)(void*);
} ConstructorParams;

// <Local Bundle Field>

/*ModuleName*/(const ConstructorParams &params) : __params(params) { __init(); }

FORCE_INLINE void tick() {
    // <Tick Field>
}

FORCE_INLINE void applyTick() {
    __is_stalled = false;
    // <ApplyTick Field>
}

FORCE_INLINE void stall() {
    // <Stall Field>
    // if connected to child module stall input
    __instptr_/*ConnInstanceName*/->stall();
    __stall_propagate_out();
}

// <Service Field>

protected:

ConstructorParams __params;

void __init() {
    // <Init Field>
}

bool __is_stalled = false;
FORCE_INLINE bool is_stalled() const { return __is_stalled; }
FORCE_INLINE void __stall_propagate_out() {
    __is_stalled = true;
    __params.__stall(__params.__parent_module);
}

// <Member Field>

private:

// <User Header Field>
};
```

### local_configs

无需额外处理，作为模板参数可直接访问

### requests

`<Constructor Params Field>` : 
```cpp
/*HandshakeType*/ (* /*request_name*/)(void*, /*ArgumentSig*/);
```
`<Member Field>` :
```cpp
// Comment
FORCE_INLINE /*HandshakeType*/ /*request_name*/(/*ArgumentSig*/) {
    return __params./*request_name*/(__params.__parent_module, /*ArgumentList*/);
}
```

### services

`<Service Field>` :
```cpp
// Comment
FORCE_INLINE /*ReturnSig*/ /*service_name*/(/*ArgumentSig*/) {
    // if connected to child module service
    return __instptr_/*ConnInstanceName*/->/*ConnServiceName*/(/*ArgumentList*/);
    // if implemented by user code
    // <Service CodeLines Field>
}
```

### pipe_inputs

`<Constructor Params Field>` : 
```cpp
    PipePopPort</*DataType*/> * /*pipe_input_name*/;
```

`<Member Field>` :
```cpp
// Comment
FORCE_INLINE bool /*pipe_input_name*/_can_pop() {
    return __params./*pipe_input_name*/->can_pop();
}
// Comment
FORCE_INLINE void /*pipe_input_name*/_pop(/*DataType*/ * data) {
    __params./*pipe_input_name*/->pop(data);
}
// Comment
FORCE_INLINE void /*pipe_input_name*/_top(/*DataType*/ * data) {
    __params./*pipe_input_name*/->top(data);
}
```

### pipe_outputs

`<Constructor Params Field>` : 
```cpp
    PipePushPort</*DataType*/> * /*pipe_output_name*/;
```

`<Member Field>` :
```cpp
// Comment
FORCE_INLINE bool /*pipe_output_name*/_can_push() {
    return __params./*pipe_output_name*/->can_push();
}
// Comment
FORCE_INLINE void /*pipe_output_name*/_push(const /*DataType*/ & data) {
    __params./*pipe_output_name*/->push(data);
}
```

### user_header_filed_codelines

`<User Header Field>` :
```cpp
// <User Header Field CodeLines Field>
```

### user_tick_codeblocks (in order)

`<Member Field>` :
```cpp
// Comment
FORCE_INLINE void __tick_block_/*InstanceName*/() {
    // <Tick Block CodeLines Field>
}
```

`<Tick Field>` :
```cpp
    __tick_block_/*InstanceName*/();
```

### req_codelines

`<Member Field>` :
```cpp
FORCE_INLINE /*HandshakeType*/ __childreq_/*InstanceName*/_/*RequestName*/(/*ArgumentSig*/) {
    // <Request CodeLines Field>
}
```

### storages / storagenexts / storagetmp

`<Member Field>` :
```cpp
// Comment
/*Type*/ /*storage_name*/{/*InitialValue*/};
```

`<Sys ApplyTick Field>` :
```cpp
    /*storage_name*/.apply_tick();
```

### pipe_instances

`Member Field` :
```cpp
std::unique_ptr</*PipeTypeStr*/> __instptr_/*PipeInstanceName*/;
FORCE_INLINE bool /*PipeInstanceName*/_can_pop() {
    return __instptr_/*PipeInstanceName*/->get_pop_port()->can_pop();
}
FORCE_INLINE void /*PipeInstanceName*/_pop(/*DataType*/ * data) {
    __instptr_/*PipeInstanceName*/->get_pop_port()->pop(data);
}
FORCE_INLINE void /*PipeInstanceName*/_top(/*DataType*/ * data) {
    __instptr_/*PipeInstanceName*/->get_pop_port()->top(data);
}
FORCE_INLINE bool /*PipeInstanceName*/_can_push() {
    return __instptr_/*PipeInstanceName*/->get_push_port()->can_push();
}
FORCE_INLINE void /*PipeInstanceName*/_push(const /*DataType*/ & data) {
    __instptr_/*PipeInstanceName*/->get_push_port()->push(data);
}
```

`<Init Field>` :
```cpp
    {
        __instptr_/*PipeInstanceName*/ = std::make_unique</*PipeTypeStr*/>(/*PipeParams*/);
    }
```

### instances

`<Member Field>` :
```cpp
std::unique_ptr</*ChildModuleName*/</*ChildLocalConfigs*/>> __instptr_/*InstanceName*/;
// for each child services
FORCE_INLINE /*ReturnSig*/ /*InstanceName*/_/*ServiceName*/(/*ArgumentSig*/) {
    return __instptr_/*InstanceName*/->/*ServiceName*/(/*ArgumentList*/);
}
// for each child requests
static /*HandshakeType*/ __childreq_/*InstanceName*/_/*RequestName*/_wrapper(void * __parent_module, /*ArgumentSig*/) {
    // if connected to module request
    return static_cast</*ModuleName*/*>(__parent_module)->/*ConnRequestName*/(/*ArgumentList*/);
    // if connected to child module service
    return static_cast</*ModuleName*/*>(__parent_module)->__instptr_/*ConnInstanceName*/->/*ConnServiceName*/(/*ArgumentList*/);
    // if implemented by user code
    return static_cast</*ModuleName*/*>(__parent_module)->__childreq_/*InstanceName*/_/*RequestName*/(/*ArgumentList*/);
}
static void __childstall_/*InstanceName*/_wrapper(void * __parent_module) {
    // if connected to other instance
    static_cast</*ModuleName*/*>(__parent_module)->__instptr_/*ConnInstanceName*/->stall();
    // if connected to module stall output
    static_cast</*ModuleName*/*>(__parent_module)->__stall_propagate_out();
}
// for local bundles in child module
using /*InstanceName*/_/*LocalBundleName*/ = /*ChildModuleName*/</*ChildLocalConfigs*/>::/*LocalBundleName*/;
```

`<Init Field>` :
```cpp
    {
        /*ChildModuleName*/</*ChildLocalConfigs*/>::ConstructorParams cparams;
        cparams.__instance_name = "/*InstanceName*/";
        cparams.__parent_module = this;
        cparams.__stall = &/*ModuleName*/::__childstall_/*InstanceName*/_wrapper;
        // for requests
        cparams./*RequestName*/ = &/*ModuleName*/::__childreq_/*InstanceName*/_/*RequestName*/_wrapper;
        // for pipe inputs connected to module pipe inputs
        cparams./*PipeInputName*/ = __params./*MappedPipeInputName*/;
        // for pipe inputs connected to pipe instances
        cparams./*PipeInputName*/ = __pipeinstptr_/*PipeInstanceName*/->get_pop_port();
        // for pipe outputs connected to module pipe outputs
        cparams./*PipeOutputName*/ = __params./*MappedPipeOutputName*/;
        // for pipe outputs connected to pipe instances
        cparams./*PipeOutputName*/ = __pipeinstptr_/*PipeInstanceName*/->get_push_port();
        __instptr_/*InstanceName*/ = std::make_unique</*ChildModuleName*/</*ChildLocalConfigs*/>>(cparams);
    }
```





