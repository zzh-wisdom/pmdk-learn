# libpmem2 man

## 使用

```cpp
#include <libpmem2.h>
cc ... -lpmem2
```

## 描述

libpmem2 为使用直接访问存储 (DAX) 的应用程序提供低级持久内存 (pmem) 支持，该存储支持加载/存储访问，而无需从块存储设备中分页块。某些类型的非易失性内存 DIMM (NVDIMM) 提供这种类型的字节可寻址存储访问。持久内存感知文件系统通常用于公开对应用程序的直接访问。从这种类型的文件系统映射文件的内存会导致对 pmem 的加载/存储、非分页访问。

此库适用于直接使用持久内存的应用程序，无需任何库提供的事务或内存分配的帮助。当前基于 libpmem（libpmem2 的先前变体）构建的更高级别的库是可用的，并且推荐用于大多数应用程序，请参阅：

- libpmemobj(7), a general use persistent memory API, providing memory allocation and transactional operations on variable-sized objects.
- libpmemblk(7), providing pmem-resident arrays of fixed-sized blocks with atomic updates.
- libpmemlog(7), providing a pmem-resident log file.

libpmem2 库提供了一套全面的函数，用于持久内存的稳健使用。
它依赖于三个核心概念：struct pmem2_src source、struct pmem2_config config 和 struct pmem2_map map：

- source - 描述用于映射的数据源对象。数据源可以是文件描述符、文件句柄或匿名映射。专用于创建源的 API 有：pmem2_source_from_fd(3)、pmem2_source_from_handle(3)、pmem2_source_from_anon(3)。
- config - 包含用于从源创建映射的参数的对象。必须始终提供 config 结构以创建映射，但在配置中设置的唯一必需参数是粒度。应该使用专用的 libpmem2 函数 pmem2_config_set_required_store_granularity(3) 来设置粒度，**该函数定义了用户请求的最大允许粒度**。有关粒度概念的更多信息，请阅读下面的粒度部分。

除了粒度设置之外，libpmem2 还提供了多个可选函数来配置目标映射，例如 pmem2_config_set_length(3) 设置将用于映射的长度，或 pmem2_config_set_offset(3) 将用于从指定位置映射内容
源、pmem2_config_set_sharing(3) 定义了写入映射页面的行为和可见性。

- map - 由 pmem2_map_new(3) 创建的对象，使用 source 和 config 作为输入参数。然后可以使用map结构体通过使用其关联的一组函数直接对创建的映射进行操作：pmem2_map_get_address(3)、pmem2_map_get_size(3)、pmem2_map_get_store_granularity(3) - 用于获取地址、大小和有效映射粒度。

除了管理虚拟地址映射的基本功能外，libpmem2 还提供了**修改映射数据的优化功能**。这包括数据刷新以及内存复制。

要获得正确的数据刷新功能，请使用：pmem2_get_flush_fn(3)、pmem2_get_persist_fn(3) 或 pmem2_get_drain_fn(3)。
要获得复制到持久内存的正确功能，请使用映射 getter：pmem2_get_memcpy_fn(3)、pmem2_get_memset_fn(3)、pmem2_get_memmove_fn(3)。