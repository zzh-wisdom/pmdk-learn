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

要获得正确的数据**刷新功能**，请使用：pmem2_get_flush_fn(3)、pmem2_get_persist_fn(3) 或 pmem2_get_drain_fn(3)。
要获得**复制到持久内存的正确功能**，请使用映射 getter：pmem2_get_memcpy_fn(3)、pmem2_get_memset_fn(3)、pmem2_get_memmove_fn(3)。

libpmem2 API 还提供对坏块（badblock）和不安全关闭状态处理的支持。

为了读取或清除坏块，提供了以下函数：pmem2_badblock_context_new(3)、pmem2_badblock_context_delete(3)、pmem2_badblock_next(3) 和 pmem2_badblock_clear(3)。

为了处理应用程序中的不安全关机，提供了以下函数：pmem2_source_device_id(3)、pmem2_source_device_usc(3)。
有关不安全关机检测和不安全关机计数的更多详细信息，可以在 libpmem2_unsafe_shutdown(7) 手册页中找到。

## 粒度

**libpmem2 库引入了粒度概念**，通过它您可以轻松区分与**断电保护域**相关的应用程序可用的不同级别的存储性能能力。数据到达该受保护域的方式因平台和存储设备功能而异。

传统的块存储设备（SSD、HDD）必须使用系统 API 调用，例如 Linux 上的 msync()、fsync() 或 Windows 上的 FlushFileBuffers()、FlushViewOfFile() 来可靠地写入数据。调用这些函数以**页面粒度**将数据刷新到介质。在 libpmem2 库中，这种类型的刷新行为称为 PMEM2_GRANULARITY_PAGE。

在支持持久内存的系统中，断电保护域可能涵盖不同的资源集：只有内存控制器或同时包含内存控制器和 CPU 缓存。为此，libpmem2 区分了两种类型的持久内存粒度：PMEM2_GRANULARITY_CACHE_LINE 和 PMEM2_GRANULARITY_BYTE。

如果断电保护域仅覆盖内存控制器，则必须flush CPU 相应的缓存行，以便将数据视为持久性。这种粒度类型称为PMEM2_GRANULARITY_CACHE_LINE。根据架构，有不同类型的机器指令可用于刷新缓存行（例如，CLWB、CLFLUSHOPT、和CLFLUSH用于 Intel x86_64 架构）。**通常，为了确保存储的顺序，此类指令必须后跟一个屏障（例如，SFENCE）**。

第三种粒度 PMEM2_GRANULARITY_BYTE 适用于断电保护域涵盖内存控制器和 CPU 缓存的平台。在这种情况下，不再需要缓存刷新指令，平台本身保证了数据的持久性。但是顺序的保证可能仍然需要障碍。

该库在 pmem2_granularity 枚举中声明了这些粒度级别，应用程序必须在 pmem2_config 中将其设置为适当的级别才能成功映射。
软件应将此配置参数设置为最准确地表示目标硬件特性和应用程序存储模式的值。例如，在驻留在 SSD 或 PMEM 上的大型逻辑页面上运行的数据库存储引擎应将此值设置为 PMEM2_GRANULARITY_PAGE。该库将创建新映射粒度低于或等于请求粒度的映射。例如，可以为所需的粒度 PMEM2_GRANULARITY_PAGE 创建具有PMEM2_GRANULARITY_CACHE_LINE 的映射，但反之则不然。

## 警告

libpmem2 依赖于从主线程调用的库析构函数。因此，所有可能触发销毁的函数（例如 dlclose(3)）都应该在主线程中调用。否则，与该线程关联的某些资源可能无法正确清理。

## 环境

libpmem2 可以根据以下环境变量更改其默认行为。**这些主要用于测试，通常不需要**。

- PMEM2_FORCE_GRANULARITY=val

将此环境变量设置为 val 会强制 libpmem2 使用特定于强制粒度和跳过粒度自动检测机制的持久方法。粒度的概念在上面的粒度部分进行了描述。此变量旨在用于**库测试期间**。

val 参数接受以下文本值：

- BYTE - force byte granularity.
- CACHE_LINE - force cache line granularity.
- PAGE - force page granularity.

上面列出的粒度值不区分大小写。

> 注意： PMEM2_FORCE_GRANULARITY 的值不会在库初始化时查询（和缓存），而是在每次 pmem2_map_new(3) 调用期间读取。

