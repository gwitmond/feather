TARGET = feather
SRC_CC = http.cc connect.cc
LIBS = libc libc_lwip libc_lwip_nic_dhcp posix

http.cc: http.rl
	ragel -o $@ $<
