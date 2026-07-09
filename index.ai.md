# src 代码索引

## src/apiinline/apiinline.cpp

**文件功能**：串联各类 API inline pass，将模块逻辑代码中的 VUL API 调用替换为端口读写代码。

**主要函数**
- `inlineAPIs(...)`：依次执行 Register、Request、Queue、Memory 等 API inline，并保留调试行号映射。

## src/apiinline/apiinline.hpp

**文件功能**：声明 API inline 总入口接口。

**主要函数/类型**
- `inlineAPIs(...)`：对逻辑 HLS 代码执行所有已支持 API 的 inline 替换。

## src/apiinline/bram.cpp

**文件功能**：实现 BRAM/ROM API 调用到 HLS 端口读写的 inline 替换。

**主要函数**
- `inlineMemoryAPIs(...)`：扫描逻辑代码中的 BRAM/ROM 对象调用并替换为端口操作。
- `unpackHelper(...)`：生成将 packed `Int<N>` 还原为内存数据类型的 helper lambda。
- `emitPack(...)`：生成将内存写入值打包为 `Int<N>` 的代码。
- `bram1rwReqBlock(...)`：生成 1RW BRAM 请求端口赋值代码块。
- `bramWriteBlock(...)`：生成多端口 BRAM 写请求代码块。
- `readReqBlock(...)`：生成 BRAM/ROM 读请求代码块。
- `readDataExpr(...)`：生成 BRAM/ROM 读返回值表达式。

## src/apiinline/bram.hpp

**文件功能**：声明 BRAM/ROM API inline 接口。

**主要函数**
- `inlineMemoryAPIs(...)`：对内存相关 API 调用执行端口化替换。

## src/apiinline/queue.cpp

**文件功能**：实现 Queue/QueueMP API 调用到 HLS 端口读写的 inline 替换。

**主要函数**
- `inlineQueueAPIs(...)`：扫描并替换队列 API 调用。
- `unpackValueHelper(...)`：生成队列出队 packed 数据到原类型的解包 helper。
- `emitPackValue(...)`：生成队列入队值到 packed 数据的打包代码。
- `enqNextBlock(...)`：生成 `enqnext` 对入队端口的赋值代码。
- `frontExpr(...)`：生成 `front` 读取表达式。
- `frontAssignBlock(...)`：将多出队 `front()` 的整数组赋值改写为逐元素赋值。
- `frontHelper(...)`：生成多出队 `front` 返回数组的 helper。
- `deqNextBlock(...)`：生成 `deqnext` 对出队 ready 端口的赋值代码。

## src/apiinline/queue.hpp

**文件功能**：声明 Queue API inline 接口。

**主要函数**
- `inlineQueueAPIs(...)`：对队列相关 API 调用执行端口化替换。

## src/apiinline/register.cpp

**文件功能**：实现 Register API 调用到 HLS 端口读写的 inline 替换。

**主要函数**
- `inlineRegisterAPIs(...)`：扫描并替换寄存器读写 API。
- `readRegisterHelper(...)`：生成从寄存器 packed 读端口还原原类型的 helper。
- `writeRegisterBlock(...)`：生成 `setnext` 写使能和写数据端口赋值代码。
- `inlineRegisterReadsInExpr(...)`：在表达式内部替换寄存器读操作。

## src/apiinline/register.hpp

**文件功能**：声明 Register API inline 接口。

**主要函数**
- `inlineRegisterAPIs(...)`：对寄存器相关 API 调用执行端口化替换。

## src/apiinline/request.cpp

**文件功能**：实现 Request/Service API 调用到握手端口和参数端口的 inline 替换。

**主要函数**
- `inlineRequestAPIs(...)`：扫描并替换 request/service 调用。
- `emitRequestBody(...)`：生成一次请求调用的 valid、参数和返回值端口操作。
- `requestHelperDef(...)`：生成带 ready 返回值的请求 helper lambda。
- `requestReadyPrelude(...)`：生成直接作为 if 条件的请求调用预处理代码。
- `requestCallExpr(...)`：生成请求调用表达式或 helper 调用表达式。

## src/apiinline/request.hpp

**文件功能**：声明 Request/Service API inline 接口。

**主要函数**
- `inlineRequestAPIs(...)`：对请求/服务相关 API 调用执行端口化替换。

## src/apiinline/utils.cpp

**文件功能**：提供 API inline 共用的文本处理、libclang token 化、字段 pack/unpack 和 debug map 工具。

