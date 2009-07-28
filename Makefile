.SUFFIXES: .cxx .o

all : tthttpd

tthttpd : main.o httpd.o utils.o
	gcc -o $@ main.o httpd.o utils.o -lstdc++ -lpthread

.cxx.o :
	g++ -c $<
