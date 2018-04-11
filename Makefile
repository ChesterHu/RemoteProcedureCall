CC=g++
AR=ar
FLAG=-std=c++11

.PHONY: all clean

all: utilities.o rpc.o librpc.a binder client.o server.o server_functions.o server_function_skels.o server client

utilities.o: utilities.cpp
	${CC} ${FLAG} -c $^ -o $@

rpc.o: rpc.cpp
	${CC} ${FLAG} -pthread -c $^ -o $@

librpc.a: utilities.o rpc.o
	${AR} rcs $@ $^

binder: binder.cpp utilities.cpp
	${CC} ${FLAG} $^ -o $@

client.o: test.cpp
	${CC} ${FLAG} -pthread -c $^ -o $@

server.o: server_A.cpp
	${CC} ${FLAG} -pthread -c $^ -o $@

server_functions.o: server_A_func.cpp
	${CC} -c $^ -o $@

server_function_skels.o: server_A_skels.cpp
	${CC} -c $^ -o $@

server: server_functions.o server_function_skels.o server.o librpc.a
	g++ -pthread -L. server_functions.o server_function_skels.o server.o -lrpc -o server

client: client.o librpc.a
	g++ -pthread -L. client.o -lrpc -o client

clean:
	rm -f binder server client *.o *.a