**主要函数**
- `joinLines(...)`：将代码行拼接为完整源码字符串。
- `splitLinesKeepEnds(...)`：按行拆分源码并保留换行符。
- `uintExtractExpr(...)`：生成 `Int<N>.at<hi, lo>()` 位段访问表达式。
- `castToLvalueTypeExpr(...)`：生成旧式按目标左值类型转换的表达式。
- `packFlatFieldValueExpr(...)`：根据 `FlatField` 生成结构体字段打包到 `Int<N>` 的表达式。
- `unpackFlatFieldValueExpr(...)`：根据 `FlatField` 生成 packed 位段还原为结构体成员的表达式。
- `flatFieldValueExpr(...)`：将展平字段名映射为相对某个根对象的访问表达式。
- `enumDefaultValueExpr(...)`：计算 enum 类型的默认初始成员表达式。
- `defaultValueExprForType(...)`：生成类型默认值表达式。
- `findMatching(...)`：在 token 序列中查找匹配括号位置。
- `splitTopLevelArgs(...)`：按顶层逗号拆分调用参数。
- `tokenizeWithLibclang(...)`：使用 libclang 对源码进行 token 化。
- `applyReplacementsWithDebug(...)`：应用文本替换并同步更新 debug 行号映射。

## src/apiinline/utils.hpp

**文件功能**：声明 API inline 共用数据结构和工具函数。

**主要函数/类型**
- `TokenInfo`：保存一个 libclang token 的文本、类型和源码区间。
- `Replacement`：描述一次源码替换的起止位置和替换文本。
- `InlineCode`：保存 inline 后代码行及其 debug 映射。
- `packFlatFieldValueExpr(...)`：声明字段打包表达式生成函数。
- `unpackFlatFieldValueExpr(...)`：声明字段解包表达式生成函数。
- `tokenizeWithLibclang(...)`：声明 libclang token 化入口。

## src/breakpoint.hpp

**文件功能**：解析断点表达式并将其转换为 trace 系统可使用的位串比较条件。

**主要函数/类型**
- `VulBreakConditionSpec`：表示一个断点条件中的信号、期望位串和显示值。
- `VulBreakPointSpec`：表示一个完整断点表达式及其条件列表。
- `collectTracedSignalWidths(...)`：收集已 trace 信号的运行时名称和位宽。
- `parseBreakExpression(...)`：解析单条断点表达式。
- `parseBreakSpecs(...)`：解析命令行或文件中的断点配置。
- `breakpointParseLiteralToBits(...)`：将断点字面量转换为固定宽度二进制字符串。

## src/bundlelib.cpp

**文件功能**：解析、静态化和展平 VUL 的 struct、enum、alias 类型定义。

**主要函数**
- `parseMemberDeclaration(...)`：解析结构体成员声明文本。
- `parseTypeSignature(...)`：解析类型签名并识别 `Int<N>`。
- `staticalizeBundle(...)`：将临时 bundle 定义解析为静态 bundle。
- `mergeStaticBundleLibs(...)`：合并全局和局部 bundle 库。
- `get_basic_width(...)`：计算基础类型位宽。
- `flatten_member(...)`：递归展平成 packed 字段列表。
- `flatten_bundle(...)`：将 struct、alias 或 enum 展平成 `FlatField` 列表。

## src/bundlelib.h

**文件功能**：定义 VUL 类型系统中 bundle、成员、enum 和展平字段的数据结构。

**主要函数/类型**
- `VulStaticTypeSignature`：表示静态类型签名及 `Int<N>` 位宽。
- `VulStaticBundle`：表示静态 struct、enum 或 alias 定义。
- `FlatField`：表示展平后的字段路径、位宽、offset、fixint 标志和 enum 类型名。
- `flatten_type_signature(...)`：按类型签名生成展平字段列表。
- `flatten_bundle(...)`：声明 bundle 展平入口。
- `flatten_member(...)`：声明成员展平入口。

## src/configexpr.cpp

**文件功能**：实现配置表达式的词法分析、语法分析、求值和引用提取。

**主要函数**
- `tokenizeConfigValueExpression(...)`：将配置表达式拆成 token。
- `parseConfigValueExpression(...)`：将 token 序列解析为 AST。
- `evaluateConfigValueExpression(...)`：计算配置表达式 AST 的整数值。
- `parseReferencedIdentifier(...)`：提取表达式中引用的配置名。

## src/configexpr.hpp

**文件功能**：声明配置表达式解析器的数据结构和接口。

