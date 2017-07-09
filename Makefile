.DEFAULT: redsimg

redsimg: redsimg.o
	gcc -o redsimg redsimg.c redsfs.c

redsimg-dbg: redsimg.o
	gcc -g -o redsimg redsimg.c redsfs.c

clean:
	rm *.o redsimg
	rm -rf redsimg.dSYM/
