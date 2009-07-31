.SUFFIXES: .cxx .o

CPPFLAGS += -DHAVE_SETRESUID

all : tthttpd

tthttpd : main.o httpd.o utils.o
	g++ -o $@ main.o httpd.o utils.o -lstdc++ -lpthread

.cxx.o :
	g++ -Wall -c ${CPPFLAGS} $<

clean :
	rm *.o tthttpd
