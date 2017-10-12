hosts=mail.batten.eu.org pi-one.home.batten.eu.org gromit.cs.bham.ac.uk offsite8.batten.eu.org offsite9.batten.eu.org

OS=$(shell uname -s)
ZIP=socket

CFLAGS=-Wall -Werror -std=gnu99 -g -I/usr/include/postgresql
# needlessly included for single-thread case: hardly a crime
LIBS=-lpthread -lpq
ifeq ($(OS), SunOS)
  LIBS+= -lsocket -lnsl
endif

CC=cc

BIN=multi_thread_server

LOCALLIBDIR = /usr/lib/x86_64-linux-gnu 
LDFLAGS = -L$(LOCALLIBDIR)

common_objs=main.o get_listen_socket.o service_client_socket.o \
	make_printable_address.o read_client_input.o database_connection.o \

multi_objs=service_listen_socket_multithread.o

all_objs=${common_objs} ${multi_objs}

all: ${BIN}

multi_thread_server: ${common_objs} ${multi_objs}
	${CC} -o $@ ${CFLAGS} $+ ${LDFLAGS} ${LIBS}

clean:
	rm -f ${common_objs} ${BIN} ${multi_objs} $(ZIP) *~