**主要函数/类型**
- `Token`：表示配置表达式 token。
- `ASTNode`：表示配置表达式 AST 节点。
- `tokenizeConfigValueExpression(...)`：声明配置表达式词法分析入口。
- `parseConfigValueExpression(...)`：声明配置表达式语法分析入口。
- `evaluateConfigValueExpression(...)`：声明配置表达式求值入口。

## src/configlib.cpp

**文件功能**：实现配置库静态化、常量表达式求值和配置合并。

**主要函数**
- `insertStaticConfig(...)`：将一个配置项插入静态配置库。
- `calculateConstexprValue(...)`：在给定配置库下计算常量表达式值。
- `mergeStaticConfigLibs(...)`：合并全局和局部静态配置。

## src/configlib.h

**文件功能**：定义配置项、配置库和配置求值接口。

**主要函数/类型**
- `VulTempConfig`：表示解析阶段的配置定义。
- `VulStaticConfigLib`：表示静态配置名到整数值的映射。
- `insertStaticConfig(...)`：声明配置静态化入口。
- `calculateConstexprValue(...)`：声明配置表达式求值入口。
- `mergeStaticConfigLibs(...)`：声明配置库合并入口。

## src/cppparse.cpp

**文件功能**：提供轻量级 C++/宏源码扫描、注释剥离和代码块解析能力。

**主要函数**
- `readFileLines(...)`：读取文件为代码行。
- `stripComments(...)`：移除 C/C++ 注释并保留行结构。
- `findNext(...)`：查找下一个指定字符位置。
- `findNextBraceBlock(...)`：查找括号包围的代码块。
- `matchMacros(...)`：按宏模式匹配宏调用。
- `codeblockContainsFunctionCall(...)`：检测代码块中是否调用指定函数。
- `findAllMacroEntries(...)`：扫描所有顶层宏条目及其参数和代码块。

## src/cppparse.hpp

**文件功能**：声明轻量 C++/宏源码解析工具。

**主要函数/类型**
- `TrimResult`：保存去注释后的代码和行号映射。
- `LinePosition`：表示源码行列位置。
- `MatchMacroResult`：表示一次宏匹配结果。
- `BlockResult`：表示括号代码块范围。
- `MacroEntry`：表示解析出的宏调用条目。
- `findAllMacroEntries(...)`：声明宏扫描入口。

## src/debugmap.hpp

**文件功能**：维护生成代码行到原始代码位置的调试映射。

**主要函数**
- `vulDebugAppendLine(...)`：追加一行代码及其 debug 位置。
- `vulDebugAppendLines(...)`：批量追加代码及 debug 位置。
- `vulDebugBuildGeneratedMap(...)`：构建最终行号映射。
- `vulDebugNormalize(...)`：规整代码行和 debug 映射长度。
- `vulDebugWriteMapToFile(...)`：写出 debug map 文件。

## src/errormsg.cpp

**文件功能**：实现全局错误上下文栈的压入、弹出和格式化。

**主要函数**
- `getGlobalErrorContextStack()`：返回进程内全局错误上下文栈。
- `vulGetErrorContextStack()`：读取当前错误上下文栈。
- `vulPushErrorContext(...)`：压入一层错误上下文。
- `vulPopErrorContext()`：弹出一层错误上下文。

## src/errormsg.hpp

**文件功能**：定义错误对象、异常类型和 RAII 错误上下文工具。

**主要函数/类型**
- `ErrorMsg`：保存错误码和错误消息。
- `VulException`：携带上下文栈的运行时异常。
- `VulErrorContextGuard`：进入作用域时压入错误上下文，离开时弹出。
- `EStr(...)`：构造 `ErrorMsg`。

## src/module.cpp

**文件功能**：将临时模块实例化为静态模块层次，并分析连接和更新顺序。

**主要函数**
- `staticalizeReqServ(...)`：静态化 request/service 定义。
- `instantiateModule(...)`：递归实例化模块及其子模块。
- `detectRequestCallInLogicBlocks(...)`：扫描逻辑块中的 request/service 调用。
- `findConnectedLogicBlockID(...)`：定位请求连接到的服务逻辑块。
- `setupUpdateSequence(...)`：计算模块仿真的更新顺序。

## src/module.h

**文件功能**：定义 VUL 模块、寄存器、请求、服务、队列、BRAM、子实例和静态实例层次的数据结构。

