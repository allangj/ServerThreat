#
#////////////Master Makefile////////////
#

.PHONY: build chkconfig preconfig buildfs install clean


all: build

build:
		(cd src; gcc -O  -DLINUX  nweb.c -o ../bin/nweb)
		(cd src; gcc -O  -DLINUX -lpthread  pnweb.c -o ../bin/pnweb)

chkconfig:

preconfig:

buildfs:

install:

clean:
		(cd bin/; rm nweb)
		(cd bin/; rm pnweb)
