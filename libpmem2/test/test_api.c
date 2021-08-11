/*
 * basic.c -- simple example for the libpmem2
 */

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

const char* PMEM_FILE_NAME = "./tmp/test.txt";

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;

	if ((fd = open(PMEM_FILE_NAME, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

    // https://blog.csdn.net/fengruoying93/article/details/97617551
    struct  stat struct_stat;
    fstat(fd, &struct_stat);
    long int file_size = struct_stat.st_size;
    printf("文件%s的大小：%ld\n",PMEM_FILE_NAME, file_size);

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

    ///////////////////////////////
    //// 设置映射的长度，文件默认初始大小为10
    //////////////////////////////

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
    /// pmem2_config_set_length(cfg, 4096);

    // 结论：
    // 1. 映射长度不能超过文件大小
    // 2. lenght与映射粒度无关，最小的映射长度要为4096的倍数，也就是完整的页，这是由操作系统的内存管理方式决定的
    // 3. 设置映射长度后，只能使用文件开头的length个字节，超出范围则发生段错误


	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

    // 若处理器不支持eADR，则使用PMEM2_GRANULARITY_BYTE会报错
	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

	if (pmem2_map_new(&map, cfg, src)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	size_t size = pmem2_map_get_size(map);
    printf("map size: %lu\n", size);

    // 发生段错误，超出映射长度
	// strcpy(addr+4096, "hello, persistent memory");
	// persist = pmem2_get_persist_fn(map);
	// persist(addr+4096, size);

	printf("addr1: %p, size1: %lu\n", addr, size);
	struct pmem2_map *map2;
	if(pmem2_map_from_existing(&map2, src, 0, size, PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_map_from_existing");
		exit(1);
	}

	char *addr2 = pmem2_map_get_address(map2);
	size_t size2 = pmem2_map_get_size(map2);
    printf("addr1: %p, size1: %lu, addr2: %p, size2: %lu\n", addr, size, addr2, size2);
	
	strcpy(addr2, "hello2, persistent2 memory2s");
	persist = pmem2_get_persist_fn(map2);
	persist(addr2, size2);

	pmem2_map_delete(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);

	return 0;
}