**主要函数/类型**
- `VulTempModule`：表示宏解析后的临时模块。
- `VulStaticModuleInstance`：表示静态化后的模块实例。
- `VulStaticReqServ`：表示静态 request/service 定义。
- `VulStaticRegister`：表示静态寄存器定义。
- `VulStaticQueue`：表示静态队列定义。
- `instantiateModule(...)`：声明模块实例化入口。
- `setupUpdateSequence(...)`：声明更新顺序构建入口。

## src/project.h

**文件功能**：定义完整 VUL 工程的静态表示。

**主要类型**
- `VulStaticProject`：保存顶层模块、全局配置、全局 bundle、全局 helper 和测试模块。

## src/rtlgen.cpp

**文件功能**：生成 RTL skeleton、HLS logic wrapper、API helper 和 Verilator 测试桥接代码。

**主要函数**
- `genModuleRTL(...)`：生成旧路径 RTL 代码。
- `genModuleRTLV2(...)`：生成 V2 RTL skeleton 和 inline 后逻辑代码。
- `genModuleRTLImpl(...)`：执行 RTL 生成各阶段的共用实现。
- `_procConstAndBundle(...)`：生成常量、类型和 helper 头部。
- `_procWires(...)`：生成 wire 相关 HLS 初始化和 RTL 声明。
- `_procRegisters(...)`：生成寄存器端口、代理/helper 和 RTL 实例。
- `_procRequests(...)`：生成 request 端口和调用包装。
- `_procServicesAndTicks(...)`：生成 service/tick 逻辑入口和返回端口。
- `_procQueries(...)`：生成 query 端口和打包逻辑。
- `_procChildrenAndConnection(...)`：生成子模块连接、服务转发和查询转发。
- `_procQueues(...)`：生成 Queue/QueueMP 端口、helper 和 RTL 实例。
- `_procBRAMAndROM(...)`：生成 BRAM/ROM 端口、helper 和 RTL 实例。
- `genVerilatorTestMainCpp(...)`：生成 Verilator 顶层测试绑定代码。

## src/rtlgen.h

**文件功能**：声明 RTLGen 结果结构和 RTL 生成入口。

**主要函数/类型**
- `RTLGenResult`：保存 skeleton、logic、debug map 和资源文件列表。
- `LogicSubModuleName(...)`：生成逻辑子模块名称。
- `genModuleRTL(...)`：声明 V1 RTL 生成入口。
- `genModuleRTLV2(...)`：声明 V2 RTL 生成入口。
- `appendRTLV2LogicRTL(...)`：声明调用 RTLZZ 追加 logic RTL 的入口。
- `genVerilatorTestMainCpp(...)`：声明 Verilator 测试代码生成入口。

## src/rtlgen_rtlzz.cpp

**文件功能**：将 RTLGenV2 生成的 logic C++ 交给 RTLZZ 转换为 RTL 并追加到 skeleton。

**主要函数**
- `appendRTLV2LogicRTL(...)`：调用 RTLZZ 生成 logic RTL 并追加到 `RTLGenResult`。
- `appendTextAsLines(...)`：将 RTLZZ 输出文本追加为代码行。

## src/rtlzz_bridge.cpp

**文件功能**：封装对 RTLZZ 编译接口的调用。

**主要函数**
- `generateLogicRTLWithRTLzz(...)`：读取 logic C++，调用 RTLZZ 生成 SystemVerilog 文本。

## src/rtlzz_bridge.hpp

**文件功能**：声明 RTLZZ 桥接函数。

**主要函数**
- `generateLogicRTLWithRTLzz(...)`：声明 logic C++ 到 RTL 文本的转换入口。

## src/simgen.cpp

**文件功能**：生成 C++ 仿真模型、测试 harness、主程序和 trace/breakpoint 集成代码。

**主要函数**
- `genCurrentTimeString()`：生成当前时间字符串。
- `genStaticConfigHeaderCode(...)`：生成静态配置头文件代码。
- `genStaticBundle(...)`：生成单个静态 bundle 的 C++ 定义。
- `genStaticBundleHeaderCode(...)`：生成 bundle 头文件代码。
- `genStaticProjectHeaderCode(...)`：生成工程公共头文件代码。
- `genStaticModuleCodeHpp(...)`：生成单个模块实例的声明和实现代码。
- `genStaticTestHarnessCodeHpp(...)`：生成测试 harness 声明和实现代码。
- `genStaticTestHarnessHpp(...)`：生成测试 harness 聚合头文件。
- `genStaticTestMainHpp(...)`：生成仿真 main 入口代码。
- `parseConcreteInstanceIndices(...)`：解析具体子实例索引。
- `buildExplicitArrayWrapperLines(...)`：为数组子实例生成显式 wrapper。

