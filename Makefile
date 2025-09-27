compile:
	gcc -Wall -o mkfs.ufs src/mkfsufs.c

install:
	cp ./mkfs.ufs /usr/bin
