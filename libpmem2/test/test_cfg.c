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

const char* PMEM_FILE_NAME = "/mnt/pmem/zzh/test.txt";

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	pmem2_persist_fn persist;

    // O_CREAT
	if ((fd = open(PMEM_FILE_NAME, O_CREAT|O_RDWR)) < 0) {
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

    // 10个page
    for(int i=0; i<10; i++) {
        for(int j=0; j<1024; j++) {
            char buf[4];
            buf[0] = buf[1] = buf[2] = buf[3] = i + '0';
            write(fd, buf, 4);
        }
    }
	fsync(fd);

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

    size_t alignment = -1;
    pmem2_source_alignment(src, &alignment);
    printf("[src] alignment: %lu\n", alignment);

    // 若处理器不支持eADR，则使用PMEM2_GRANULARITY_BYTE会报错
	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_CACHE_LINE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

    pmem2_config_set_offset(cfg, 4096);
    pmem2_config_set_length(cfg, 4096);

	if (pmem2_map_new(&map, cfg, src)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}

	char *addr = pmem2_map_get_address(map);
	size_t size = pmem2_map_get_size(map);
	printf("[map] addr: %p, size: %lu,\n", addr, size);

    for(int i = 0; i< size; i++) {
        putchar(addr[i]);
    }
    putchar('\n');

	pmem2_map_delete(&map);
	pmem2_source_delete(&src);
	pmem2_config_delete(&cfg);
	close(fd);

	return 0;
}