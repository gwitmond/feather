# makefile

http.c: http.rl
	ragel $<

feather: http.c connect.c
	gcc -o $@ $<

run: feather
	./feather
