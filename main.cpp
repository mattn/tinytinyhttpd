#include "XmlRpcHttpd.h"
#include <string.h>
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

typedef std::map<std::string, std::string>	Config;
typedef std::map<std::string, Config>		ConfigList;
ConfigList loadConfigs(const char* filename) {
	ConfigList configs;
	Config config;
	char buffer[BUFSIZ];
	FILE* fp = fopen(filename, "r");
	std::string profile = "global";
	while(fp && fgets(buffer, sizeof(buffer), fp)) {
		char* line = buffer;
		char* ptr = strpbrk(line, "\r\n");
		if (ptr) *ptr = 0;
		ptr = strchr(line, ']');
		if (*line == '[' && ptr) {
			*ptr = 0;
			if (config.size())
				configs[profile] = config;
			config.clear();
			profile = line+1;
			continue;
		}
		ptr = strchr(line, '=');
		if (ptr && *line != ';') {
			*ptr++ = 0;
			config[line] = ptr;
		}
	}
	configs[profile] = config;
	if (fp) fclose(fp);
	return configs;
}

int main(int argc, char* argv[]) {
	int c;
	const char* root = "./public_html";
	unsigned short port = 80;
	const char* cfg = NULL;
	bool verbose = false;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	opterr = 0;
	while ((c = getopt(argc, (char**)argv, "p:c:d:v") != -1)) {
		switch (optopt) {
		case 'p': port = (unsigned short)atol(optarg); break;
		case 'c': cfg = optarg; break;
		case 'd': root = optarg; break;
		case 'v': verbose = true; break;
		case '?': break;
		default:
			argc = 0;
			break;
		}
		optarg = NULL;
	}

	XmlRpc::XmlRpcHttpd httpd(port);
	//httpd.loggerfunc = logFunc;
	httpd.bindRoot(XmlRpc::string2tstring(root));
	httpd.debug_mode = verbose;
	if (cfg) {
		ConfigList configs = loadConfigs(cfg);
		Config config;
		Config::iterator it;
		std::string val;

		val = configs["global"]["root"];
		if (val.size()) httpd.bindRoot(XmlRpc::string2tstring(val));
		val = configs["global"]["port"];
		if (val.size()) httpd.port = atol(val.c_str());
		val = configs["global"]["indexpages"];
		if (val.size()) httpd.default_pages = XmlRpc::split_string(XmlRpc::string2tstring(val), _T(","));
		val = configs["global"]["charset"];
		if (val.size()) httpd.fs_charset = val;
		val = configs["global"]["debug"];
		if (val == "on") httpd.debug_mode = true;

		config = configs["request/aliases"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.request_aliases[XmlRpc::string2tstring(it->first)] = XmlRpc::string2tstring(it->second);

		config = configs["mime/types"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.mime_types[XmlRpc::string2tstring(it->first)] = XmlRpc::string2tstring(it->second);

		config = configs["request/environments"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.request_environments[XmlRpc::string2tstring(it->first)] = XmlRpc::string2tstring(it->second);

	} else {
#ifdef _WIN32
	httpd.mime_types[_T("cgi")] = _T("@c:/strawberry/perl/bin/perl.exe");
	httpd.mime_types[_T("php")] = _T("@c:/progra~1/php/php-cgi.exe");
#else
	httpd.mime_types[_T("cgi")] = _T("@/usr/bin/perl");
	httpd.mime_types[_T("php")] = _T("@/usr/bin/php-cgi");
#endif
	}
	httpd.start();
	httpd.wait();
	// Ctrl-C to break
}
