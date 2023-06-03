all:init_disk WFS
init_disk: init_disk.o
	gcc init_disk.o -o init_disk
WFS: WFS.o hashmap.o
	gcc WFS.o hashmap.o -o WFS -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse3 -lrt -ldl
WFS.o: WFS.c
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o WFS.o WFS.c
hashmap.o: hashmap.c
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o hashmap.o hashmap.c
init_disk.o: init_disk.c
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o init_disk.o init_disk.c

.PHONY : all

clean :
	rm -rf WFS init_disk WFS.o init_disk.o
	@if findmnt -n ./tt; \
    then \
        echo "./tt is mounted"; \
		fusermount -u tt; \
    else \
        echo "./tt is not mounted"; \
    fi
.PHONY : clean

cp_diskimg:
	cp ./diskimg /tmp/
.PHONY : cp_diskimg

run:init_disk WFS
	echo "run"
	./WFS tt
.PHONY : run