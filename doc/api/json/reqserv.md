# VulReqServ 序列化 JSON 格式

用于 `module.info` `module.req.*` 和 `module.serv.*` 操作的 VulReqServ 对象的 JSON 序列化格式说明。

```json
{
  "comment": "...",
  "has_handshake": true/false,
  "args": [
   {
     "name": "...",
     "type": "...",
     "comment": "..."
  }, ...
  ],
  "rets": [
   {
     "name": "...",
     "type": "...",
     "comment": "..."
  }, ...
  ]
}
```
