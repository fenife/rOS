TOP_DIR = .
BOOT_DIR = ${TOP_DIR}/boot
KERNEL_DIR = ${TOP_DIR}/kernel

INC_DIR = ./boot/include/

OBJ :=

all :
	nasm -I ${INC_DIR} -o ${BOOT_DIR}/mbr.bin ${BOOT_DIR}/mbr.S
	nasm -I ${INC_DIR} -o ${BOOT_DIR}/loader.bin ${BOOT_DIR}/loader.S
	gcc -c -o ${KERNEL_DIR}/main.o ${KERNEL_DIR}/main.c
	ld ${KERNEL_DIR}/main.o -Ttext 0xc0001500 -e main -o ${KERNEL_DIR}/kernel.bin

mbr :
	nasm -I ${INC_DIR} -o ${BOOT_DIR}/mbr.bin ${BOOT_DIR}/mbr.S

loader :
	nasm -I ${INC_DIR} -o ${BOOT_DIR}/loader.bin ${BOOT_DIR}/loader.S

kernel :
	gcc -c -o ${KERNEL_DIR}/main.o ${KERNEL_DIR}/main.c
	ld ${KERNEL_DIR}/main.o -Ttext 0xc0001500 -e main -o ${KERNEL_DIR}/kernel.bin

clean :
	rm $(shell find ./ -name *.o) -f
	rm $(shell find ./ -name *.bin) -f
