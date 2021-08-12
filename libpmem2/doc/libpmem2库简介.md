# The libpmem2 library

libpmem2 提供低级持久内存支持。特别是，提供了对用于将更改刷新到 pmem 的**持久内存指令**的支持。

该库是为跟踪每个存储到 pmem 并需要将这些更改刷新到持久性的软件提供的。大多数开发人员会发现像 libpmemobj 这样的高级库更方便。

包含提供的 Linux 接口列表的手册页：

- Man page for [libpmem2 current master](https://pmem.io/pmdk/manpages/linux/master/libpmem2/libpmem2.7.html)

包含提供的 Windows 接口列表的手册页：

- Man page for [libpmem2 current master](https://pmem.io/pmdk/manpages/windows/master/libpmem2/libpmem2.7.html)

## libpmem2 Example

### The Basics

如果您决定自己处理**跨程序中断的持久内存分配和一致性**，您会发现 libpmem2 中的函数很有用。了解原始 pmem 编程意味着您必须创建自己的事务或说服自己不要在意系统或程序崩溃是否使您的 pmem 文件处于不一致状态，了解这一点很重要。libpmem2 中的接口是非事务性的，但可用于构建事务性接口，如 [libpmemobj](https://pmem.io/pmdk/libpmemobj)。

为了说明基础知识，让我们先浏览一下手册页示例：

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
```

如上所示，该示例首先包含必要的头文件。第 18 行（突出显示的行）显示了使用 libpmem2 需要包含的头文件：**libpmem2.h**。

对于这个简单的例子，我们将向内存映射文件写入一个字符串。
首先，我们需要根据源文件准备配置和映射。

```cpp
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
```

上面几行创建文件、准备config和source结构，设置所需的**粒度**并将文件映射到内存中。

这说明了 libpmem2 中的基本功能：首先，==pmem2_config_new()== 创建将用于映射的配置结构。 Config 是一个对象，我们用它来定义要创建的目标映射的参数。

在这个例子中，我们只在配置中设置粒度，其他值保持默认。**粒度是配置中唯一需要的参数**。除了粒度设置之外，libpmem2 还提供了多个可选功能来配置目标映射，例如==pmem2_config_set_length()== 设置将用于映射的长度，或 ==pmem2_config_set_offset== 将用于从源的指定位置映射内容。

突出显示的第二行包含对 pmem2_source_from_fd() 的调用，该调用采用文件描述符并创建source结构体的新实例，该实例描述用于映射的数据源。在此特定示例中，映射源来自文件描述符，但是，libpmem2 还提供使用文件句柄作为源或创建匿名映射的函数。

此示例中的下一个关键步骤是使用  pmem2_config_set_required_store_granularity 设置粒度。粒度必须是以下三个值之一：PMEM2_GRANULARITY_BYTE、PMEM2_GRANULARITY_CACHE_LINE、PMEM2_GRANULARITY_PAGE。
在这种情况下，我们将最大允许粒度设置为 PMEM2_GRANULARITY_PAGE。从逻辑上讲，通过将粒度设置为 page，应用程序表明即使底层设备是基于块的，它也会继续运行。

可以在[此处](https://pmem.io/pmdk/manpages/linux/master/libpmem2/libpmem2.7.html)找到有关粒度概念和每个选项的更多详细信息

最后一步是使用上述config和source创建映射。pmem2_map() 的底层函数在 POSIX 上调用 mmap(2) 或在 Windows 上调用 CreateFileMapping() 以内存映射关于允许粒度的整个文件。

创建映射后，我们就可以写入其中了。

```cpp
    char *addr = pmem2_map_get_address(map);
    size_t size = pmem2_map_get_size(map);

    strcpy(addr, "hello, persistent memory");
```

使用像 pmem2_map_get_size 或 pmem2_map_get_address 这样的 getter，我们可以轻松读取有关创建的映射的信息。

pmem 的新颖之处在于您可以直接复制到它，就像任何内存一样。
上面第 63 行显示的 strcpy() 调用只是将字符串存储到内存的常用 libc 函数。如果此示例程序在 strcpy() 调用期间或刚刚调用之后被中断，则您无法确定字符串的哪些部分被持久化到媒体。它可能是没有任何字符串、所有字符串或介于两者之间的某个地方。此外，**不能保证字符串会按照存储的顺序进入媒体！**对于更长的范围，稍后复制的部分很可能先于较早的部分进入媒体。 （所以不要像上面的例子那样编写代码，然后期望检查零以查看写入了多少字符串。）

字符串是如何可以以一个看似随机的顺序进行存储的？原因是在像 msync() 这样的刷新函数成功返回之前，活动系统上发生的正常缓存压力可以随时以任何顺序将更改推送到媒体。大多数处理器都有屏障指令（如 Intel 平台上的 SFENCE），但这些指令处理存储对其他线程可见性的顺序，而不是更改到达持久性的顺序。**刷新到持久性的唯一障碍是 pmem2_get_persist_fn() 返回的函数**，如下所示。

```cpp
	persist = pmem2_get_persist_fn(map);
	persist(addr, size);
```

libpmem2 函数 pmem2_get_persist_fn 自动决定将数据刷新到底层存储的最合适机制。这意味着如果映射不支持用户空间的flushpersist 函数将回退到使用 OS 原语来同步数据。

**如果可能，上面的 persist() 函数将直接从用户空间执行刷新，而无需调用操作系统**。这可以在英特尔平台上使用英特尔手册中描述的 CLWB 和 CLFLUSHOPT 等指令实现。**当然，您可以直接在程序中自由使用这些指令**，但是如果您尝试在不支持它们的平台上使用这些指令，程序将因未定义的操作码而崩溃。这就是 libpmem2 通过在启动时检查平台功能并为其支持的每个操作选择最佳指令来帮助您的地方。

```cpp
	pmem2_unmap(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);
```

为了避免我们示例中的内存泄漏，最后要做的是取消映射、释放源和配置结构并关闭文件描述符。（文件描述符在创建好映射后就可以关闭了吧）

PMDK 存储库中提供了上述 [libpmem2 basic.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem2) 示例的可构建源代码。

### Copying to Persistent Memory

libpmem2 的另一个特性是有一组用于最佳复制到持久内存的例程。这些函数执行与 libc 函数 memcpy()、memset() 和 memmove() 相同的功能，但它们针对复制到 pmem 进行了优化。在英特尔平台上，这是使用**绕过处理器缓存**的非临时（non-temporal）store指令完成的（无需刷新数据路径的那部分）。

下面的示例说明了以下代码：

```cpp
        memset(dest, c, len);
        pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
        persist_fn(dest, len);
```

在功能上等同于：

```cpp
        pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
        memset_fn(dest, c, len);
```

上面代码的第二部分展示了如何像 memset(3) 一样使用 memset_fn()，而不用像 libpmem2 那样处理，将数据刷新到持久性作为集合的一部分。

### Separating the Flush Steps

刷新到持久化有两个步骤。第一步是**刷新处理器缓存**，或**完全绕过它们**，如前面示例中所述。第二步是等待任何硬件缓冲区drain，以确保写入已到达介质。当调用 pmem2_get_persist_fn() 返回的函数时，**上述的两个步骤会一起执行**：

```cpp
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	persist_fn(addr, len);
```

或者它们可以通过调用来自 ==pmem2_get_flush_fn()== 的函数进行（上述的）第一步和来自 ==pmem2_get_drain_fn()== 的函数进行第二步：

```cpp
	pmem2_flush_fn flush_fn = pmem2_get_flush_fn(map);
	pmem2_drain_fn drain_fn = pmem2_get_drain_fn(map);

	flush_fn(addr, len);
	drain_fn();
```
**（？？）**

请注意，在给定的平台上，**这些步骤中的任何一个都可能是不必要的**，并且库知道如何检查并做正确的事情。例如，在带有 eADR 的 Intel 平台上，flusn_fn() 是一个空函数。（**这里的drain_fn相当于fence操作，防止重排序**。）

什么时候将flush分解为步骤是有意义？例如，如果一个程序多次调用 memcpy()，它可以复制数据并只执行flush，将最后的drain步骤推迟到最后。这是有效的，因为与刷新步骤不同，drain步骤不采用地址范围 - 它是系统范围的drain操作，因此可以在复制单个数据块的循环结束时发生。
