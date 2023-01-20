export
GCC= g++
LIB= -lpthread
STD= -std=c++11

INCLUDE = -I $(shell pwd)/include
CF = -g -D_REENTRANT -fpermissive
CFLAG = ${CF} ${INCLUDE}

.PHONY:all clean 
all: build client server

build:
	${MAKE} -C src/Client all
	${MAKE} -C src/Server all
	${MAKE} -C src/Util all
	@echo -e '\n'Build Finished OK

client:
	${GCC} ${CFLAG} -o $@ src/Client/client.o src/Util/util.o ${LIB} ${STD}

server:
	${GCC} ${CFLAG} -o $@ src/Server/server.o src/Util/util.o ${LIB} ${STD}

clean:
	${MAKE} -C src/Client clean
	${MAKE} -C src/Server clean
	${MAKE} -C src/Util clean
	$(shell rm client)
	$(shell rm server)
	@echo -e '\n'Clean Finished