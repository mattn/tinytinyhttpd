.SUFFIXES: .cxx .o

all : tthttpd

tthttpd : main.o httpd.o utils.o
	g++ -o $@ main.o httpd.o utils.o -lstdc++ -lpthread

.cxx.o :
	g++ -Wall -c $<

clean :
	rm *.o tthttpd
