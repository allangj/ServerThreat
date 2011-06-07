#
#////////////Master Makefile////////////
#

.PHONY: build chkconfig preconfig buildfs install clean


all: build

build:
		(cd src; gcc -O  -DLINUX  nweb.c -o ../bin/nweb)
sim:

chkconfig:

preconfig:

buildfs:

install:

clean:
		(cd bin/; rm nweb)
