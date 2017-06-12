; boot.inc
; --- loader 和 kernel 的配置信息 ---

; loader 被加载到内存的地址
LOADER_BASE_ADDR  	equ 0x900

; 硬盘采用LBA扇区编址方式，MBR位于第0个扇区，
; loader位于第2个扇区（即 loader 在硬盘上的逻辑扇区地址）
LOADER_START_SECTOR	equ 0x2			
