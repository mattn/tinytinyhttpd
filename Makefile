.SUFFIXES: .cxx .o

CPPFLAGS += -DHAVE_SETRESUID
TAG=`git tag | tail -1`

all : tthttpd

tthttpd : main.o httpd.o utils.o
	g++ -o $@ main.o httpd.o utils.o -lstdc++ -lpthread

.cxx.o :
	g++ -Wall -c ${CPPFLAGS} $<

clean :
	rm *.o tthttpd

dist :
	git archive --format=tar --prefix=tinytinyhttpd-${TAG}/ HEAD | gzip > tinytinyhttpd-${TAG}.tar.gz

dist-upload :
	github-upload tinytinyhttpd-${TAG}.tar.gz mattn/tinytinyhttpd

