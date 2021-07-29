PROGS = clean

LIBS = -lpmem2
CFLAGS = ${LIBS} # 编译时的参数
LDFLAGS = ${LIBS} # 链接时的参数

DIR_BIN := bin
PMEM_DIR = /mnt/pmem/zzh
TEST_FILE = ${PMEM_DIR}/test.txt

basic: basic.o
	${CC} -o ${DIR_BIN}/$@ $^ $(CFLAGS)

basic_run:
	./${DIR_BIN}/basic ${TEST_FILE}
	cat ${TEST_FILE}

api: test_api.o
	${CC} -o ${DIR_BIN}/$@ $^ $(CFLAGS)

api_run:
	./${DIR_BIN}/api 
	cat ${TEST_FILE}

${DIR_BIN}:
	mkdir $@

clean:
	rm -rf *.o bin/*