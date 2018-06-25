# makefile

CFLAGS = -I ../../include

all: feather

http.c: http.rl
	ragel $<

feather: http.c connect.c
	gcc $(CFLAGS) -o $@ http.c connect.c

run: feather
	./feather
