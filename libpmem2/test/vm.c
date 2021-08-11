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
char buffers[4096*2];

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;
    // printf("map size:\n");
	if ((fd = open(PMEM_FILE_NAME, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}
    // printf("map size:\n");
	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

    if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

    // printf("map size:\n");
    // 若处理器不支持eADR，则使用PMEM2_GRANULARITY_BYTE会报错
	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

    // printf("map size:");
    struct pmem2_vm_reservation* rsv_p;
    void* tmp = (void*)((((long unsigned int)buffers+4096-1)/4096)*4096);
    printf("buffers: \t%p\ntmp: \t\t%p\n", buffers, tmp);
    // 失败，已经被占用的虚拟地址
    if(pmem2_vm_reservation_new(&rsv_p, tmp+4096*2, 4096)){
        pmem2_perror("pmem2_vm_reservation_new");
		exit(1);
    }

    // 设置虚拟映射区域
    if(pmem2_config_set_vm_reservation(cfg, rsv_p, 0)){
        pmem2_perror("pmem2_config_set_vm_reservation");
		exit(1);
    }

	if (pmem2_map_new(&map, cfg, src)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	size_t size = pmem2_map_get_size(map);
    printf("map size: %lu\n", size);
    printf("addr: \t\t%p\nbuffers: \t%p\ntmp: \t\t%p\n", addr, buffers, tmp);

    // 发生段错误，超出映射长度
	strcpy(addr, "hello, persistent memory");
	persist = pmem2_get_persist_fn(map);
	persist(addr, size);

	pmem2_map_delete(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
    pmem2_vm_reservation_delete(&rsv_p);
	close(fd);

	return 0;
}