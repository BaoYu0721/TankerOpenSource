CC = /home/baoyu/OpenWrt-Toolchain-ramips-for-mipsel_24kec+dsp-gcc-4.8-linaro_uClibc-0.9.33.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
#CC = gcc
LDLIBS = -lm -L. -ljsmn -lpthread -ldl
CFLAGS = -Wall -Werror

all: iwl_socket

iwl_socket: iwl_socket.o


clean:
	rm -rf  iwl_socket *.o
