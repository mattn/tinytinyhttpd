.SUFFIXES: .cpp .o

all : tthttpd

tthttpd : main.o XmlRpcHttpd.o XmlRpcUtils.o
	gcc -o $@ main.o XmlRpcHttpd.o XmlRpcUtils.o `pkg-config --libs libxml-2.0` -lcurl -lstdc++

XmlRpcHttpd.cpp : XmlRpcHttpd.h XmlRpcUtils.h XmlRpcValue.h
XmlRpcUtils.cpp : XmlRpcUtils.h XmlRpcValue.h
main.cpp : XmlRpcHttpd.cpp
.cpp.o :
	g++ `pkg-config --cflags libxml-2.0` -c $<
