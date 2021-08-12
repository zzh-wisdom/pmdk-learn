# libpmem 简介

<https://pmem.io/pmdk/libpmem/>

libpmem 提供**低级持久内存支持**。特别是，提供了对用于将更改刷新到 pmem 的持久内存指令的支持。

该库是为跟踪每个存储到 pmem 并需要将这些更改刷新到持久性的软件提供的。大多数开发人员会发现像 libpmemobj 这样的高级库更方便。

Man pages that contains a list of the Linux interfaces provided:

- Man page for [libpmem current master](https://pmem.io/pmdk/manpages/linux/master/libpmem/libpmem.7.html)

Man pages that contains a list of the Windows interfaces provided:

- Man page for [libpmem current master](https://pmem.io/pmdk/manpages/windows/master/libpmem/libpmem.7.html)

## libpmem Examples

如果您决定自己处理跨程序中断的持久内存分配和一致性，您会发现 libpmem 中的函数很有用。了解原始 pmem 编程意味着您必须创建自己的事务或说服自己您不在乎系统或程序崩溃使您的 pmem 文件处于不一致状态，了解这一点很重要。像 libpmemobj 这样的库通过构建这些 libpmem 函数来提供事务性接口，但 libpmem 中的接口是非事务性的。

为了说明基础知识，让我们先浏览一下手册页示例：

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
```

如上所示，该示例首先包含必要的头文件。第 45 行（突出显示的行）显示了使用 libpmem 需要包含的头文件：libpmem.h。

```cpp
/* using 4k of pmem for this example */
#define	PMEM_LEN 4096
```

对于这个简单的例子，我们将硬编码一个 4 KB 大小的 pmem 文件。

```cpp
int
main(int argc, char *argv[])
{
	int fd;
	char *pmemaddr;
	int is_pmem;

	/* create a pmem file */
	if ((fd = open("/pmem-fs/myfile", O_CREAT|O_RDWR, 0666)) < 0) {
		perror("open");
		exit(1);
	}

	/* allocate the pmem */
	if ((errno = posix_fallocate(fd, 0, PMEM_LEN)) != 0) {
		perror("posix_fallocate");
		exit(1);
	}

	/* memory map it */
	if ((pmemaddr = pmem_map(fd)) == NULL) {
		perror("pmem_map");
		exit(1);
	}
	close(fd);
```

> 注意：pmem_map replaced with pmem_map_file。pmem_map函数已经不受支持

上面的几行创建了文件，确保分配了 4k，并将文件映射到内存中。这说明了 libpmem 中的一个辅助函数：pmem_map()，它接受一个文件描述符并调用 mmap(2) 来内存映射整个文件。直接调用 mmap() 也会正常工作——pmem_map() 的主要优点是它会尝试找到一个可能使用大页面映射的地址，以便在使用大范围 pmem 时获得更好的性能。

**请注意，一旦 pmem 文件映射到内存中，就没有必要保持文件描述符打开。**

由于内存映射持久内存的系统调用与 POSIX 内存映射任何文件的调用相同，您可能希望编写代码以在给定 pmem 文件或传统文件系统上的文件时正确运行。**几十年来，写入文件的内存映射范围的更改在刷新到媒体之前可能不会持久**。一种常见的方法是使用 POSIX 调用 msync(2)。如果您将程序写入内存映射文件并在每次要刷新更改到媒体时使用 msync()，那么它将对 pmem 以及传统文件系统上的文件正常工作。**但是，如果您明确检测到 pmem 并在这种情况下使用 libpmem 刷新更改，您可能会发现您的程序性能更好**。

```cpp
	/* determine if range is true pmem */
	is_pmem = pmem_is_pmem(pmemaddr, PMEM_LEN);
```

libpmem 函数 pmem_is_pmem() 可用于确定给定范围内的内存是否真的是持久内存，或者它是否只是传统文件系统上的内存映射文件。在您的程序中使用此调用将允许您决定在给定非 pmem 文件时要执行的操作。您的程序可以决定打印错误消息并退出（例如：“错误：此程序仅适用于 pmem”）。**但您似乎更有可能想要保存 pmem_is_pmem() 的结果，如上所示，然后使用该标志来决定在将更改刷新到持久性时要做什么**，如本示例程序后面的内容。

```cpp
	/* store a string to the persistent memory */
	strcpy(pmemaddr, "hello, persistent memory");
```

pmem 的新颖之处在于您可以直接复制到它，就像任何内存一样。上面第 80 行显示的 strcpy() 调用只是将字符串存储到内存的常用 libc 函数。如果此示例程序在 strcpy() 调用期间或刚刚调用之后被中断，则您无法确定字符串的哪些部分一直到媒体。它可能没有任何字符串、所有字符串或介于两者之间的某个地方。此外，不能保证字符串会按照存储的顺序进入媒体！对于更长的范围，稍后复制的部分很可能先于较早的部分进入媒体。 （所以不要像上面的例子那样编写代码，然后期望检查零以查看写入了多少字符串。）

一个字符串如何以看似随机的顺序存储呢？原因是在像 msync() 这样的刷新函数成功返回之前，活动系统上发生的正常缓存压力可以随时以任何顺序将更改推送到媒体。大多数处理器都有屏障指令（如 Intel 平台上的 SFENCE），但这些指令处理存储对其他线程**可见性的排序**，而不是更改到达持久性的顺序。刷新到持久性的唯一屏障是 msync() 或 pmem_persist() 等函数，如下所示。

```cpp
	/* flush above strcpy to persistence */
	if (is_pmem)
		pmem_persist(pmemaddr, PMEM_LEN);
	else
		pmem_msync(pmemaddr, PMEM_LEN);
```

如上所示，此示例使用从先前调用 pmem_is_pmem() 中保存的 is_pmem 标志。这是使用此信息的推荐方法，而不是每次要持久化更改时调用 pmem_is_pmem()。**这是因为 pmem_is_pmem() 可能有很高的开销**，必须搜索数据结构以确保整个范围是真正的持久内存。

对于真正的 pmem，上​​面突出显示的第 84 行是将更改刷新到持久性的最佳方式。**如果可能，pmem_persist() 将直接从用户空间执行刷新，而无需调用操作系统**。这可以在英特尔平台上使用英特尔手册中描述的 CLWB 和 CLFLUSHOPT 等指令实现。当然，您可以直接在程序中自由使用这些指令，但是如果您尝试在不支持它们的平台上使用这些指令，程序将因未定义的操作码而崩溃。**这就是 libpmem 通过在启动时检查平台功能并为其支持的每个操作选择最佳指令来帮助您的地方。**

上面的例子也使用 pmem_msync() 来处理非 pmem 的情况，而不是直接调用 msync(2)。为方便起见， pmem_msync() 调用是 msync() 的一个小包装器，可确保参数对齐，符合 POSIX 的要求。

PMDK 存储库中提供了上述 [libpmem manpage.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem) 示例的可构建源代码。

## 复制到永久内存

libpmem 的另一个特性是一组用于最佳复制到持久内存的例程。这些函数执行与 libc 函数 memcpy()、memset() 和 memmove() 相同的功能，但它们针对复制到 pmem 进行了优化。在英特尔平台上，这是使用绕过处理器缓存的non-temporal存储指令完成的（无需刷新数据路径的那部分，也就是直接写回介质，而不是先写到cache，再flush）。

第一个复制示例，称为 simple_copy，说明了如何使用 pmem_memcpy()。

```cpp
	/* read up to BUF_LEN from srcfd */
	if ((cc = read(srcfd, buf, BUF_LEN)) < 0) {
		perror("read");
		exit(1);
	}

	/* write it to the pmem */
	if (is_pmem) {
		pmem_memcpy(pmemaddr, buf, cc);
	} else {
		memcpy(pmemaddr, buf, cc);
		pmem_msync(pmemaddr, cc);
	}
```

突出显示的行，即上面的第 105 行，显示了 pmem_memcpy() 的使用方式与 memcpy(3) 一样，不同之处在于当目标是 pmem 时，libpmem 处理将数据刷新到持久性作为拷贝工作的一部分。

PMDK 存储库中提供了上述 [libpmem simple_copy.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem) 示例的可构建源代码。

## 分离flush步骤

刷新到持久化有两个步骤。第一步是刷新处理器缓存，或者完全绕过它们，如前一个示例中所述。第二步是等待任何硬件缓冲区排空drain，以确保写入已到达介质。这些步骤在调用 pmem_persist() 时一起执行，或者可以通过调用 pmem_flush() 进行第一步和 pmem_drain() 进行第二步来单独调用它们。请注意，在给定的平台上，这些步骤中的任何一个都可能是不必要的，并且库知道如何检查并做正确的事情。例如，在具有 eADR 的 Intel 平台上，pmem_flush() 是一个空函数。

什么时候将冲洗分解为步骤有意义？这个名为 full_copy 的示例说明了您可能会这样做的一个原因。由于该示例使用多次调用 memcpy() 来复制数据，因此它使用仅执行刷新的 libpmem copy 版本，将最后的排放步骤推迟到最后。这是有效的，**因为与刷新步骤不同，排空步骤不采用地址范围——它是一个系统范围的排空操作，因此可以在复制单个数据块的循环结束时发生。**

```cpp
/*
 * do_copy_to_pmem -- copy to pmem, postponing drain step until the end
 */
void
do_copy_to_pmem(char *pmemaddr, int srcfd, off_t len)
{
	char buf[BUF_LEN];
	int cc;

	/* copy the file, saving the last flush step to the end */
	while ((cc = read(srcfd, buf, BUF_LEN)) > 0) {
		pmem_memcpy_nodrain(pmemaddr, buf, cc);
		pmemaddr += cc;
	}

	if (cc < 0) {
		perror("read");
		exit(1);
	}

	/* perform final flush step */
	pmem_drain();
}
```

在复制每个块时，上面示例中的第 65 行将一个数据块复制到 pmem，从而有效地将其从处理器缓存中刷新。但不是每次都等待硬件队列耗尽，而是将这一步保存到最后，如上面的第 75 行所示。

PMDK 存储库中提供了上述 [libpmem full_copy.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem) 示例的可构建源代码。

