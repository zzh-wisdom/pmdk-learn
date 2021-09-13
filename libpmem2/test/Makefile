PROGS = clean all

SOURCE = $(wildcard *.c)
TARGETS = $(patsubst %.c,%,$(SOURCE))

CC = gcc
CFLAGS += --std=c++11
LIBS = -lpmem2

DIR_BIN := bin
PMEM_DIR = ./tmp
TEST_FILE = ${PMEM_DIR}/test.txt

# DEPENCES = ../cpu.h ../chunk_allocator.h
# OBJ_SOURCE = ../chunk_allocator.cc

%:%.c # $(OBJ_SOURCE) $(DEPENCES)
	$(shell if [ ! -e $(DIR_BIN) ];then mkdir -p $(DIR_BIN); fi)
	$(CC) -o $(DIR_BIN)/$@ $< $(LIBS)

all:$(TARGETS)

basic_run:
	./${DIR_BIN}/basic ${TEST_FILE}
	cat ${TEST_FILE}

api_run:
	./${DIR_BIN}/api 
	cat ${TEST_FILE}

vm_run:
	./${DIR_BIN}/vm 
	cat ${TEST_FILE}

fn_run:
	./${DIR_BIN}/fn

${DIR_BIN}:
	mkdir $@

clean:
	rm -rf *.o bin/*