# libpmem api

- [1. pmem map](#1-pmem-map)
- [2. 持久化操作](#2-持久化操作)
- [3. 内存操作](#3-内存操作)
- [4. 其他](#4-其他)

v1.11

## 1. pmem map

```cpp
#include <libpmem.h>

int pmem_is_pmem(const void *addr, size_t len);
void *pmem_map_file(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
int pmem_unmap(void *addr, size_t len);
```

大多数 pmem 感知应用程序将利用更高级别的库，这些库减轻了应用程序直接调用 libpmem 的需要。希望直接访问原始内存映射持久性（通过 mmap(2)）并希望负责将存储刷新到持久性的应用程序开发人员会发现本节中描述的函数是最常用的。

**pmem_is_pmem() 函数检测整个范围 [addr, addr+len) 是否由持久内存组成**。使用源自不同于 pmem_map_file() 的源的内存范围调用此函数是未定义的（**即跨多个map映射范围，函数调用未定义**）。 pmem_is_pmem() 的实现需要**大量的工作**来确定给定的范围是否完全是持久内存。为此，最好在第一次遇到内存范围时调用 pmem_is_pmem() 一次，保存结果，并使用保存的结果来确定 pmem_persist(3) 或 msync(2) 是否适合将更改刷新到持久性。每次将更改刷新到持久性时调用 pmem_is_pmem() 将不会表现良好。

pmem_map_file() 函数为文件创建新的读/写映射。如果标志中未指定 **PMEM_FILE_CREATE**，**则映射整个现有文件路径，len 必须为零，并且忽略 mode**。**否则，将按照指定的flag和mode的打开或创建路径，并且 len 必须非零**。 pmem_map_file() 使用 mmap(2) 映射文件，但它更有可能需要额外的步骤来使大页面映射。

成功时，pmem_map_file() 返回一个指向映射区域的指针。如果mapped_lenp 不为NULL，则映射的长度存储在*mapped_lenp 中。如果 is_pmemp 不为 NULL，则指示映射文件是否为实际 pmem、或者说是否必须使用 msync() 来刷新映射范围的写入的的标志，将存储到 *is_pmemp 中。

flags 参数是 0 或以下一个或多个文件创建标志的按位或：

- PMEM_FILE_CREATE - 如果文件不存在，则创建名为 path 的文件。 len 必须非零并指定要创建的文件的大小。**如果文件已经存在，它将被扩展或截断为 len**。然后使用 posix_fallocate(3) 将新文件或现有文件完全分配到大小为 len。 mode 指定在创建新文件时使用的模式（请参阅 creat(2)）。

其余标志在指定 PMEM_FILE_CREATE 时修改 pmem_map_file() 的行为。

- PMEM_FILE_EXCL - 如果与 PMEM_FILE_CREATE 一起指定，**并且路径已经存在**，那么 pmem_map_file() 将失败并显示 EEXIST。否则，与 open(2) 上的 O_EXCL 具有相同的含义，通常未定义。
- PMEM_FILE_SPARSE - 当与 PMEM_FILE_CREATE 一起指定时，使用 ftruncate(2) 创建一个稀疏 (holey) 文件，而不是使用 posix_fallocate(3) 分配它。否则忽略。
- PMEM_FILE_TMPFILE - **为未命名的临时文件创建映射**。必须用 PMEM_FILE_CREATE 指定。 len 必须非零，**模式被忽略（临时文件总是使用模式 0600 创建）**，并且路径必须指定一个现有的目录名称。如果底层文件系统支持O_TMPFILE，则在包含目录路径的文件系统中创建未命名的临时文件；如果还指定了 PMEM_FILE_EXCL，则临时文件可能不会随后链接到文件系统（请参阅 open(2)），否则，文件将在路径中创建并立即取消链接。

**该路径可以指向设备 DAX。在这种情况下，只有 PMEM_FILE_CREATE 和 PMEM_FILE_SPARSE 标志有效，但它们都被忽略**。对于设备 DAX 映射，**len 必须等于 0 或设备的确切大小**。

要删除使用 pmem_map_file() 创建的映射，请使用 pmem_unmap()。

pmem_unmap() 函数删除指定地址范围的所有映射，并导致后来对该范围内地址的引用会产生无效的内存引用。它将使用参数 addr 指定的地址，其中 addr 必须是先前映射的区域。 pmem_unmap() 将使用 munmap(2) 删除映射。

> 注意： pmem_is_pmem() 查询的结果仅对使用 pmem_map_file() 创建的映射有效。对于其他内存区域，尤其是通过直接调用 mmap(2) 创建的内存区域，pmem_is_pmem() 始终返回 false，即使查询的范围完全是持久内存。

## 2. 持久化操作

```cpp
#include <libpmem.h>

void pmem_persist(const void *addr, size_t len);
int pmem_msync(const void *addr, size_t len);
void pmem_flush(const void *addr, size_t len);
void pmem_deep_flush(const void *addr, size_t len); (EXPERIMENTAL)
int pmem_deep_drain(const void *addr, size_t len); (EXPERIMENTAL)
int pmem_deep_persist(const void *addr, size_t len); (EXPERIMENTAL)
void pmem_drain(void);
int pmem_has_auto_flush(void); (EXPERIMENTAL实验性)
int pmem_has_hw_drain(void);
```

本节中的函数提供对刷新到持久化阶段的访问（更细阶段的控制），适用于应用程序需要比 pmem_persist() 函数更多地控制刷新操作的不太常见的情况。

> 警告：在 pmem_is_pmem(3) 返回 false 的范围内使用 pmem_persist() 可能没有任何用处——请改用 msync(2)。

pmem_persist() 函数强制将 [addr, addr+len) 范围内的任何更改持久存储在持久内存中。这等效于调用 msync(2) 但可能更优化，并且会尽可能避免调用内核。 addr 和 len 描述的范围没有对齐限制，但 pmem_persist() 可以根据需要扩展范围以满足平台对齐要求。

> 警告：与 msync(2) 一样，此调用没有任何原子性或事务性。给定范围内的任何未写入的存储都将被写入，但某些存储可能已经根据正常的缓存逐出/替换策略被写入。正确编写的代码不能依赖于等待直到 pmem_persist() 被调用才能持久化的存储——它们可以在调用 pmem_persist() 之前的任何时间变得持久化。

pmem_msync() 函数类似于 pmem_persist()，因为它强制持久存储范围 [addr, addr+len) 中的任何更改。由于它调用 msync()，此函数适用于持久内存或传统存储上的内存映射文件。 pmem_msync() 采取措施确保传递给 msync() 的地址和长度的对齐符合该系统调用的要求。它调用带有 MS_SYNC 标志的 msync()，如 msync(2) 中所述。通常，应用程序只检查是否存在持久内存一次，然后在整个程序中使用该结果，例如：

```cpp
/* do this call once, after the pmem is memory mapped */
int is_pmem = pmem_is_pmem(rangeaddr, rangelen);

/* ... make changes to a range of pmem ... */

/* make the changes durable */
if (is_pmem)
	pmem_persist(subrangeaddr, subrangelen);
else
	pmem_msync(subrangeaddr, subrangelen);

/* ... */
```

> 警告：在 Linux 上，pmem_msync() 和 msync(2) 对从**设备 DAX** 映射的内存范围没有影响。如果 pmem_is_pmem(3) 返回 true 的内存范围，请使用 pmem_persist() 强制将更改持久存储在持久内存中。

pmem_flush() 和 pmem_drain() 函数提供了 pmem_persist() 函数的部分版本。 pmem_persist() 可以被认为是这样的：

```cpp

void
pmem_persist(const void *addr, size_t len)
{
	/* flush the processor caches */
	pmem_flush(addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	pmem_drain();
}
```

这些函数允许高级程序创建它们自己的 pmem_persist() 变体。例如，需要刷新多个不连续范围的程序可以为每个范围调用 pmem_flush()，然后通过调用 pmem_drain() 进行跟进。

pmem_deep_flush() 函数的语义与 pmem_flush() 函数相同，只是 pmem_deep_flush() 与 PMEM_NO_FLUSH 环境变量无关（请参阅 libpmem(7) 中的 ENVIRONMENT 部分）并始终刷新处理器缓存。

pmem_deep_persist() 函数的行为与 pmem_persist() 相同，不同之处在于它通过将持久内存存储刷新到软件可用的**最可靠**持久域来提供更高的可靠性，而不是依赖于断电时的自动 WPQ（写挂起队列）刷新(ADR)。

pmem_deep_flush() 和 pmem_deep_drain() 函数提供了 pmem_deep_persist() 函数的部分版本。 pmem_deep_persist() 可以被认为是这样的：

```cpp
int pmem_deep_persist(const void *addr, size_t len)
{
	/* flush the processor caches */
	pmem_deep_flush(addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	return pmem_deep_drain(addr, len);
}
```

由于此操作通常比 pmem_persist() 昂贵得多，因此应该很少使用。通常，应用程序应仅使用此功能来刷新最关键的数据，这些数据需要在断电后恢复。

pmem_has_auto_flush() 函数检查机器是否支持在电源故障或系统崩溃时**自动刷新 CPU 缓存**。仅当系统中的每个 NVDIMM 都被此机制覆盖时，函数才返回 true。

> 如果给定平台支持在断电事件时刷新处理器缓存，则 pmem_has_auto_flush() 函数返回 1。否则返回 0。出错时返回 -1 并适当设置 errno。

pmem_has_hw_drain() 函数检查机器是否支持持久内存的显式硬件drain指令。

> 如果机器支持持久内存的显式硬件排放指令，则 pmem_has_hw_drain() 函数返回 true。在具有持久内存的 Intel 处理器上，一旦从 CPU 缓存中flush到持久内存的存储就被认为是持久的，因此此函数始终返回 false。**尽管如此，使用 pmem_flush() 刷新内存范围的程序仍应通过调用 pmem_drain() 一次以确保刷新完成**。如上所述，pmem_persist() 处理调用 pmem_flush() 和 pmem_drain()。

## 3. 内存操作

```cpp
#include <libpmem.h>

void *pmem_memmove(void *pmemdest, const void *src, size_t len, unsigned flags);
void *pmem_memcpy(void *pmemdest, const void *src, size_t len, unsigned flags);
void *pmem_memset(void *pmemdest, int c, size_t len, unsigned flags);
void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);
```

pmem_memmove()、pmem_memcpy() 和 pmem_memset() 函数提供与其同名 memmove(3)、memcpy(3) 和 memset(3) 相同的内存复制，并确保结果在返回之前已刷新到持久性（除非 PMEM_F_MEM_NOFLUSH标志被使用）。

例如，以下代码在功能上等效于 pmem_memmove()（flag等于 0）：

```cpp
	memmove(dest, src, len);
	pmem_persist(dest, len);
```

调用 pmem_memmove() 可能会胜过上述代码，因为 libpmem(7) 实现可能会利用 pmemdest 是持久内存这一事实，并使用non-temporal存储等指令来避免刷新处理器缓存的需要。

> 警告：在 pmem_is_pmem(3) 返回 false 的情况下使用这些函数可能没有任何用处。在这种情况下使用 libc 函数。

与 libc 实现不同，libpmem 函数保证如果目标缓冲区地址和长度是 8 字节对齐的，那么所有存储都将使用至少 8 字节存储指令执行。**这意味着一系列 8 字节存储后跟 pmem_persist(3) 可以安全地替换为对上述函数之一的单个调用**。

上述所有函数的 flags 参数具有相同的含义。它可以是 0 或以下一个或多个标志的按位或：

- PMEM_F_MEM_NODRAIN - 修改行为以跳过最后的 pmem_drain() 步骤。这允许应用程序优化多个范围被复制到持久内存的情况，然后是对 pmem_drain() 的单个调用。以下示例说明了在将多个内存范围复制到 pmem 时如何使用此标志来避免多次调用 pmem_drain()：

```cpp
/* ... write several ranges to pmem ... */
pmem_memcpy(pmemdest1, src1, len1, PMEM_F_MEM_NODRAIN);
pmem_memcpy(pmemdest2, src2, len2, PMEM_F_MEM_NODRAIN);

/* ... */

/* wait for any pmem stores to drain from HW buffers */
pmem_drain();
```

- PMEM_F_MEM_NOFLUSH - 不要刷新任何东西。这意味着 PMEM_F_MEM_NODRAIN。使用此标志仅在其后跟任何刷新数据的函数时才有意义。

其余的标志说明操作应该如何完成，并且只是提示。

- PMEM_F_MEM_NONTEMPORAL - 使用non-temporal指令。此标志与 PMEM_F_MEM_TEMPORAL 互斥。在 x86_64 上，此标志与 PMEM_F_MEM_NOFLUSH 互斥。
- PMEM_F_MEM_TEMPORAL - 使用temporal指令。此标志与 PMEM_F_MEM_NONTEMPORAL 互斥。
- PMEM_F_MEM_WC - 使用写组合模式。此标志与 PMEM_F_MEM_WB 互斥。在 x86_64 上，此标志与 PMEM_F_MEM_NOFLUSH 互斥。
- PMEM_F_MEM_WB - 使用回写模式。此标志与 PMEM_F_MEM_WC 互斥。在 x86_64 上，这是 PMEM_F_MEM_TEMPORAL 的别名。

> 使用无效的标志组合具有未定义的行为。

如果没有上述任何标志，libpmem 将尝试根据大小猜测最佳策略。有关详细信息，请参阅 libpmem(7) 中的 PMEM_MOVNT_THRESHOLD 描述。

pmem_memmove_persist() 是 pmem_memmove() (标志等于 0)的别名。

pmem_memcpy_persist() 是 pmem_memcpy() 的别名，标志等于 0。

pmem_memset_persist() 是 pmem_memset() 的别名，标志为 0。

pmem_memmove_nodrain() 是 pmem_memmove() 的别名，其标志等于 PMEM_F_MEM_NODRAIN。

pmem_memcpy_nodrain() 是 pmem_memcpy() 的别名，其标志等于 PMEM_F_MEM_NODRAIN。

pmem_memset_nodrain() 是 pmem_memset() 的别名，其标志等于 PMEM_F_MEM_NODRAIN。

返回值：

以上所有函数都返回目标缓冲区的地址。

> 警告：在调用具有 PMEM_F_MEM_NODRAIN 标志的任何函数之后，在调用 pmem_drain(3) 或任何 _persist 函数之前，不应期望其他线程可以看到内存的最新提交。这是因为在 x86_64 上，这些函数可能使用弱排序的非临时存储指令。有关详细信息，请参阅“Intel 64 和 IA-32 架构软件开发人员手册”，第 1 卷，“缓存Temporal数据与Non-Temporal数据”部分。

## 4. 其他

```cpp
const char *pmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

检查libpmem库的版本

```cpp
const char *pmem_errormsg(void);
```

错误处理。