这意味着 PMEM2_FORCE_GRANULARITY 仍然可以由程序设置或修改，直到第一次尝试映射文件。

- PMEM_NO_CLWB=1

将此环境变量设置为 1 会强制 libpmem2 永远不会在 Intel 硬件上发出 CLWB 指令，而是回退到该硬件上的其他缓存刷新指令（CLFLUSHOPT 或 CLFLUSH）。如果没有此设置，libpmem2 将始终使用 CLWB 指令在支持此指令的平台上刷新处理器缓存。此变量旨在用于库测试期间，但在使用 CLWB 对性能有负面影响的一些极少数情况下可能需要。

- PMEM_NO_CLFLUSHOPT=1

将此环境变量设置为 1 会强制 libpmem2 永远不会在 Intel 硬件上发出 CLFLUSHOPT 指令，而是回退到 CLFLUSH 指令。如果没有此环境变量，libpmem2 将始终使用 CLFLUSHOPT 指令在支持该指令但 CLWB 不可用的平台上刷新处理器缓存。此变量旨在用于库测试期间。

- PMEM_NO_MOVNT=1

将此环境变量设置为 1 会强制 libpmem2 从不使用 Intel 硬件上的非临时（non-temporal）move指令。如果没有这个环境变量，libpmem2 将使用非临时指令将更大的范围复制到支持这些指令的平台上的持久内存。此变量旨在用于库测试期间。

- PMEM_MOVNT_THRESHOLD=val

此环境变量允许覆盖 pmem2_memmove_fn 操作的最小长度，为此（大于该值时）libpmem2 使用非临时移动指令。将此环境变量设置为 0 会强制 libpmem2 始终使用非临时移动指令（如果可用）。如果 PMEM_NO_MOVNT 设置为 1，则无效。此变量旨在用于库测试期间。

## DEBUGGING

开发系统上通常有两个版本的 libpmem2。使用 -lpmem2 选项链接程序时访问的普通版本针对性能进行了优化。该版本会跳过影响性能的检查，并且从不记录任何跟踪信息或执行任何运行时断言。

当程序使用 /usr/lib/pmdk_debug 下的库时访问的 libpmem2 的第二个版本则包含运行时断言和跟踪点。访问调试版本的典型方法是根据需要将环境变量 LD_LIBRARY_PATH 设置为 /usr/lib/pmdk_debug 或 /usr/lib64/pmdk_debug。使用以下环境变量控制调试输出。这些变量对库的非调试版本没有影响。

- PMEM2_LOG_LEVEL

PMEM2_LOG_LEVEL 的值在库的调试版本中启用跟踪点，如下所示：

- 0 - 这是未设置 PMEM2_LOG_LEVEL 时的默认级别。在此级别不会发出日志消息。
- 1 - 除了像往常一样返回基于 errno 的错误之外，还会记录有关检测到的任何错误的其他详细信息。可以使用 pmem2_errormsg() 检索到相同的信息。
- 2 - 记录基本操作的跟踪。
- 3 - 在库中启用大量的函数调用跟踪。
- 4 - 启用可能仅对 libpmem2 开发人员有用的大量且相当模糊的跟踪信息。

除非设置了 PMEM2_LOG_FILE，否则调试输出将写入 stderr。

- PMEM2_LOG_FILE

指定应写入所有日志信息的文件的名称。如果名称的最后一个字符是“-”，则在创建日志文件时将当前进程的 PID 附加到文件名。
如果未设置 PMEM2_LOG_FILE，则输出将写入 stderr。

## EXAMPLE

以下示例使用 libpmem2 flush对原始内存映射持久内存所做的更改。

> 警告：在本示例中，pmem2_get_persist_fn(3) 调用的持久化没有任何事务性。中断程序可能会导致对 pmem 的部分写入。使用诸如 libpmemobj(7) 之类的事务库来避免更新中断。

```cpp
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

	if (pmem2_map(cfg, src, &map)) {
		pmem2_perror("pmem2_map");
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	size_t size = pmem2_map_get_size(map);

	strcpy(addr, "hello, persistent memory");

	persist = pmem2_get_persist_fn(map);
	persist(addr, size);

	pmem2_unmap(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);

	return 0;
}
```

上面的例子在[这里](https://pmem.io/pmdk/libpmem2/)详细描述。

