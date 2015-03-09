CC = gcc

LIBS =  -lpthread\
	/users/cse533/Stevens/unpv13e/libunp.a

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/users/cse533/Stevens/unpv13e/lib

all: client_smaguluri server_smaguluri ODR_smaguluri
 
client_smaguluri: client_smaguluri.o get_hw_addrs.o
	${CC} ${FLAGS} -o client_smaguluri client_smaguluri.o get_hw_addrs.o ${LIBS}

server_smaguluri: server_smaguluri.o get_hw_addrs.o
	${CC} ${FLAGS} -o server_smaguluri server_smaguluri.o ${LIBS}

ODR_smaguluri: ODR_smaguluri.o get_hw_addrs.o get_hw_addrs.o
	${CC} ${FLAGS} -o ODR_smaguluri ODR_smaguluri.o get_hw_addrs.o ${LIBS}

client_smaguluri.o : client_smaguluri.c
	${CC} ${CFLAGS} -c client_smaguluri.c

server_smaguluri.o : server_smaguluri.c
	${CC} ${CFLAGS} -c server_smaguluri.c

ODR_smaguluri.o : ODR_smaguluri.c
	${CC} ${CFLAGS} -c ODR_smaguluri.c


clean:
	rm server_smaguluri.o server_smaguluri client_smaguluri.o client_smaguluri ODR_smaguluri.o ODR_smaguluri get_hw_addrs.o

