TARGET = feather
SRC_C = http.c connect.c
LIBS = libc libc_lwip libc_lwip_nic_dhcp

http.c: http.rl
	ragel -o $@ $<