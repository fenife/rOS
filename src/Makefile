ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc
LD = ld

TOP_DIR   = .
BUILD_DIR = ./build
OBJS_DIR  = ${BUILD_DIR}

INC_DIR	:= ${TOP_DIR}/include
INC_DIR +=

ASFLAGS := -f elf
CFLAGS  := -I ${INC_DIR} -c
LDFLAGS := -Ttext ${ENTRY_POINT} -e main

OBJS := ${OBJS_DIR}/main.o ${OBJS_DIR}/print.o

all :
	${AS} ${ASFLAGS} -o ${OBJS_DIR}/print.o ${TOP_DIR}/lib/kernel/print.S
	${CC} ${CFLAGS} -o ${OBJS_DIR}/main.o ${TOP_DIR}/kernel/main.c
	${LD} ${LDFLAGS} -o ${OBJS_DIR}/kernel.bin ${OBJS}

mbr :
	${AS} -I ${TOP_DIR}/boot/ -o ${OBJS_DIR}/mbr.bin ${TOP_DIR}/boot/mbr.S
	${AS} -I ${TOP_DIR}/boot/ -o ${OBJS_DIR}/loader.bin ${TOP_DIR}/boot/loader.S

hd	:
	./dd.sh

clean :
	rm $(shell find ./ -name *.o) -f
	rm $(shell find ./ -name *.bin) -f
