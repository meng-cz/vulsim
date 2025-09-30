# VulSim SE Controller

内置combine，包含以下tag


* config CoreNum 目标核心数量，用于代码生成

* config MemorySizeMB 目标内存空间，映射到__internal_config_memory_size_mb

* config BaseAddr 目标内存空间，映射到__internal_config_base_addr

* config DevBaseAddr 设备内存空间基地址，映射到__internal_config_dev_base_addr

* request redirect{0...CoreNum}() CoreNum个核心分别的重定向

* service se_exception{0...CoreNum}(uint64 pc, uint32 cause, uint64 arg, array<uint64, 64> & regs, uint64 * nextpc, array<uint64, 64> * modregs) CoreNum个核心分别的异常处理

* service translate{0...CoreNum}(uint64 vaddr, uint64 * paddr, uint32 * error) CoreNum个核心分别的地址翻译

* request dma_read(uint64 addr, array<uint8,64> * data) 读取一个CacheLine

* request dma_write(uint64 addr, array<uint8,64> & data) 写入一个CacheLine

* service devmem_read(uint64 paddr, uint8 len, uint64 * data) 设备内存读

* service devmem_write(uint64 paddr, uint8 len, uint64 data) 设备内存写




