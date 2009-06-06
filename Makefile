.SUFFIXES: .cpp .o

all : httpd

httpd : main.o XmlRpcHttpd.o XmlRpcUtils.o
	gcc -o $@ main.o XmlRpcHttpd.o XmlRpcUtils.o `pkg-config --libs libxml-2.0` -lcurl -lstdc++

.cpp.o :
	g++ `pkg-config --cflags libxml-2.0` -c $<
