#CROSS_COMPILE_PATH=~/Mount/CompileServer/tina/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-arm9-musl/toolchain/bin
#CROSS_COMPILE_PATH=~/tina/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin
#CROSS_COMPILE_PATH=~/tina/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin
#CROSS_COMPILE=$(CROSS_COMPILE_PATH)/arm-openwrt-linux-
LD=$(CROSS_COMPILE)ld
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

CFLAGS= -Wall -fstack-protector -Os

all:
	$(CC) main.c mempool.c $(CFLAGS) -DDEBUG -lpthread -o memorypool

clean:
	@rm -f memorypool