## src/simgen.h

**文件功能**：声明仿真代码生成接口和生成结果结构。

**主要函数/类型**
- `StaticModuleCodeHpp`：保存模块声明/实现代码及 debug map。
- `StaticTestHarnessCodeHpp`：保存测试 harness 声明/实现代码。
- `genStaticModuleCodeHpp(...)`：声明模块仿真代码生成入口。
- `genStaticTestHarnessCodeHpp(...)`：声明测试 harness 代码生成入口。
- `genStaticTestMainHpp(...)`：声明 main 代码生成入口。

## src/stringop.hpp

**文件功能**：提供字符串 trim、空白跳过和 split 工具。

**主要函数**
- `ltrim_inplace(...)`：原地移除左侧空白。
- `rtrim_inplace(...)`：原地移除右侧空白。
- `trim_inplace(...)`：原地移除两侧空白。
- `trim(...)`：返回去除两侧空白的新字符串。
- `skip_spaces(...)`：从指定位置跳过空白字符。
- `split(...)`：按字符或字符串分隔符拆分字符串。

## src/toposort.hpp

**文件功能**：提供带诊断信息的有向图拓扑排序工具。

**主要函数/类型**
- `TopoGraph`：维护节点、边和拓扑排序状态。
- `addNode(...)`：加入排序节点。
- `addEdge(...)`：加入依赖边。
- `sort()`：执行拓扑排序并检测环。
- `getSorted()`：返回排序结果。
- `getCycle()`：返回检测到的环路径。

## src/trace.cpp

**文件功能**：解析 trace 规则并为每个模块实例选择需要记录的展平信号。

**主要函数**
- `parseTraceMatcher(...)`：解析单条 trace 匹配表达式。
- `parseTraceOptions(...)`：根据工程层次和匹配规则生成 trace 表。
- `matchPathSegments(...)`：按路径段和通配符匹配实例或信号路径。
- `extractInstanceIndexFilter(...)`：解析数组实例 trace 的索引过滤条件。

## src/trace.hpp

**文件功能**：定义 trace 匹配规则、trace 信号和 trace 表。

**主要函数/类型**
- `VulTraceMatcher`：表示实例路径和信号路径匹配规则。
- `VulTracedSignal`：表示一个被 trace 的展平信号及其位宽和实例过滤。
- `VulTraceTable`：表示实例 ID 到 trace 信号列表的映射。
- `parseTraceMatcher(...)`：声明 trace 规则解析入口。
- `parseTraceOptions(...)`：声明 trace 表生成入口。

## src/type.cpp

**文件功能**：实现标识符合法性检查、标识符替换和基础类型判断。

**主要函数**
- `isValidIdentifier(...)`：检查字符串是否为合法且非保留的标识符。
- `identifierReplace(...)`：按标识符边界替换名称。
- `isBasicVulType(...)`：判断类型名是否为 VUL 基础类型。

## src/type.h

**文件功能**：定义基础别名和类型检查工具接口。

**主要函数/类型**
- `Comment`：表示源码注释文本。
- `DataType`：表示类型名字符串。
- `isValidIdentifier(...)`：声明标识符合法性检查。
- `identifierReplace(...)`：声明标识符替换工具。
- `isBasicVulType(...)`：声明基础类型判断工具。

## src/vcpp.cpp

**文件功能**：解析 VUL C++ 宏工程，构建临时模块、测试模块和静态工程。

**主要函数/类型**
- `parseVcppStaticProject(...)`：解析工程并返回静态工程对象。
- `_parseTempModule(...)`：解析单个模块头文件为临时模块。
- `_parseTestModule(...)`：解析测试 main 文件为测试模块。
- `parseProjectHeaders(...)`：扫描并解析工程 header 文件。
- `importGlobalHeaderModule(...)`：导入全局 header 中的配置、类型和 helper。
- `VCPPModuleHandler`：模块宏处理器基类。
- `VCPPTestHandler`：测试宏处理器基类。

## src/vcpp.hpp

**文件功能**：声明 VUL C++ 工程解析入口。

**主要函数**
- `parseVcppStaticProject(...)`：从工程目录、顶层模块和测试文件解析静态工程。

## src/vullib.hpp

**文件功能**：定义生成输出时需要复制的 VUL runtime 文件清单。

**主要函数/类型**
- `getVulLibFiles()`：返回 runtime 头文件和资源文件列表。
