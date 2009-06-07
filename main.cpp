#include "XmlRpcHttpd.h"
#include <string.h>

int  opterr = 1;
int  optind = 1;
int  optopt;
char *optarg;
 
static int getopt(int argc, char** argv, const char* opts) {
	static int sp = 1;
	register int c;
	register char *cp;

	if(sp == 1) {
		if(optind >= argc ||
				argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if(strcmp(argv[optind], "--") == 0) {
			optind++;
			return(EOF);
		}
	}
	optopt = c = argv[optind][sp];
	if(c == ':' || (cp=strchr((char*)opts, c)) == NULL) {
		if(argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return('?');
	}
	if(*++cp == ':') {
		if(argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if(++optind >= argc) {
			sp = 1;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if(argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}

void logFunc(const XmlRpc::XmlRpcHttpd::HttpdInfo* httpd_info, const tstring& request) {
	printf("%s\n", request.c_str());
}

int main(int argc, char* argv[]) {
	int c;
	const char* root = "./public_html";
	unsigned short port = 80;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	opterr = 0;
	while ((c = getopt(argc, (char**)argv, "p:d:") != -1)) {
		switch (optopt) {
		case 'p': port = (unsigned short)atol(optarg); break;
		case 'd': root = optarg; break;
		case '?': break;
		default:
			argc = 0;
			break;
		}
		optarg = NULL;
	}
	XmlRpc::XmlRpcHttpd httpd(port);
	httpd.loggerfunc = logFunc;
	httpd.bindRoot(root);
#ifdef _WIN32
	httpd.mime_types["cgi"] = "@c:/strawberry/perl/bin/perl.exe";
	httpd.mime_types["php"] = "@c:/progra~1/php/php.exe";
#else
	httpd.mime_types["cgi"] = "@/usr/bin/perl";
	httpd.mime_types["php"] = "@/usr/bin/php-cgi";
#endif
	httpd.start();
	httpd.wait();
	// Ctrl-C to break
}
