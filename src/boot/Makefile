
INC_DIR = ./include/

all : 
	nasm -I ${INC_DIR} -o mbr.bin mbr.S
	nasm -I ${INC_DIR} -o loader.bin loader.S
	
clean :
	rm *.bin -f
