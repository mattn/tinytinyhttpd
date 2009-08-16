#include "httpd.h"
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

bool loadAuthfile(const char* filename, std::vector<tthttpd::server::AuthInfo>& auths) {
	char buffer[BUFSIZ];
	auths.clear();
	FILE* fp = fopen(filename, "r");
	if (!fp) return false;
	while(fp && fgets(buffer, sizeof(buffer), fp)) {
		char* line = buffer;
		char* ptr = strpbrk(line, "\r\n");
		if (ptr) *ptr = 0;
		ptr = strchr(line, ':');
		if (ptr) *ptr++ = 0;
		tthttpd::server::AuthInfo info;
		info.user = line;
		info.pass = ptr;
		auths.push_back(info);
	}
	fclose(fp);
	return true;
}

int main(int argc, char* argv[]) {
	int c;
	const char* root = ".";
	unsigned short port = 8080;
	const char* cfg = NULL;
	bool spawn_exec = false;
	int verbose = 0;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	opterr = 0;
	while ((c = getopt(argc, (char**)argv, "p:c:d:xvh") != -1)) {
		switch (optopt) {
		case 'p': port = (unsigned short)atol(optarg); break;
		case 'c': cfg = optarg; break;
		case 'd': root = optarg; break;
		case 'v': verbose++; break;
		case 'x': spawn_exec = true; break;
		case 'h':
			  std::cerr << "usage: " << argv[0] << " [-v] [-p server-port] [-c config-file] [-d root-dir]" << std::endl;
			  exit(1);
			  break;
		case '?': break;
		default:
			argc = 0;
			break;
		}
		optarg = NULL;
	}

	tthttpd::server httpd(port);
	httpd.bindRoot(root);
	httpd.spawn_executable = spawn_exec;
	httpd.verbose_mode = verbose;
	if (cfg) {
		ConfigList configs = loadConfigs(cfg);
		Config config;
		Config::iterator it;
		std::string val;

		val = configs["global"]["path"];
#ifdef _WIN32
		if (val.size()) {
			std::string tmp = "PATH=";
			tmp += val;
			putenv(tmp.c_str());
		}
#else
		if (val.size()) setenv("PATH", val.c_str(), true);
#endif
		val = configs["global"]["root"];
		if (val.size()) httpd.bindRoot(val);
		val = configs["global"]["port"];
		if (val.size()) httpd.port = (unsigned short)atoi(val.c_str());
		val = configs["global"]["indexpages"];
		if (val.size()) httpd.default_pages = tthttpd::split_string(val, ",");
		val = configs["global"]["charset"];
		if (val.size()) httpd.fs_charset = val;
		val = configs["global"]["chroot"];
		if (val.size()) httpd.chroot = val;
		val = configs["global"]["user"];
		if (val.size()) httpd.user = val;
		val = configs["global"]["debug"];
		if (val == "on") httpd.verbose_mode = 1;
		else if (val.size()) httpd.verbose_mode = atol(val.c_str());
		val = configs["global"]["spawnexec"];
		if (val == "on") httpd.spawn_executable = true;

		config = configs["request/aliases"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.request_aliases[it->first] = it->second;

		config = configs["mime/types"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.mime_types[it->first] = it->second;

		config = configs["request/environments"];
		for (it = config.begin(); it != config.end(); it++)
			httpd.request_environments[it->first] = it->second;

		config = configs["authentication"];
		for (it = config.begin(); it != config.end(); it++) {
			tthttpd::server::BasicAuthInfo basic_auth_info;
			basic_auth_info.target = it->first;
			std::vector<std::string> infos = tthttpd::split_string(it->second, ",");
			basic_auth_info.method = infos[0];
			basic_auth_info.realm = infos[1];
			std::vector<tthttpd::server::AuthInfo> auth_infos;
			if (loadAuthfile(infos[2].c_str(), auth_infos))
				basic_auth_info.auths = auth_infos;
			httpd.basic_auths.push_back(basic_auth_info);
		}


	} else {
#ifdef _WIN32
	httpd.mime_types["cgi"] = "@c:/strawberry/perl/bin/perl.exe";
	httpd.mime_types["php"] = "@c:/progra~1/php/php-cgi.exe";
	httpd.mime_types["rb"] = "@c:/ruby/bin/ruby.exe";
#else
	httpd.mime_types["cgi"] = "@/usr/bin/perl";
	httpd.mime_types["php"] = "@/usr/bin/php-cgi";
	httpd.mime_types["rb"] = "@/usr/bin/ruby";
#endif
	}

	httpd.start();
	httpd.wait();
	// Ctrl-C to break

	return 0;
}
