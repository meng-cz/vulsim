/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "design.h"

#include <memory>

using std::unique_ptr;
using std::shared_ptr;


/*
 Terminal 命令手册（示例语法，参数需按项目约定填写）

 项目操作:
     new <project-name> <project-dir>   # 新建一个设计项目（创建 project_dir 和 project.vul）
     load <project-file-or-dir>      # 加载一个设计项目（打开 project.vul或project_dir）
     save [<project-file-or-dir>]    # 保存当前设计到磁盘（可选目标路径另存为）
     close                           # 关闭当前项目（释放内存、清除状态）
     cancel                          # 关闭当前项目但不保存
     info                            # 显示当前打开项目的路径和基本信息

 简写：
     s -> show
     cf -> config
     c -> combine
     b -> bundle
     i -> instance
     p -> pipe
     co -> connect
     dco -> disconnect
     req -> request
     serv -> service
     st -> storage
     stn -> storagenext
     stt -> storagetick
     pi -> pipein
     po -> pipeout
     -c -> --comment

 设计浏览与查询:
     show                                    # 输出当前设计的全局摘要（bundles, combines, instances, pipes）
     show combine                            # 列出所有 combines
     show combine <combine-name>             # 显示指定 combine 的详细信息（ports, requests, services, storages, cpp funcs）
     show bundle                             # 列出所有 bundles
     show bundle <bundle-name>               # 显示 bundle 定义
     show instance                          # 列出所有实例及其所属 combine
     show pipe                               # 列出所有 pipes
     show config                             # 列出设计级 config 项

 Config 操作:
     config add <name> <value> [--comment "..."]
     config update <name> <value> [--comment "..."]
     config rename <old> <new>
     config delete <name>

 Bundle 操作:
     bundle add <name> [--comment "..."]
     bundle update <name> [--comment "..."]
     bundle member add <bundle> <member> <type> [--comment "..."]
     bundle member update <bundle> <member> <type> [--comment "..."]
     bundle member remove <bundle> <member>
     bundle rename <old> <new>
     bundle delete <name>

 Combine 操作:
     combine add <name> [--comment "..."] [--stallable]
     combine update <name> [--comment "..."] [--stallable|--no-stallable]
     combine rename <old> <new>
     combine duplicate <old> <new>
     combine delete <name>
     combine updatecpps <combine>   # 生成或更新 init/tick/applytick/service 的 cpp 帮助文件并汇报损坏文件

 Combine 内部成员:
     combine config add|update|remove <combine> <configname> [<ref>] [--comment "..."]
     combine pipein|pipeout add|update|remove <combine> <port> [<type>] [--comment "..."]
     combine pipein|pipeout rename <combine> <oldport> <newport>
     combine request|service add|update|remove <combine> <name> [--arg <type> <name> [<comment>]] [--return <type> <name> [<comment>]]  [--comment "..."]
     combine request|service rename <combine> <oldname> <newname>
     combine storage add|update|remove <combine> <storage> <type> [--value "..."] [--comment "..."]
     combine storagenext / storagetick 同上

 Instance 操作:
     instance add <name> <combine> [--comment "..."]
     instance update <name> [<combine>] [--comment "..."]   # 变更绑定或注释（变更绑定需确保无连接）
     instance rename <old> <new>
     instance delete <name>
     instance config set <instance> <configname> <value>
     instance config remove <instance> <configname>

 Pipe 操作:
     pipe add <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
     pipe update <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
     pipe rename <old> <new>
     pipe delete <name>

 连接操作:
     connect mod->pipe <instance> <pipeoutport> <pipename> <portindex>
     disconnect mod->pipe <instance> <pipeoutport> <pipename> <portindex>

     connect pipe->mod <instance> <pipeinport> <pipename> <portindex>
     disconnect pipe->mod <instance> <pipeinport> <pipename> <portindex>

     connect req->serv <req-instance> <req-port> <serv-instance> <serv-port>
     disconnect req->serv <req-instance> <req-port> <serv-instance> <serv-port>

     connect stalled <src-instance> <dest-instance>    # 仅可用于 stallable 的 combines，且不会形成 update/sequnce 循环
     disconnect stalled <src-instance> <dest-instance>

     connect sequence <former-instance> <latter-instance>  # 添加 update 顺序约束（不可形成回路）
     disconnect sequence <former-instance> <latter-instance>

 其它:
     genbundleh [<output-path>]                 # 导出 bundle.h，默认在项目cpp目录下
     validate                                   # 运行完整的设计检查并返回错误代码/说明
     generate <output-dir> [<vulsim-lib-dir>]   # 生成设计的 C++ 代码文件，默认 vulsim-lib-dir 为程序同目录下的 lib
     help [subcommand]                          # 列出可用命令与简要说明

 错误与返回:
     大多数命令在失败时会返回以 "#<错误码>: <说明>" 格式的错误字符串，方便上层 UI 处理和定位问题。

 说明:
     - 上述命令是对 `command.h` 中接口的终端映射示例；实际参数格式与交互提示应由 `VulConsole::performCommand` 解析并调用对应的 `cmd*` 接口。
     - 对涉及文件的操作（例如生成/重命名/删除 .cpp 文件）会在项目的 `project_dir/cpp/` 目录下进行。
*/



class VulConsole {
public:
    VulConsole() = default;
    ~VulConsole() = default;

    unique_ptr<VulDesign> design;

    /**
     * @brief Get the command prompt prefix string.
     * The prefix includes the project directory if a design is loaded.
     * @return The command prompt prefix string.
     */
    string getPrefix() {
        if (design) return string("vul[") + design->project_name + "]> ";
        return "vul> ";
    }

    /**
     * @brief Perform a command line input.
     * @param cmdline The command line input string.
     * @return A unique_ptr to vector<string> containing the output messages, or nullptr if no output.
     *         The output message can be an error message or a success message.
     */
    unique_ptr<vector<string>> performCommand(const string &cmdline);

protected:

    /**
     * @brief Handle the 'load' command to load a design project from a file or directory.
     * @param args The arguments passed to the 'load' command.
     * @return A unique_ptr to a vector<string> containing the output messages.
     */
    unique_ptr<vector<string>> _consoleLoad(const vector<string> &args);

    /**
     * @brief Handle the 'save' command to save the current design project to a file or directory.
     * @param args The arguments passed to the 'save' command.
     * @return A unique_ptr to a vector<string> containing the output messages.
     */
    unique_ptr<vector<string>> _consoleSave(const vector<string> &args);
};


