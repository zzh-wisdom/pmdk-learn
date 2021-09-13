# API 文档

<https://pmem.io/pmdk/manpages/linux/master/libpmem2/libpmem2.7.html>

- [1. config](#1-config)
  - [1.1. 创建与销毁](#11-创建与销毁)
  - [1.2. Config中的参数设置](#12-config中的参数设置)
    - [1.2.1. length](#121-length)
    - [1.2.2. offset](#122-offset)
    - [1.2.3. protection](#123-protection)
    - [1.2.4. granularity](#124-granularity)
    - [1.2.5. sharing](#125-sharing)
- [2. source](#2-source)
  - [2.1. 创建与销毁](#21-创建与销毁)
  - [2.2. 匿名内存页数据源](#22-匿名内存页数据源)
  - [2.3. alignment 对齐方式](#23-alignment-对齐方式)
  - [2.4. size](#24-size)
  - [2.5. get fd/handle](#25-get-fdhandle)
  - [2.6. numa node](#26-numa-node)
- [3. vm_reservation 虚拟内存预留](#3-vm_reservation-虚拟内存预留)
  - [3.1. 创建与销毁](#31-创建与销毁)
  - [3.2. extend & shrink](#32-extend--shrink)
  - [3.3. 设置config的vm_reservation](#33-设置config的vm_reservation)
  - [3.4. get address & size](#34-get-address--size)
  - [3.5. vm中的map查找](#35-vm中的map查找)
- [4. map](#4-map)
  - [4.1. 创建与销毁](#41-创建与销毁)
  - [4.2. 从已有的map创建](#42-从已有的map创建)
  - [4.3. get address & size & granularity](#43-get-address--size--granularity)
- [5. 持久化相关的函数 fn](#5-持久化相关的函数-fn)
  - [5.1. persist](#51-persist)
  - [5.2. flush & drain](#52-flush--drain)
  - [5.3. deep_flush](#53-deep_flush)
  - [5.4. 内存 copy & move & set](#54-内存-copy--move--set)
- [6. 错误处理](#6-错误处理)
- [不安全关机处理](#不安全关机处理)
  - [device id & usc](#device-id--usc)
  - [badblock context](#badblock-context)
  - [badblock next & clear](#badblock-next--clear)

## 1. config

### 1.1. 创建与销毁

```cpp
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_new(struct pmem2_config **cfg);
int pmem2_config_delete(struct pmem2_config **cfg);
```

pmem2_config_new() 函数实例化了一个新的（不透明的）配置结构体 pmem2_config，用于定义 pmem2_map_new() 函数的映射参数，并通过 *cfg 中的指针返回。

新创建的config始终**使用大多数参数的默认值进行初始化**，这些参数可以与相应的 setter 函数一起指定。**应用程序必须显式设置映射的粒度值**。

pmem2_config_delete() 函数释放 pmem2_config_new() 返回的 *cfg 并将 *cfg 设置为 NULL。**如果 \*cfg 为 NULL，则不执行任何操作**。

**返回值：**

- pmem2_config_new() 函数成功时返回 0，失败时返回负错误代码。pmem2_config_new() 确保在失败时将 *cfg 设置为 NULL。
- pmem2_config_new()返回的错误代码有：**-ENOMEM** - out of memory内存不足
- pmem2_config_delete() 函数总是返回 0。

### 1.2. Config中的参数设置

#### 1.2.1. length 

```cpp
int pmem2_config_set_length(struct pmem2_config *config, size_t length);
```

pmem2_config_set_length() 函数设置将用于映射的长度。 此时，*config 需要已被初始化了。**length必须是数据源所需对齐方式(即后面设置的映射粒度)的倍数**，该数据源将用于与配置一起映射。要检索 *pmem2_source ** 的特定实例所需的对齐方式，请使用 pmem2_source_alignment(3)。**默认情况下，长度等于被映射文件的大小**。

返回值总是0。

测试

```cpp
// 超过文件的大小（10+10）, 发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size+10);

    // 超过文件的大小，且是4kb的倍数, 发生error，提示mapping larger than file size
    // pmem2_config_set_length(cfg, (file_size+4096)/4096*4096);

    // 等于文件的大小10，发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size);

    // 等于文件大小10，设置映射粒度为PMEM2_GRANULARITY_BYTE，发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size);

    // 小于文件大小，error，length is not a multiple of 4096
    // pmem2_config_set_length(cfg, 8);

    // 将文件大小扩大为5120
    for(int i=0; i<1024; i++) {
        write(fd, "00000", 5);
    }
    // 超过文件的大小, mapping larger than file size
    // pmem2_config_set_length(cfg, 8192);
    // 小于文件大小，是4kb的倍数，运行通过
    pmem2_config_set_length(cfg, 4096);
```

结论：

1. 映射长度不能超过文件大小
2. length与映射粒度无关，最小的映射长度要为4096的倍数，也就是完整的页，这是由操作系统的内存管理方式决定的
3. 设置映射长度后，只能使用文件开头的length个字节，超出范围则发生段错误

#### 1.2.2. offset

```shell
struct pmem2_config;
int pmem2_config_set_offset(struct pmem2_config *config, size_t offset);
```

pmem2_config_set_offset() 函数配置偏移量，该偏移量将用于从源的指定位置开始映射内容。 \*config 应该已经初始化了，详情请参见 pmem2_config_new(3)。 **\offset 必须是配置所需对齐方式的倍数**。对齐要求是特定于数据源的。要检索 *pmem2_source** 的特定实例所需的对齐方式，请使用 pmem2_source_alignment(3)。默认情况下，偏移量为 0。

pmem2_config_set_offset() 函数成功时返回 0，失败时返回负错误代码。

- PMEM2_E_OFFSET_OUT_OF_RANGE - argument out of range, offset is greater than INT64_MAX

#### 1.2.3. protection

```cpp
struct pmem2_config;

#define PMEM2_PROT_EXEC		(1U << 29)
#define PMEM2_PROT_READ		(1U << 30)
#define PMEM2_PROT_WRITE	(1U << 31)
#define PMEM2_PROT_NONE		0

int pmem2_config_set_protection(struct pmem2_config *cfg,
		unsigned prot);
```

pmem2_config_set_protection() 函数设置将用于内存映射的**保护标志**。 pmem2_config 结构中的**默认值**是 PMEM2_PROT_READ | PMEM2_PROT_WRITE。 \prot 参数描述了映射所需的内存保护。**内存保护不能与文件打开方式冲突**。 *config 应该已经初始化了，详情请参见 pmem2_config_new(3)。

它是 PROT_NONE 或以下一个或多个标志的按位或：

- PMEM2_PROT_EXEC - 页面可以执行。
- PMEM2_PROT_READ - 页面可以读取。
- PMEM2_PROT_WRITE - 页面可以写入。
- PMEM2_PROT_NONE - 不能访问页面。在 Windows 上，不支持此标志。

返回的错误码：

- PMEM2_E_INVALID_PROT_FLAG - 提供的部分或全部标志无效。

#### 1.2.4. granularity

```cpp
enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};
int pmem2_config_set_required_store_granularity(struct pmem2_config *cfg,
		enum pmem2_granularity g);
```

pmem2_config_set_required_store_granularity() 设置用户在 pmem2_config 结构中请求的最大允许粒度 g（即保证原子存储的最大粒度）。

如果硬件不支持相应的粒度，则会报错并终止。

错误码：

- PMEM2_E_GRANULARITY_NOT_SUPPORTED - granularity g is not a valid value.

#### 1.2.5. sharing

```cpp
struct pmem2_config;
enum pmem2_sharing_type {
	PMEM2_SHARED,
	PMEM2_PRIVATE,
};
int pmem2_config_set_sharing(struct pmem2_config *config, enum pmem2_sharing_type sharing);
```

pmem2_config_set_sharing() 函数配置对映射页面的写入行为和可见性。下面列出了可能的值：

- PMEM2_SHARED - 直接写入底层内存，使其**对同一内存区域的其他映射可见**。（默认）
- PMEM2_PRIVATE - 写入不会影响底层内存，并且对同一内存区域的其他映射不可见。（可以考虑使用这个，<font color="red">看能否提高性能</font>）

错误码：

- PMEM2_E_INVALID_SHARING_VALUE - sharing value is invalid.

## 2. source

### 2.1. 创建与销毁

```cpp
int pmem2_source_from_fd(struct pmem2_source *src, int fd);
int pmem2_source_from_handle(struct pmem2_source *src, HANDLE handle); /* Windows only */
int pmem2_source_delete(struct pmem2_source **src);
```

在 Linux 上，pmem2_source_from_fd() 函数验证文件描述符并实例化一个新的 *struct pmem2_source** 对象来描述数据源。

在 Windows 上，pmem2_source_from_fd() 函数将文件描述符转换为文件句柄（使用 _get_osfhandle()），并将其传递给 pmem2_source_from_handle()。默认情况下，_get_osfhandle() 在文件描述符无效的情况下调用 abort()，但这种行为可以通过 _set_abort_behavior() 和 SetErrorMode() 函数来抑制。有关 Windows CRT 错误处理的更多信息，请查看 MSDN 文档。

fd 必须以 O_RDONLY 或 O_RDWR 模式打开，但在 Windows 上它没有被验证。如果 fd 无效，则函数失败。

pmem2_source_from_handle() 函数验证句柄并实例化一个描述数据源的新 struct pmem2_source** 对象。如果 *handle 是 INVALID_HANDLE_VALUE，则函数失败。必须使用 GENERIC_READ 或 (GENERIC_READ | GENERIC_WRITE) 访问模式创建句柄。有关详细信息，请参阅 CreateFile() 文档。

pmem2_source_delete() 函数释放由 pmem2_source_from_fd() 或 pmem2_source_from_handle() 返回的 *src 并将 *src 设置为 NULL。如果 *src 为 NULL，则不执行任何操作。

警告：

- 在非DAX的Windows 卷上，使用映射时 fd/handle 必须保持打开状态。

也就是说，在linux上，创建文件映射后，可以提前关闭文件描述符。

### 2.2. 匿名内存页数据源

```cpp
int pmem2_source_from_anon(struct pmem2_source **src, size_t size);
```

创建由**匿名内存页**支持的数据源

pmem2_source_from_anon() 函数实例化一个描述匿名数据源的新struct pmem2_source 对象。使用此函数创建的映射不受任何文件的支持并且是零初始化的。

该函数的 size 参数定义匿名源的字节长度，由 pmem2_source_size(3) 返回。应用程序应设置此值，使其大于或等于使用匿名源创建的任何映射的大小。

映射的偏移值被忽略。

### 2.3. alignment 对齐方式

```cpp
int pmem2_source_alignment(const struct pmem2_source *source, size_t *alignment);
```

返回数据源的对齐方式：
pmem2_source_alignment() 函数检索 pmem2_map_new(3)函数运行成功 所需的偏移量和长度的对齐方式。对齐方式存储在 *alignment 中。

### 2.4. size

```cpp
int pmem2_source_size(const struct pmem2_source *source, size_t *size);
```

返回数据源的大小
pmem2_source_size() 函数检索存储在源中的文件描述符或句柄指向的文件的大小（以字节为单位），并将其放入 *size。

此函数是操作系统特定 API 的可移植替代品。在 Linux 上，它隐藏了设备 DAX 大小检测的古怪之处。

### 2.5. get fd/handle

```cpp
int pmem2_source_get_fd(const struct pmem2_source *src, int *fd);
```

pmem2_source_get_fd() 函数读取描述数据源的struct pmem2_source** 对象的文件描述符，并通过*fd 参数返回。

此函数仅适用于 Linux，在 Windows 上使用 pmem2_source_get_handle(3)。

```cpp
int pmem2_source_get_handle(const struct pmem2_source *src, HANDLE *h);
```

### 2.6. numa node

```cpp
int pmem2_source_numa_node(const struct pmem2_source *source, int *numa_node);
```

pmem2_source_numa_node() 函数检索给定数据源的 numa 节点。 **numa 节点可用于，例如，将线程固定到近内存核心**。 numa 节点存储在 *numa_node 中。它与 ndctl list -v 中显示为 numa_node 的值相同。

## 3. vm_reservation 虚拟内存预留

### 3.1. 创建与销毁

```cpp
struct pmem2_vm_reservation;
int pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv_ptr,
		void *addr, size_t size);
int pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv_ptr);
```

pmem2_vm_reservation_new() 函数在**调用进程的虚拟地址空间中**创建一个新的空白映射。Reservation作为给定大小的占位符，可以在其上映射源。

要使函数成功，**addr 必须与适当的分配粒度对齐或为 NULL，大小始终必须与适当的分配粒度对齐**。
（和mmap类似<https://www.cnblogs.com/houjun/p/4881090.html>，addr推荐使用NULL，让系统自己寻找一段空闲的虚拟地址空间，否则降低程序的可移植性，因为不同系统的可用地址范围不一样）

如果 pmem2_vm_reservation_new() 成功创建保留，它会实例化一个新的 struct pmem2_vm_reservation 对象来**描述保留**。指向该对象的指针通过 rsv_ptr 指针存储在用户提供的变量中。如果函数失败，将返回适当的错误值。有关可能的返回值列表，请参阅返回值。

通过 pmem2_vm_reservation_new() 函数实例化对象后，可以使用 pmem2_vm_reservation_delete() 函数销毁它。

pmem2_vm_reservation_delete() 函数销毁描述预留的对象并取消 struct pmem2_vm_reservation** 在初始化期间分配的虚拟内存区域的映射。要使删除函数成功，要求通过 *rsv_ptr 指针传递的保留不包含任何映射。

### 3.2. extend & shrink

扩展和收缩现有的虚拟内存预留。

```cpp
int pmem2_vm_reservation_extend(struct pmem2_vm_reservation *rsv, size_t size);
int pmem2_vm_reservation_shrink(struct pmem2_vm_reservation *rsv, size_t offset,
		size_t size);
```

pmem2_vm_reservation_extend() 函数按给定大小扩展现有的虚拟内存预留。为了使函数成功，**大小必须与适当的分配粒度对齐**。

如果 pmem2_vm_reservation_extend() 成功扩展预留，它会提供占位符虚拟内存范围，该范围从旧预留末尾的地址开始。对扩展前预留的映射将被保留。

pmem2_vm_reservation_shrink() 函数将预留收缩（释放）由偏移和大小指定的区域。为了使函数成功，大小和偏移变量必须与适当的分配粒度对齐。 offset和size组成的区域必须属于reservation，或者为空，并且需要覆盖reservation的起始或结束位置。不支持从中间收缩预留或收缩整个预留。

如果 pmem2_vm_reservation_shrink() 成功缩小预留，它将**释放**由偏移量和大小变量指定的占位符虚拟内存范围。保留收缩前对预留的映射。

如果这些函数中的任何一个失败，则保留将保持原样，并返回适当的错误值。

### 3.3. 设置config的vm_reservation

```cpp
struct pmem2_config;
struct pmem2_vm_reservation;
int pmem2_config_set_vm_reservation(struct pmem2_config *config,
		struct pmem2_vm_reservation *rsv, size_t rsv_offset);
```

指定内存映射到的虚拟内存区域。

pmem2_config_set_vm_reservation() 函数设置**映射期间要使用的虚拟内存预留和偏移量**。 rsv 应该已经初始化了。有关详细信息，请参阅 pmem2_vm_reservation_new(3)。 rsv_offset 标记映射到保留区域的偏移。

### 3.4. get address & size

```cpp
void *pmem2_vm_reservation_get_address(struct pmem2_vm_reservation *rsv);
```

pmem2_vm_reservation_get_address() 函数读取创建的虚拟内存预留的地址。 rsv 参数指向描述使用 pmem2_vm_reservation_new(3) 函数创建的预留的结构。

返回一个指向虚拟内存保留区的指针。

```cpp
size_t pmem2_vm_reservation_get_size(struct pmem2_vm_reservation *rsv);
```

返回虚拟保留区的大小。

### 3.5. vm中的map查找

```cpp
struct pmem2_map;
struct pmem2_vm_reservation;
int pmem2_vm_reservation_map_find(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len, struct pmem2_map **map_ptr);
int pmem2_vm_reservation_map_find_prev(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map, struct pmem2_map **prev_map);
int pmem2_vm_reservation_map_find_next(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map, struct pmem2_map **next_map);
int pmem2_vm_reservation_map_find_first(struct pmem2_vm_reservation *rsv,
		struct pmem2_map **first_map);
int pmem2_vm_reservation_map_find_last(struct pmem2_vm_reservation *rsv,
		struct pmem2_map **last_map);
```

映射按其虚拟地址空间位置的顺序插入到虚拟内存预留中。第一个映射代表保留中包含的虚拟地址空间中最早的映射，而最后一个映射代表最后一个。

pmem2_vm_reservation_map_find() 函数搜索存储在虚拟内存预留中，与reserv_offset 和len 变量定义的区间相交的最早的映射，并通过map_ptr 变量返回。

pmem2_vm_reservation_map_find_prev() 函数搜索提供的映射之前的映射，并通过提供的 prev_map 变量返回它。

pmem2_vm_reservation_map_find_next() 函数在提供的映射之后搜索下一个映射，并通过 next_map 变量返回它。

pmem2_vm_reservation_map_find_first() 函数搜索保留中的第一个映射并通过提供的 first_map 变量返回它。

pmem2_vm_reservation_map_find_last() 函数搜索保留中的最后一个映射，并通过提供的 last_map 变量返回它。

错误码：

- PMEM2_E_MAPPING_NOT_FOUND - no mapping found at the desirable location of the reservation

## 4. map

### 4.1. 创建与销毁

```cpp
int pmem2_map_new(struct pmem2_map **map_ptr, const struct pmem2_config *config,
		const struct pmem2_source *source);
```

pmem2_map_new() 函数在调用进程的虚拟地址空间中创建一个新的映射。此功能需要映射的配置config和数据源source。

或者，可以在配置config中设置的虚拟内存预留的偏移量处创建映射。有关详细信息，请参阅 pmem2_config_set_vm_reservation(3)。

要使映射成功，配置结构必须将**粒度参数**设置为适当的级别。有关更多详细信息，请参阅 pmem2_config_set_required_store_granularity(3) 和 libpmem2(7)。

如果 pmem2_map_new() 函数成功创建新映射，它会实例化一个描述映射的新 struct pmem2_map** 对象。指向这个新创建对象的指针存储在通过 *map_ptr 指针传递的用户提供的变量中。如果映射失败，map_ptr 指向的变量将包含一个 NULL 值并返回适当的错误值。有关可能返回值的列表，请参阅返回值。

所有通过 pmem2_map_new() 函数创建的 struct pmem2_map 对象都必须使用 pmem2_map_delete() 函数销毁。

```cpp
int pmem2_map_delete(struct pmem2_map **map_ptr);
```

销毁一个映射。

如果 pmem2_map_delete() 成功删除映射，它会释放描述它的 struct pmem2_map 对象并将 NULL 值写入 map_ptr。如果函数失败，map_ptr 变量和map对象本身将保持不变，并返回适当的错误值。有关可能返回值的列表，请参阅返回值。

**pmem2_map_delete() 函数不会取消（unmap）用户通过 pmem2_map_from_existing() 函数提供的映射**。在这种情况下，它只会释放 struct pmem2_map 对象。

错误码：

- PMEM2_E_MAPPING_NOT_FOUND - mapping was not found (it was already unmapped or pmem2_map state was corrupted)

### 4.2. 从已有的map创建

```cpp
int pmem2_map_from_existing(struct pmem2_map **map, const struct pmem2_source *src,
	void *addr, size_t len, enum pmem2_granularity gran);
```

从现有映射创建 pmem2_map 对象

pmem2_map_from_existing() 返回一个新的 struct pmem2_map** 用于用户提供的映射(**应该是普通的映射吧！测试时发现，如果addr区域已经创建过pmem2_map对象，则会报错**)。通过该函数从而允许用户使用 libpmem2(7) API来操作普通映射，而无需使用 pmem2_map_new(3) 来重新映射文件。映射由 *addr 和 len 定义。您必须将底层文件指定为 src，并定义此映射的粒度。有关更多详细信息，请参阅 pmem2_config_set_required_store_granularity(3) 和 libpmem2(7)。

对于 pmem2_map_from_existing(3) 函数创建的 pmem2_map 对象，pmem2_map_delete(3) 只会销毁该对象，但不会取消映射该对象描述的映射。

错误码：

- PMEM2_E_MAPPING_EXISTS - when contiguous region (addr, addr + len) is all ready registered by libpmem2

### 4.3. get address & size & granularity

```cpp
void *pmem2_map_get_address(struct pmem2_map *map);
```

返回已创建映射的起始地址

```cpp
size_t pmem2_map_get_size(struct pmem2_map *map);
```

返回已创建映射的大小

```cpp
enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};
enum pmem2_granularity pmem2_map_get_store_granularity(struct pmem2_map *map);
```

返回已创建映射的映射粒度

## 5. 持久化相关的函数 fn

### 5.1. persist

```cpp
typedef void (*pmem2_persist_fn)(const void *ptr, size_t size);

struct pmem2_map;

pmem2_persist_fn pmem2_get_persist_fn(struct pmem2_map *map);
```

pmem2_get_persist_fn() 函数返回一个指向函数的指针，该函数负责在映射所拥有的范围内**有效地持久化数据**。

使用 pmem2_persist_fn 持久化数据可确保数据在返回时持久存储。

**对 ptr 和 size 描述的范围没有对齐限制，但 pmem2_persist_fn 可能会根据需要扩展范围以满足平台对齐要求。**

pmem2_persist_fn **没有任何原子性或事务性**。给定范围内的任何未写入的存储都将被写入，**但某些存储可能已经根据正常的缓存逐出/替换策略被写入**。正确编写的代码不能依赖于直到 pmem2_persist_fn 被调用才能持久化的存储——因为它们可以在调用 pmem2_persist_fn 之前的任何时间变得持久化。

如果两个（或更多）映射共享同一个 pmem2_persist_fn 并且它们彼此相邻，则可以安全地为跨越这些映射的范围调用此函数。

pmem2_persist_fn **内部执行两个操作**：

- 内存刷新 (pmem2_get_flush_fn(3))，可以由 CPU 与其他刷新重新排序
- 排干drain（pmem2_get_drain_fn（3）），确保此操作之前的刷新不会在它之后重新排序（相当于fence）

所以，下面的代码：

```cpp
pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
persist_fn(addr, len);
```

等同于：

```cpp
pmem2_flush_fn flush_fn = pmem2_get_flush_fn(map);
pmem2_drain_fn drain_fn = pmem2_get_drain_fn(map);

flush_fn(addr, len);
drain_fn();
```

**高级应用程序可能希望刷新多个不连续的区域并仅执行一次排放操作。**

同一映射的 pmem2_get_persist_fn() 始终返回相同的函数。这意味着缓存其返回值是安全的。然而，**这个函数非常便宜（因为它返回一个预先计算的值）**，所以缓存可能没有必要。

### 5.2. flush & drain

```cpp
typedef void (*pmem2_flush_fn)(const void *ptr, size_t size);

struct pmem2_map;

pmem2_flush_fn pmem2_get_flush_fn(struct pmem2_map *map);
```

返回一个指向函数的指针，该函数负责有效地刷新映射所拥有范围内的数据。

使用 pmem2_flush_fn 刷新数据并不能保证数据在返回时持久存储。要获得此保证，应用程序应使用持久操作（请参阅 pmem2_get_persist_fn(3)）或通过排放drain操作跟随在 pmem2_flush_fn 后（请参阅 pmem2_get_drain_fn(3)）。（但在具有ADR的机器上，只要将Cacheline flush，就可以保证持久化，只是还存在乱排序的问题，仍需要drain）

**ptr 和 size 描述的范围没有对齐限制，但 pmem2_flush_fn 可能会根据需要扩展范围以满足平台对齐要求。**

> pmem2_flush_fn 没有任何原子性或事务性。给定范围内的任何未写入的存储都将被写入，但某些存储可能已经根据正常的缓存逐出/替换策略被写入。正确编写的代码不能依赖于等待直到 pmem2_flush_fn 被调用才能刷新的存储——它们可以在调用 pmem2_flush_fn 之前的任何时间被刷新。
>
> 如果两个（或更多）映射共享同一个 pmem2_flush_fn 并且它们彼此相邻，则可以安全地为跨越这些映射的范围调用此函数。

```cpp
typedef void (*pmem2_drain_fn)(void);

struct pmem2_map;

pmem2_drain_fn pmem2_get_drain_fn(struct pmem2_map *map);
```

pmem2_get_drain_fn() 函数返回一个指针，该函数负责在 map 拥有的范围内有效地排出flush（请参阅 pmem2_get_flush_fn(3)）。在这种情况下，排空drain意味着确保此操作之前的刷新不会在其之后重新排序。虽然严格来说并非如此，但可以将排空视为等待之前的flush完成。

如果两个（或更多）映射共享同一个排水函数，则为属于这些映射的所有刷新调用一次此函数是安全的。

<font color="red">测试发现，即使对于两个不同文件的pmem2 map对象，它们获取出来的pmem2_drain_fn函数指针的值相同。猜测，这些函数都是预先编写在库中，然后根据map的类型、机器的环境等等因素，返回对应的函数指针。因此，对于相同的map类型（比如粒度相同），可以使用同一个drain函数。</font>查看源码可以发现，持久化相关的函数都只与粒度有关。

### 5.3. deep_flush

高度可靠的持久内存同步（直接持久化到介质）

```cpp
int pmem2_deep_flush(struct pmem2_map *map, void *ptr, size_t size)
```

pmem2_deep_flush() 函数强制将map中 [ptr, ptr+len) 范围内的任何更改持久存储在软件可用的**最可靠**持久域中。特别是，在受支持的平台上，这使代码在电源故障 (ADR/eADR) 时不依赖于自动缓存或 WPQ（写挂起队列）刷新（即不依赖于ADR，直接flush到介质）。

**由于此操作通常比常规持久化要昂贵得多，因此应谨慎使用**。通常，应用程序应该只使用此函数作为硬件故障的预防措施，例如，在检测由不安全关闭引起的静默数据损坏的代码中（更多信息请参见 libpmem2_unsafe_shutdown(7)）。

### 5.4. 内存 copy & move & set

```cpp
typedef void *(*pmem2_memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*pmem2_memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*pmem2_memset_fn)(void *pmemdest, int c, size_t len,
		unsigned flags);

struct pmem2_map;

pmem2_memmove_fn pmem2_get_memmove_fn(struct pmem2_map *map);
pmem2_memset_fn pmem2_get_memset_fn(struct pmem2_map *map);
pmem2_memcpy_fn pmem2_get_memcpy_fn(struct pmem2_map *map);
```

获取提供优化的复制到持久内存的函数

pmem2_get_memmove_fn(), pmem2_get_memset_fn(), pmem2_get_memcpy_fn() 函数返回一个指向负责高效存储和刷新映射数据的函数指针。

pmem2_memmove_fn()、pmem2_memset_fn() 和 pmem2_memcpy_fn() 函数提供与其同名 memmove(3)、memcpy(3) 和 memset(3) 相同的内存复制功能，**并确保结果在返回之前已刷新到持久性**（除非使用了 PMEM2_F_MEM_NOFLUSH 标志）。

例如，对于一下代码：

```cpp
memmove(dest, src, len);
        pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
        persist_fn(dest, len);
```

等同于

```cpp
pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);
        memmove_fn(dest, src, len, 0);
```

与 libc 实现不同，libpmem2 函数保证如果**目标缓冲区**地址和长度是 8 字节对齐的，那么所有存储都将使用至少 8 字节存储指令执行。这意味着可以通过单个 memmove_fn 调用安全地替换紧随persist_fn 的一系列8 字节存储。

上述所有函数的 flags 参数具有相同的含义。它可以是 0 或以下一个或多个标志的按位或：

- PMEM2_F_MEM_NODRAIN - 修改行为以跳过最后的 pmem2_drain_fn 步骤。这允许应用程序优化多个范围被复制到持久内存的情况，然后是对 pmem2_drain_fn 的单个调用。以下示例说明了在将多个内存范围复制到 pmem 时如何使用此标志来避免多次调用 pmem2_drain_fn：

  ```cpp
  pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
  pmem2_drain_fn drain_fn = pmem2_get_drain_fn  (map);
  
  /* ... write several ranges to pmem ... */
  memcpy_fn(pmemdest1, src1, len1,   PMEM2_F_MEM_NODRAIN);
  memcpy_fn(pmemdest2, src2, len2,   PMEM2_F_MEM_NODRAIN);
  
  /* ... */
  
  /* wait for any pmem stores to drain from   HW buffers */
  drain_fn();
  ```

- PMEM2_F_MEM_NOFLUSH - 不要flush任何东西。这隐含着 PMEM2_F_MEM_NODRAIN。使用此标志仅在其后跟随刷新数据的函数时才有意义。

其余的标志说明操作应该如何完成，并且只是提示。

- PMEM2_F_MEM_NONTEMPORAL - 使用non-temporal指令。此标志与 PMEM2_F_MEM_TEMPORAL 互斥。在 x86_64 上，此标志与 PMEM2_F_MEM_NOFLUSH 互斥。
- PMEM2_F_MEM_TEMPORAL - 使用temporal指令。此标志与 PMEM2_F_MEM_NONTEMPORAL 互斥。
- PMEM2_F_MEM_WC - 使用写组合模式。此标志与 PMEM2_F_MEM_WB 互斥。在 x86_64 上，此标志与 PMEM2_F_MEM_NOFLUSH 互斥。
- PMEM2_F_MEM_WB - 使用回写模式（cache回写）。此标志与 PMEM2_F_MEM_WC 互斥。在 x86_64 上，这是 PMEM2_F_MEM_TEMPORAL 的别名。

使用无效的标志组合具有未定义的行为。

如果没有上述任何标志，libpmem2 将尝试根据数据大小猜测最佳策略。有关详细信息，请参阅 libpmem2(7) 中的 PMEM_MOVNT_THRESHOLD 描述。

> 关于non-temporal： Load/Store Non-temporal Pair指令提供给memory system一个暗示（hint）：这个访问是non-temporal或者流式（streaming）的，在短时间内很可能不会再次访问，要访问的数据不需要缓存到cache中。这仅仅是一个暗示，这个指令允许preload或者和其他写合并在一起形成bulk transfers。（即短时间内不会再次访问）

注意：如果两个（或更多）映射共享相同的 pmem2_memmove_fn、pmem2_memset_fn、pmem2_memcpy_fn 并且它们彼此相邻，则可以安全地为跨越这些映射的范围调用这些函数。

## 6. 错误处理

```cpp
void pmem2_perror(const char *format, ...);
```

将描述性错误消息打印到 stderr

**pmem2_perror() 函数在标准错误流上生成一条消息，描述在库调用期间遇到的最后一个错误。**

pmem2_perror() 接受可变数量的参数。首先，打印参数字符串格式 - 类似于 printf(3)，后跟一个冒号和一个空格。然后从 pmem2_errormsg() 中检索到一条错误消息和一个换行符。要查看错误消息是如何生成的，请参阅 pmem2_errormsg(3)。

```cpp
const char *pmem2_errormsg(void);
```

返回上一个错误消息

如果在调用 libpmem2(7) 函数期间检测到错误，应用程序可以从 pmem2_errormsg() 检索描述失败原因的错误消息。**错误消息缓冲区是线程本地的**；在一个线程中遇到的错误不会影响它在其他线程中的值。任何库函数都不会清除缓冲区；仅当对 libpmem2(7) 函数的紧接调用的返回值指示错误时，其内容才有意义。**应用程序不得修改或释放错误消息字符串**。对其他库函数的后续调用可能会修改之前的消息。

## 不安全关机处理

相关概念参考文档：[libpmem2_unsafe_shutdown](/libpmem2/doc/libpmem2_unsafe_shutdown.md)

### device id & usc

```cpp
int pmem2_source_device_id(const struct pmem2_source *source, char *id, size_t *len);
```

返回设备的唯一标识符：
pmem2_source_device_id() 函数检索支持数据源的所有 NVDIMM 的唯一标识符。该功能有两种操作模式：

- 如果 *id 为 NULL，函数计算存储 *source 设备标识符所需的缓冲区长度并将此长度放入 *len。支持数据源的硬件设备越多，长度越长。
- 如果 *id 不为 NULL，则它必须指向一个长度为 *len 的缓冲区，该缓冲区由先前对该函数的调用提供。成功后， pmem2_source_device_id() 将存储支持数据源的所有硬件设备的唯一标识符。

有关如何使用唯一标识符**检测不安全关机**的详细信息，请参阅 libpmem2_unsafe_shutdown(7) 手册页。

```cpp
int pmem2_source_device_usc(const struct pmem2_source *source, uint64_t *usc);
```

返回设备不安全关机计数器的值

pmem2_source_device_usc() 函数检索支持数据源的所有硬件设备的不安全关闭计数 (USC) 值的总和，并将其存储在 *usc 中。

有关如何正确使用此信息的详细说明，请参阅libpmem2_unsafe_shutdown(7)。

### badblock context

```cpp
struct pmem2_source;
struct pmem2_badblock_context;

int pmem2_badblock_context_new(
		struct pmem2_badblock_context **bbctx,
		const struct pmem2_source *src);

void pmem2_badblock_context_delete(
		struct pmem2_badblock_context **bbctx);
```

pmem2_badblock_context_new() 函数实例化了一个新的（不透明的）坏块上下文结构 pmem2_badblock_context，用于读取和清除坏块（通过 pmem2_badblock_next() 和 pmem2_badblock_clear()）。该函数通过*bbctx 中的指针返回坏块上下文。

使用从作为第一个参数 (src) **给出的源**读取的值初始化新的坏块上下文结构。

**坏块是不可纠正的介质错误** - 由于永久性物理损坏而无法访问或不可写入的存储介质的一部分。在内存映射 I/O 的情况下，如果进程尝试访问（读取或写入）损坏的块，它将被 SIGBUS 信号终止。

pmem2_badblock_context_delete() 函数释放 pmem2_badblock_context_new() 返回的 *bbctx 并将 *bbctx 设置为 NULL。如果 *bbctx 为 NULL，则不执行任何操作。

<font color="red">它在 Windows 上不受支持。</font>

错误码：

- PMEM2_E_NOSUPP - on Windows or when the OS does not support this functionality
- 等等

### badblock next & clear

```cpp
struct pmem2_badblock;
struct pmem2_badblock_context;

int pmem2_badblock_next(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);
```

pmem2_badblock_next() 函数读取给定坏块上下文 *bbctx 的下一个坏块。

它在 Windows 上不受支持。

错误码：

- PMEM2_E_NO_BAD_BLOCK_FOUND - 对于给定的坏块上下文 *bbctx，没有更多坏块，在这种情况下 *bb 未定义。
- PMEM2_E_NOSUPP - 在 Windows 上或操作系统不支持此功能时

```cpp
struct pmem2_badblock;
struct pmem2_badblock_context;

int pmem2_badblock_clear(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);
```

pmem2_badblock_clear() 函数清除给定的 *bb 坏块。

这意味着 pmem2_badblock_clear() 函数取消映射坏块并映射新的、健康的块来代替坏块。新块被归零。**坏块的内容丢失**。

它在 Windows 上不受支持。
