ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc
LD = ld
RM = rm -rf

TOP_DIR   = .
BUILD_DIR = ./build
OBJS_DIR  = ${BUILD_DIR}
BOCHS_PATH =/root/tools/bochs

INC_DIR	:= -I${TOP_DIR}/include
INC_DIR	+= -I${TOP_DIR}/include/kernel
INC_DIR	+= -I${TOP_DIR}/include/dev
INC_DIR	+= -I${TOP_DIR}/include/thread
INC_DIR	+= -I${TOP_DIR}/include/user
INC_DIR	+= -I${TOP_DIR}/include/fs

ASFLAGS := -f elf
CFLAGS  := -Wall ${INC_DIR} -c -fno-builtin -W -Wstrict-prototypes \
		   -Wmissing-prototypes
LDFLAGS := -Ttext ${ENTRY_POINT} -e main

OBJS := ${OBJS_DIR}/main.o  ${OBJS_DIR}/init.o ${OBJS_DIR}/interrupt.o \
		${OBJS_DIR}/timer.o ${OBJS_DIR}/intr_entry.o ${OBJS_DIR}/print.o \
		${OBJS_DIR}/debug.o ${OBJS_DIR}/memory.o ${OBJS_DIR}/bitmap.o \
		${OBJS_DIR}/printk.o ${OBJS_DIR}/vsprintf.o ${OBJS_DIR}/string.o \
		${OBJS_DIR}/thread.o ${OBJS_DIR}/list.o	${OBJS_DIR}/switch.o \
		${OBJS_DIR}/console.o ${OBJS_DIR}/sync.o  ${OBJS_DIR}/keyboard.o \
		${OBJS_DIR}/ioqueue.o ${OBJS_DIR}/tss.o ${OBJS_DIR}/process.o	\
		${OBJS_DIR}/syscall.o ${OBJS_DIR}/sys.o ${OBJS_DIR}/stdio.o	\
		${OBJS_DIR}/ide.o ${OBJS_DIR}/fs.o ${OBJS_DIR}/inode.o

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

${OBJS_DIR}/string.o : ${TOP_DIR}/lib/string.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/vsprintf.o : ${TOP_DIR}/kernel/vsprintf.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/printk.o : ${TOP_DIR}/kernel/printk.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/bitmap.o : ${TOP_DIR}/lib/kernel/bitmap.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/memory.o : ${TOP_DIR}/kernel/memory.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/thread.o : ${TOP_DIR}/thread/thread.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/list.o : ${TOP_DIR}/lib/kernel/list.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/sync.o : ${TOP_DIR}/thread/sync.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/console.o : ${TOP_DIR}/device/console.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/keyboard.o : ${TOP_DIR}/device/keyboard.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/ioqueue.o : ${TOP_DIR}/device/ioqueue.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/tss.o : ${TOP_DIR}/user/tss.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/process.o : ${TOP_DIR}/user/process.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/syscall.o : ${TOP_DIR}/lib/user/syscall.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/sys.o : ${TOP_DIR}/user/sys.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/stdio.o : ${TOP_DIR}/lib/stdio.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/ide.o : ${TOP_DIR}/device/ide.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/fs.o : ${TOP_DIR}/fs/fs.c
	${CC} ${CFLAGS} $< -o $@

${OBJS_DIR}/inode.o : ${TOP_DIR}/fs/inode.c
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

${OBJS_DIR}/switch.o : ${TOP_DIR}/thread/switch.S
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
