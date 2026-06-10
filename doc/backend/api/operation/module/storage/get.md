# module.storage.get - Get Module Storage Information

Get a storage value from a module in the project's module library.

## Arguments

- [0] `name`: The name of the module to get the storage from.
- [1] `storagename`: The name of the storage to get.

## Results

- `category`: The type of the storage ("regular", "register", "temporary", etc.).
- `definition`: The definition JSON of the storage.

## Notes

- Returns the storage definition and category if found; otherwise an error is returned.
