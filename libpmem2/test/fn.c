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

const char* PMEM_FILE_NAME1 = "./tmp/fn1.txt";
const char* PMEM_FILE_NAME2 = "./tmp/fn2.txt";

int
main(int argc, char *argv[])
{
	int fd1;
	struct pmem2_config *cfg1;
	struct pmem2_map *map1;
	struct pmem2_source *src1;
	pmem2_drain_fn drain1;
    int fd2;
	struct pmem2_config *cfg2;
	struct pmem2_map *map2;
	struct pmem2_source *src2;
	pmem2_drain_fn drain2;

	if ((fd1 = open(PMEM_FILE_NAME1, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}
	if (pmem2_source_from_fd(&src1, fd1)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}
    if (pmem2_config_new(&cfg1)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}
	if (pmem2_config_set_required_store_granularity(cfg1,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}
	if (pmem2_map_new(&map1, cfg1, src1)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}
	drain1 = pmem2_get_drain_fn(map1);
    
    if ((fd2 = open(PMEM_FILE_NAME2, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}
	if (pmem2_source_from_fd(&src2, fd2)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}
    if (pmem2_config_new(&cfg2)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}
	if (pmem2_config_set_required_store_granularity(cfg2,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}
	if (pmem2_map_new(&map2, cfg2, src2)) {
		pmem2_perror("pmem2_map_new");
		exit(1);
	}
	drain2 = pmem2_get_drain_fn(map2);

    printf("drain1: %p, drain2: %p\n", drain1, drain2);

	pmem2_map_delete(&map1);
	pmem2_source_delete(&src1);
	pmem2_config_delete(&cfg1);
	close(fd1);

    pmem2_map_delete(&map2);
	pmem2_source_delete(&src2);
	pmem2_config_delete(&cfg2);
	close(fd2);

	return 0;
}