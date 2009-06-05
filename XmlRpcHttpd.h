#ifndef _XMLRPCHTTPD_H_
#define _XMLRPCHTTPD_H_

#define XMLRPCHTTPD_VERSION 0x0100

#pragma warning(disable:4018 4503 4530 4786)
#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#include <process.h>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <sstream>

#include "XmlRpcUtils.h"

namespace XmlRpc {

class XmlRpcHttpd {
public:
	typedef struct {
		std::string name;
		unsigned long size;
		bool isdir;
		struct tm date;
	} ListInfo;
	typedef struct {
		int msgsock;
		XmlRpcHttpd *httpd;
		tstring address;
	} HttpdInfo;
	typedef std::map<tstring, tstring> BasicAuths;
	typedef struct {
		std::vector<tstring> accept_list;
	} AcceptAuth;
	typedef std::map<tstring, AcceptAuth> AcceptAuths;
	typedef std::vector<tstring> AcceptIPs;

	typedef XmlRpcValue (*XmlRpcFunc)(const std::vector<XmlRpcValue>& requests);
	typedef void (*XmlRpcLoggerFunc)(const HttpdInfo* httpd_info, const tstring& request);
	typedef std::map<tstring, tstring> MimeTypes;
	typedef std::vector<tstring> DefaultPages;

private:
	std::map<tstring, XmlRpcFunc> callbacks;
#ifdef _WIN32
	HANDLE thread;
#else
	pthread_t thread;
#endif

public:
	int sock;
	std::string hostname;
	std::string hostaddr;
	std::string target;
	std::string root;
	std::string fs_charset;
	unsigned short port;
	BasicAuths basic_auths;
	AcceptAuths accept_auths;
	AcceptIPs accept_ips;
	MimeTypes mime_types;
	DefaultPages default_pages;
	XmlRpcLoggerFunc loggerfunc;

	void initialize() {
		sock = -1;
		port = 80;
		target = "/RPC2";
		fs_charset = "utf-8";
		thread = 0;
		loggerfunc = NULL;
		mime_types[_T("gif")] = _T("image/gif");
		mime_types[_T("jpg")] = _T("image/jpeg");
		mime_types[_T("png")] = _T("image/png");
		mime_types[_T("htm")] = _T("text/html");
		mime_types[_T("html")] = _T("text/html");
		mime_types[_T("txt")] = _T("text/plain");
		mime_types[_T("xml")] = _T("text/xml");
		mime_types[_T("js")] = _T("application/x-javascript");
		mime_types[_T("css")] = _T("text/css");
		default_pages.push_back(_T("index.html"));
		default_pages.push_back(_T("index.php"));
		default_pages.push_back(_T("index.cgi"));
	};

	XmlRpcHttpd() {
		initialize();
	}
	XmlRpcHttpd(unsigned short _port) {
		initialize();
		port = _port;
	}
	XmlRpcHttpd(unsigned short _port, tstring _target) {
		initialize();
		port = _port;
		target = tstring2string(_target);
	}
	~XmlRpcHttpd() {
		stop();
	}
	bool start();
	bool stop();
	bool wait();
	void set_fs_charset(tstring _fs_charset) {
		fs_charset = tstring2string(_fs_charset);
	}
	tstring get_fs_charset() {
		return string2tstring(fs_charset);
	}
	void registerXmlRpcFunc(tstring name, XmlRpcFunc func) {
		callbacks[name] = func;
		accept_auths.erase(name);
	}
	void registerXmlRpcFunc(tstring name, XmlRpcFunc func, std::vector<tstring> _accept_auth) {
		callbacks[name] = func;
		accept_auths[name].accept_list = _accept_auth;
	}
	void unregisterXmlRpcFunc(tstring name) {
		callbacks.erase(name);
	}
	bool dispatchXmlRpcFunc(tstring name, std::vector<XmlRpcValue>& requests, XmlRpcValue& response);
	void setAuthentication(BasicAuths _basic_auths) {
		basic_auths = _basic_auths;
	}
	void bindRoot(tstring _root) {
		root = get_realpath(tstring2string(_root));
	}
	static std::string get_realpath(std::string path) {
		std::replace(path.begin(), path.end(), '\\', '/');
		int end_pos = path.find_last_of("?#");
		if (end_pos != -1) path.resize(end_pos);

		std::vector<std::string> path_sep = split_string(path, "/");
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
		if (path[path.size()-1] == '/')
			path_real += "/";
		return path_real;
	}
};

void registerXmlRpcFunc(tstring name, XmlRpcHttpd::XmlRpcFunc func);
void unregisterXmlRpcFunc(tstring name);
bool dispatchXmlRpcFunc(tstring name, std::vector<XmlRpcValue>& requests, XmlRpcValue& response);

}

#endif /* _XMLRPCHTTPD_H_ */
