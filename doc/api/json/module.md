# VulModuleOverview 序列化 JSON 格式

用于 `module.info` 操作的 VulModuleOverview 对象的 JSON 序列化格式说明。

```json
{
 "comment": "...",
 "local_configs": [
  { "name": "...", "value": "...", "comment": "..." }, ...
 ],
 "local_bundles": [
  { "name": "...", "comment": "..."}, ...
 ],
 "requests": [
  { "name": "...", "comment": "...", "sig": "..." }, ...
 ],
 "services": [
  { "name": "...", "comment": "...", "sig": "..." }, ...
 ],
 "pipein": [
  { "name": "...", "comment": "...", "type": "..." }, ...
 ],
 "pipeout": [
  { "name": "...", "comment": "...", "type": "..." }, ...
 ],
 "instances": [
  { "name": "...", "comment": "...", "module": "...", "order": "...", "configs": [{ "name": "...", "value": "..." }, ...], "module" : {...} }, ...
 ],
 "pipes": [
  { "name": "...", "comment": "...", "type": "...", "has_handshake": true/false, "has_valid": true/false }, ...
 ],
 "user_tick_codeblocks": [
  { "name": "...", "comment": "...", "order": "..." }, ...
 ],
 "serv_codelines": [ "...", ... ],
 "req_codelines": [
  { "instance": "...", "req_name": "..."}, ...
 ],
 "storages": [
  { "name": "...", "category": "...", "type": "...", "comment": "..." },
 ],
 "connections" : [
  { "src_instance": "...", "src_port": "...", "dst_instance": "...", "dst_port": "..." }, ...
 ],
 "pipe_connections" : [
  { "instance": "...", "port": "...", "pipe": "...", "pipe_port": "..." }, ...
 ],
 "stalled_connections" : [
  { "former_instance": "...", "latter_instance": "..." }, ...
 ],
 "update_constraints" : [
  { "former_instance": "...", "latter_instance": "..." }, ...
 ]
}
```

- comment: 模块注释，字符串类型。
- local_configs: 模块本地配置列表
- local_bundles: 模块本地 bundle 列表
- requests: 模块请求端口列表
- services: 模块服务端口列表
- pipein: 模块管道输入端口列表
- pipeout: 模块管道输出端口列表
- instances: 模块子实例列表
- pipes: 模块管道子实例列表
- user_tick_codeblocks: 用户时钟代码块的名称、注释、更新顺序列表
- serv_codelines: 服务端口代码行绑定的端口名列表
- req_codelines: 请求端口代码行绑定的实例名和端口名列表
- storages: 模块存储实例列表
- connections: 模块内请求-服务端口连接列表
- pipe_connections: 模块内管道子实例连接列表
- stalled_connections: 模块内阻塞传递连接列表
- update_constraints: 模块内更新次序连接列表

对于从外部导入的模块，其内部结构不可见，仅包含 comment, local_configs, local_bundles, requests, services, pipein, pipeout 字段，无其他字段。

**后续需要补充额外的字段用于可视化信息**
