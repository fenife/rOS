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
CFLAGS  := -I ${INC_DIR} -c -fno-builtin
LDFLAGS := -Ttext ${ENTRY_POINT} -e main

OBJS := ${OBJS_DIR}/main.o  ${OBJS_DIR}/init.o ${OBJS_DIR}/interrupt.o \
		${OBJS_DIR}/print.o ${OBJS_DIR}/intr_entry.o ${OBJS_DIR}/timer.o

all :
	${AS} -I ${TOP_DIR}/boot/ -o ${OBJS_DIR}/mbr.bin ${TOP_DIR}/boot/mbr.S
	${AS} -I ${TOP_DIR}/boot/ -o ${OBJS_DIR}/loader.bin ${TOP_DIR}/boot/loader.S
	${AS} ${ASFLAGS} -o ${OBJS_DIR}/print.o ${TOP_DIR}/lib/kernel/print.S
	${AS} ${ASFLAGS} -o ${OBJS_DIR}/intr_entry.o ${TOP_DIR}/kernel/intr_entry.S
	${CC} ${CFLAGS} -o ${OBJS_DIR}/interrupt.o ${TOP_DIR}/kernel/interrupt.c
	${CC} ${CFLAGS} -o ${OBJS_DIR}/init.o ${TOP_DIR}/kernel/init.c
	${CC} ${CFLAGS} -o ${OBJS_DIR}/main.o ${TOP_DIR}/kernel/main.c
	${CC} ${CFLAGS} -o ${OBJS_DIR}/timer.o ${TOP_DIR}/device/timer.c

rld :
	${LD} ${LDFLAGS} -o ${OBJS_DIR}/kernel.bin ${OBJS}

hd	:
	./dd.sh

clean :
	rm $(shell find ./ -name *.o) -f
	rm $(shell find ./ -name *.bin) -f
