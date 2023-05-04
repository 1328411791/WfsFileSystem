all:init_disk WFS
init_disk: init_disk.o
	gcc init_disk.o -o init_disk
WFS: WFS.o
	gcc WFS.o -o WFS -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse3 -lrt -ldl

WFS.o: WFS.c
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o WFS.o WFS.c
init_disk.o: init_disk.c
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o init_disk.o init_disk.c
.PHONY : all
clean :
	rm -f WFS init_disk WFS.o init_disk.o
