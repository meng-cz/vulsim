# module.storage.set - Set Module Storage Information

Set a storage value in a module in the project's module library.

## Arguments

- [0] `name`: The name of the module to set the storage in.
- [1] `oldname` (optional): The name of the storage to set.
- [2] `newname` (optional): The new name of the storage.
- [3] `definition` (optional): The new definition of the storage.
- [4] `category` (optional): The new category of the storage ("regular", "register", "temporary").

## Notes

- This operation modifies module storage and is undoable.
