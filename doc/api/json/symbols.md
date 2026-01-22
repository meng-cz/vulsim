# VulSymbolTable 序列化 JSON 格式

用于 `code.get` 操作的 VulSymbolTable 对象的 JSON 序列化格式说明。


```json
{
  "constexpr_defs": [ {
     "comment": "...",
     "name": "...",
     "value": "..."   
  }, ... ],
  "function_defs": [ {
     "comment": "...",
     "name": "...",
     "handshake": true/false, 
     "args": [ "...", "...", ... ],
     "relavent_instance": "..."
  }, ... ],
  "struct_defs": [ {
     "comment": "...",
     "name": "...",
     "members": [ { "type": "...", "name": "...", "value": "..." }, ... ],
     "aliased_type": "...",
     "is_enum": true/false,
     "is_alias": true/false
  }, ... ],
  "variable_defs": [ {
     "comment": "...",
     "type": "...",
     "name": "...",
     "relavent_instance": "..."
  }, ... ]
}
```
