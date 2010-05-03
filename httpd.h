/* Copyright 2009 by Yasuhiro Matsumoto
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _HTTPD_H_
#define _HTTPD_H_

#define HTTPD_VERSION 0x0006

#ifdef _MSC_VER
#pragma warning(disable:4018 4503 4530 4786 4996)
#pragma comment(lib, "ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <limits.h>
#include <unistd.h>
#endif
#include <sstream>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "utils.h"

namespace tthttpd {

class server {
public:
	typedef struct {
		std::string name;
		unsigned long size;
		bool isdir;
		struct tm date;
	} ListInfo;
	typedef struct {
		int msgsock;
		server *httpd;
		std::string address;
		std::string port;
		int servno;
	} HttpdInfo;
	typedef struct {
		std::string user;
		std::string pass;
	} AuthInfo;
	typedef struct {
		std::string target;
		std::string method;
		std::string realm;
		std::vector<AuthInfo> auths;
	} BasicAuthInfo;
	typedef std::vector<BasicAuthInfo> BasicAuths;
	typedef struct {
		std::vector<std::string> accept_list;
	} AcceptAuth;
	typedef std::map<std::string, AcceptAuth> AcceptAuths;
	typedef std::vector<std::string> AcceptIPs;

	typedef void (*LoggerFunc)(const HttpdInfo* httpd_info, const std::string& request);
	typedef std::map<std::string, std::string> MimeTypes;
	typedef std::vector<std::string> DefaultPages;
	typedef std::map<std::string, std::string> RequestAliases;
	typedef std::map<std::string, std::string> RequestEnvironments;

private:
#ifdef _WIN32
	HANDLE thread;
#else
	pthread_t thread;
#endif

public:
	std::vector<unsigned int> socks;
	int family;
	std::string hostname;
	std::vector<std::string> hostaddr;
	std::string root;
	std::string fs_charset;
	std::string chroot;
	std::string user;
	std::string port;
	std::string default_cgi;
	BasicAuths basic_auths;
	AcceptAuths accept_auths;
	AcceptIPs accept_ips;
	MimeTypes mime_types;
	DefaultPages default_pages;
	RequestAliases request_aliases;
	RequestEnvironments request_environments;
	LoggerFunc loggerfunc;
	bool spawn_executable;
	int verbose_mode;

	void initialize() {
		port = "www";
		fs_charset = "utf-8";
		thread = 0;
		loggerfunc = NULL;
		mime_types["gif"] = "image/gif";
		mime_types["jpg"] = "image/jpeg";
		mime_types["png"] = "image/png";
		mime_types["htm"] = "text/html";
		mime_types["html"] = "text/html";
		mime_types["txt"] = "text/plain";
		mime_types["xml"] = "text/xml";
		mime_types["js"] = "application/x-javascript";
		mime_types["css"] = "text/css";
		default_pages.push_back("index.html");
		default_pages.push_back("index.php");
		default_pages.push_back("index.rb");
		default_pages.push_back("index.cgi");
		spawn_executable = false;
		verbose_mode = 0;
	};

	server() {
		initialize();
	}
	server(std::string _port) {
		initialize();
		port = _port;
	}
	~server() {
		stop();
	}
	bool start();
	bool stop();
	bool wait();
	bool is_running();
	void set_fs_charset(std::string _fs_charset) {
		fs_charset = _fs_charset;
	}
	std::string get_fs_charset() {
		return fs_charset;
	}
	void setAuthentication(BasicAuths& _basic_auths) {
		basic_auths = _basic_auths;
	}
	void bindRoot(std::string _root) {
		root = get_realpath(_root + "/");
	}
	static std::string get_realpath(std::string abspath) {
		std::string path = abspath;
#ifdef _WIN32
		char fullpath[_MAX_PATH] = {0};
		char* filepart = NULL;
		if (GetFullPathNameA(path.c_str(), _MAX_PATH, fullpath, &filepart))
			path = fullpath;
#else
		char fullpath[PATH_MAX] = {0};
		if (realpath((char*)path.c_str(), fullpath)) {
			path = fullpath;
		}
#endif
		std::replace(path.begin(), path.end(), '\\', '/');
		size_t end_pos = path.find_last_of('?');
		if (end_pos != std::string::npos) path.resize(end_pos);

		std::vector<std::string> path_sep;
		split_string(path, "/", path_sep);
		std::vector<std::string>::iterator it;
		while(true) {
			it = std::find(path_sep.begin(), path_sep.end(), "..");
			if (it == path_sep.end()) break;
			if (it == path_sep.begin()) {
				continue;
			}
			path_sep.erase(it-1);
			path_sep.erase(it-1);
		}
		std::string path_real;
		for(it = path_sep.begin(); it != path_sep.end(); it++) {
			path_real += *it;
			if (it+1 != path_sep.end())
				path_real += "/";
		}
		if (abspath[abspath.size()-1] == '/')
			path_real += "/";
		return path_real;
	}
};

}

#endif /* _HTTPD_H_ */
