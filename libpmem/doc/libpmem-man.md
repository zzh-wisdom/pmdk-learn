# libpmem man首页

- [1. 描述](#1-描述)
- [2. 警告](#2-警告)
- [3. 库 API 版本](#3-库-api-版本)
- [4. ENVIRONMENT](#4-environment)
- [5. 调试和错误处理](#5-调试和错误处理)
- [6. EXAMPLE](#6-example)

```cpp
#include <libpmem.h>
cc ... -lpmem
```

## 1. 描述

libpmem 为使用直接访问存储 (DAX) 的应用程序提供低级持久内存 (pmem) 支持，该存储支持加载/存储访问，而无需从块存储设备中分页块。某些类型的非易失性内存 DIMM (NVDIMM) 提供这种类型的字节可寻址存储访问。持久内存感知文件系统通常用于公开对应用程序的直接访问。从这种类型的文件系统映射文件的内存会导致对 pmem 的加载/存储、非分页访问。

此库适用于直接使用持久内存的应用程序，无需任何库提供的事务或内存分配的帮助。可以使用基于 libpmem 构建的更高级别的库，并且推荐用于大多数应用程序，请参阅：

- libpmemobj(7)，一个通用的持久内存API，提供对可变大小对象的内存分配和事务操作。
- libpmemblk(7)，提供具有原子更新的固定大小块的 pmem 常驻数组。
- libpmemlog(7)，提供驻留在 pmem 的日志文件。

在正常使用情况下，libpmem 永远不会打印消息或故意导致进程退出。唯一的例外是调试信息（启用时），如下面所说的的调试和错误处理中所述。

## 2. 警告

libpmem依赖于从主线程调用的库析构器。因此，所有可能触发销毁的函数（如 dlclose （3）应在主线程中调用。否则，与该线程相关的某些资源可能无法正确清理。

## 3. 库 API 版本

本节描述了库 API 是如何进行版本控制的，从而允许应用程序与不断发展的 API 一起工作。

pmem_check_version() 函数用于确定安装的 libpmem 是否支持应用程序所需的库 API 版本。最简单的方法是让应用程序提供编译时版本信息，由<libpmem.h>中的定义提供，如下所示：

```cpp
reason = pmem_check_version(PMEM_MAJOR_VERSION,
                            PMEM_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

**主版本号中的任何不匹配都被视为失败**，但具有较新次(minor)版本号的库将通过此检查，因为增加次版本意味着向后兼容。

应用程序还可以通过检查引入接口的版本来专门检查接口是否存在。这些版本在此手册页中记录如下：除非另有说明，否则此处描述的所有接口都在库的 1.0 版中可用。 1.0 版之后添加的接口将包含相关的介绍文本，位于在本手册的版本x.y中描述该功能的章节中。

当 pmem_check_version() 执行的版本检查成功时，返回值为 NULL。否则，返回值是一个描述版本检查失败原因的静态字符串。 **pmem_check_version() 返回的字符串不得修改或释放**。

## 4. ENVIRONMENT

libpmem 可以根据以下环境变量更改其默认行为。这些主要用于测试，通常不需要。

- PMEM_IS_PMEM_FORCE=val

如果 val 为 0（零），则 pmem_is_pmem(3) 将始终返回 false。将 val 设置为 1 会导致 pmem_is_pmem(3) 始终返回 true。此变量主要用于测试，但可用于强制系统上的 pmem 行为，在该系统中，由于某种原因，某些 pmem 范围无法检测为 pmem。

> 注意：与其他变量不同，PMEM_IS_PMEM_FORCE 的值不会在库初始化时查询（和缓存），而是在第一次调用 pmem_is_pmem(3) 时查询。这意味着在 libpmemlog(7)、libpmemblk(7) 和 libpmemobj(7) 的情况下，程序仍可能设置或修改 PMEM_IS_PMEM_FORCE，直到第一次尝试创建或打开持久内存池。

- PMEM_NO_CLWB=1

将此环境变量设置为 1 强制 libpmem永远不会在英特尔硬件上发出 CLWB 指令，而是返回到使用其他缓存冲洗指令（CLFLUSHOPT 或英特尔硬件上的 CLFLUSH）。如果没有此环境变量，libpmem 将始终使用 CLWB 指令在支持该指令的平台上冲洗处理器缓存。此变量用于库测试期间，但对于某些罕见情况下，使用 CLWB 对性能有负面影响时可能需要此变量。

- PMEM_NO_CLFLUSHOPT=1

将此环境变量设置为 1 个强制 libpmem永远不会在英特尔硬件上发出 CLFLUSHOPT 指令，而是回到使用 CLFLUSH 指令。如果没有此环境变量，libpmem 将始终使用 CLFLUSHOPT 指令在支持该指令但在 CLWB 不可用的平台上冲洗处理器缓存。此变量用于库测试期间。

- PMEM_NO_FLUSH=1

将此环境变量设置为 1 强制大多数 libpmem 函数切勿在英特尔硬件上发布任何 CLFLUSH、CLFLUSHOPT 或 CLWB 指令。唯一的例外是pmem_deep_flush（3）和pmem_deep_persist（3）功能。

- PMEM_NO_FLUSH=0

将此环境变量设置为 0，即使pmem_has_auto_flush（3）函数返回true且平台支持在电源丢失或系统崩溃时冲洗处理器缓存，也始终使用 CLFLUSH、CLFLUSHOPT 或 CLWB 指令之一，以始终冲洗 CPU 缓存。

- PMEM_NO_MOVNT=1

将此环境变量设置为 1 强制 libpmem 永远不要在英特尔硬件上使用non-temporal move指令。如果没有此环境变量，对于将较大的范围，libpmem 将使用non-temporal指令在支持的平台上复制到持久内存。此变量用于库测试期间。

- PMEM_MOVNT_THRESHOLD=val

此环境变量允许pmem_memmove_persist （3） 操作的最小覆盖长度，为此 libpmem 使用non-temporal移动指令。将此环境变量设置为 0 则强制 libpmem在可用时始终使用非临时移动指令。如果PMEM_NO_MOVNT设置为 1，则该变量不会产生任何影响。此变量用于库测试期间。

- PMEM_MMAP_HINT=val

此环境变量允许pmem_map_file（）操作覆盖提示地址。**如果设置，它还禁止映射地址的随机化**。此变量用于库测试和调试期间。将其设置为一些相当大的值（比如0x10000000000）则**很可能导致在指定的地址（如果该地址没有被使用）或者给定地址之后的第一个未使用的区域映射文件**，而无需添加任何随机偏移。调试时，这可以根据文件中的偏移来更轻松地计算持久存储块的实际地址。在 libpmemobj 的情况下，它简化了将持久对象标识符 （OID） 转换为指向对象的直接指针。

> 注意：设置此环境变量会影响所有 PMDK 库，禁用映射地址随机化，并导致指定地址用作map放置位置的提示。

## 5. 调试和错误处理

如果在调用 libpmem 功能时检测到错误，应用程序可能会从pmem_errormsg（）检索描述故障原因的错误消息。此功能将指针返回到静态缓冲区，其中包含记录当前线程的最后一条错误消息。如果设置errno，错误消息可能包括由 strerror 返回的相应错误代码的描述 （3）。错误消息缓冲区是线程本地的：在一个线程中遇到的错误不会影响其在其他线程中的value。缓冲区永远不会被任何库功能清除：其内容仅在立即调用到 libpmem 函数的返回值显示错误时才具有重大意义。**应用程序不得修改或释放错误消息字符串。随后调用其他库函数可能会修改以前的消息**。

开发系统上通常提供两个版本的 libpmem。使用 -lpmem 选项链接程序时访问的正常版本会根据性能进行优化。该版本跳过影响性能的检查，从不记录任何跟踪信息或执行任何运行时间断言。

当程序使用 /usr/lib/pmdk_debug 下的库时访问的第二个版本的 libpmem 包含运行时间断言和跟踪点。访问调试版本的典型方法是酌情将环境变量LD_LIBRARY_PATH设置为 /usr/lib/pmdk_debug 或 /usr/lib64/pmdk_debug。使用以下环境变量控制调试输出。**这些变量对库的非调试版本没有影响**。

- PMEM_LOG_LEVEL

PMEM_LOG_LEVEL值可在库的调试版本中启用示踪点，具体如下：

- 0 - 这是未设置PMEM_LOG_LEVEL时的默认级别。此级别不会发送日志消息
- 1 - 除了像往常一样返回基于错误的错误外，还记录了检测到的任何错误的其他详细信息。使用pmem_errormsg（）检索到相同的信息。
- 2 - 记录基本操作的痕迹。
- 3 - 在库中启用非常详细的函数调用跟踪。
- 4 - 启用大量且相当模糊的跟踪信息，这些信息可能仅对 libpmem 开发人员有用。

除非设置PMEM_LOG_FILE，否则调试输出将写到stderr。

- PMEM_LOG_FILE

指定所有记录信息都应写到的文件的名称。如果名称中的最后一个字符是"-"，则创建日志文件时，当前进程的 PID 将附加到文件名称中。如果不设置PMEM_LOG_FILE，输出将写到stderr。

## 6. EXAMPLE

以下示例使用 libpmem 来flush对原始、内存映射持久内存的更改。

> 警告：本示例中pmem_persist （3） 或pmem_msync （3） 调用没有任何事务性。中断程序可能导致部分写到 pmem。使用事务库（如 libpmobj （7） 以避免撕裂更新。

```cpp
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

/* using 4k of pmem for this example */
#define PMEM_LEN 4096

#define PATH "/pmem-fs/myfile"

int
main(int argc, char *argv[])
{
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem;

	/* create a pmem file and memory map it */

	if ((pmemaddr = pmem_map_file(PATH, PMEM_LEN, PMEM_FILE_CREATE,
			0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}

	/* store a string to the persistent memory */
	strcpy(pmemaddr, "hello, persistent memory");

	/* flush above strcpy to persistence */
	if (is_pmem)
		pmem_persist(pmemaddr, mapped_len);
	else
		pmem_msync(pmemaddr, mapped_len);

	/*
	 * Delete the mappings. The region is also
	 * automatically unmapped when the process is
	 * terminated.
	 */
	pmem_unmap(pmemaddr, mapped_len);
}
```
