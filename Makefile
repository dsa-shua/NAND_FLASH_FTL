GXX=gcc
FLAGS=-o
EXECUTABLES=main
SRC=main.c

all:
	${GXX} ${FLAGS} main main.c
	./main

clean:
	rm -rf ${EXECUTABLES}