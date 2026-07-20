# RTLzz issue notes

RTLzz 当前版本（验证时子模块 HEAD 为 `9f7ad3c`）已经修复了非数组 output port 被错误生成为 `port[0]` 的问题；`example/prodcon/TopModule.hpp` 和 `example/enumdemo/Top.hpp` 均可完成 `vulrtlgen -t` 生成并通过 Verilator lint（仅剩 `TIMESCALEMOD` warning）。下面记录本轮验证中仍阻断更多 example RTL 生成或 lint 的问题。

`example/systolic2d/Top.hpp` 在 RTLzz `s0ast` 阶段失败，生成的 `top.logic.cpp` 中出现 lambda 与 `int` 参与二元表达式的情况，报错为 `invalid operands to binary expression ('(lambda at rtlzz_input.logic.cpp:87:16)' and 'int')`，位置在 `rtlzz_input.logic.cpp:140:67`。

`example/apiinline_proxy/Top.hpp` 在 RTLzz `s0ast` 阶段失败，报错为 `redefinition of 'popped' with a different type`，同名局部变量被识别为不同数组元素类型：`PayloadPair[1]` 与 `PayloadPair[0]`，位置在 `rtlzz_input.logic.cpp:232:15`。

`example/querydemo/Top.hpp` 在 RTLzz `s3statementize` 阶段失败，原因是当前 lowering 不支持条件运算符 `?:`，报错为 `Unsupported binary operator '?'`，位置在 `rtlzz_input.logic.cpp:86:12-:24`。

`example/childalias/Top.hpp` 在 RTLzz `s3statementize` 阶段失败，原因同样是条件运算符 `?:` 未被支持，报错为 `Unsupported binary operator '?'`，位置在 `rtlzz_input.logic.cpp:37:26-:43`。

`example/pipeline_holdreset/Top.hpp` 在 RTLzz `s0ast` 阶段失败，报错为 `S0 cannot resolve operator call receiver from cursor metadata`，说明 S0 从 clang cursor 元数据解析某个 operator call 的 receiver 失败，位置在 `rtlzz_input.logic.cpp:89:51`。

`example/queue_mp_demo/Top.hpp` 在 RTLzz `s0ast` 阶段失败，报错同样为 `S0 cannot resolve operator call receiver from cursor metadata`，位置在 `rtlzz_input.logic.cpp:71:49`。

`example/apiinline_register/Top.hpp` 在 RTLzz `s0ast` 阶段失败，报错同样为 `S0 cannot resolve operator call receiver from cursor metadata`，位置在 `rtlzz_input.logic.cpp:227:199`。

`example/array1d/Top.hpp` 在 RTLzz `s2validate` 阶段失败，报错为 `Static local declaration is not supported unless it is a lookup table`，说明生成的 logic 源中存在非 lookup table 的 static local declaration，目前 S2 validation 不接受这种结构。

`example/rv64ima5/Top.hpp` 在 RTLzz `s2validate` 阶段失败，报错同样为 `Static local declaration is not supported unless it is a lookup table`，说明该复杂示例生成的 logic 源中也包含当前 RTLzz validation 不支持的 static local declaration。

`example/verilator_fsm/Top.hpp` 在 RTLzz `s2validate` 阶段失败，报错同样为 `Static local declaration is not supported unless it is a lookup table`，需要确认该类 static local 是否应由前端规避，或由 RTLzz 支持。

`example/mulu32/MulU32.hpp` 在 RTLzz `s7flatten` 阶段失败，报错为 `Non-ternary operation target is aggregate`，位置在 `rtlzz_input.logic.cpp:210:45-:51`，说明 flatten 阶段遇到了非三元操作以 aggregate 作为 target 的情况，目前无法 lowering。

`example/aes1/AES1.hpp` 在 RTLzz `s0ast` 阶段失败，报错为 `use of undeclared identifier 'value'`，位置在 `rtlzz_input.logic.cpp:123:33`，需要检查 S0 对该段生成 C++ 中临时变量或 lambda 返回值的识别是否丢失上下文。

`example/fifotest/Top.hpp` 没有进入 RTLzz 阶段，失败来自主项目工程解析：`example/fifotest` 目录下缺少 `header.hpp/header.h` 或 `header/` 目录，报错为 `Project root must contain either a header directory or a header.hpp/header.h file`。该项不是 RTLzz lowering 问题，但会阻止按 `vulrtlgen -t example/fifotest/Top.hpp` 直接验证该示例。

