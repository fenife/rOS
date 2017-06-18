ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc
LD = ld
RM = rm -rf 

TOP_DIR   = .
BUILD_DIR = ./build
OBJS_DIR  = ${BUILD_DIR}
BOCHS_PATH =/root/tools/bochs

INC_DIR	:= ${TOP_DIR}/include

ASFLAGS := -f elf
CFLAGS  := -Wall -I ${INC_DIR} -c -fno-builtin -W -Wstrict-prototypes \
		   -Wmissing-prototypes
LDFLAGS := -Ttext ${ENTRY_POINT} -e main

OBJS := ${OBJS_DIR}/main.o  ${OBJS_DIR}/init.o ${OBJS_DIR}/interrupt.o \
		${OBJS_DIR}/print.o ${OBJS_DIR}/intr_entry.o ${OBJS_DIR}/timer.o \
		${OBJS_DIR}/debug.o

all : build rhd

##############     c代码编译     ###############
${OBJS_DIR}/interrupt.o : ${TOP_DIR}/kernel/interrupt.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/init.o : ${TOP_DIR}/kernel/init.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/main.o : ${TOP_DIR}/kernel/main.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/timer.o : ${TOP_DIR}/device/timer.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/debug.o : ${TOP_DIR}/kernel/debug.c
	${CC} ${CFLAGS} $< -o $@

##############    汇编代码编译    ###############
${OBJS_DIR}/mbr.bin : ${TOP_DIR}/boot/mbr.S
	${AS} -I ${TOP_DIR}/boot/ $< -o $@

${OBJS_DIR}/loader.bin : ${TOP_DIR}/boot/loader.S
	${AS} -I ${TOP_DIR}/boot/ $< -o $@

${OBJS_DIR}/print.o : ${TOP_DIR}/lib/kernel/print.S
	${AS} ${ASFLAGS} $< -o $@

${OBJS_DIR}/intr_entry.o : ${TOP_DIR}/kernel/intr_entry.S
	${AS} ${ASFLAGS} $< -o $@

##############    链接所有目标文件    #############
${OBJS_DIR}/kernel.bin : ${OBJS}
	${LD} ${LDFLAGS} $^ -o $@

.PHONY : rhd build all rls clean

##############    用命令dd把编译好的文件写入到硬盘中    #############
rhd	:
	@dd if=${OBJS_DIR}/mbr.bin of=${BOCHS_PATH}/hd60M.img bs=512 \
		count=1 conv=notrunc
	@dd if=${OBJS_DIR}/loader.bin of=${BOCHS_PATH}/hd60M.img bs=512 \
		count=4 seek=2 conv=notrunc
	@dd if=${OBJS_DIR}/kernel.bin of=${BOCHS_PATH}/hd60M.img bs=512 \
		count=200 seek=9 conv=notrunc

build : ${OBJS_DIR}/kernel.bin ${OBJS_DIR}/mbr.bin ${OBJS_DIR}/loader.bin

rls :
	@cd ${OBJS_DIR}; ls

clean :
	${RM} ${shell find . -name *.o}
	${RM} ${shell find . -name *.bin}
