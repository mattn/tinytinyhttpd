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
#include "httpd.h"
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace tthttpd {

#ifdef _WIN32
typedef int socklen_t;
#else
#define closesocket(x) close(x)
#define strnicmp(x, y, z) strncasecmp(x, y, z)
#endif

#ifndef S_ISREG
#define S_ISREG(x) (x & S_IFREG)
#endif

#define VERBOSE(x) (httpd->verbose_mode >= x)

typedef struct {
#ifdef _WIN32
	HANDLE read;
	HANDLE write;
	HANDLE process;
#else
	FILE* read;
	FILE* write;
	pid_t process;
#endif
} RES_INFO;

bool operator<(const server::ListInfo& left, const server::ListInfo& right) {
  return left.name < right.name;
}

bool operator>(const server::ListInfo& left, const server::ListInfo& right) {
  return left.name > right.name;
}

const char * const months[]={
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"};
const char * const wdays[]={
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"};

#ifdef _WIN32
static bool filetime2unixtime(const FILETIME* ft, struct tm* tm) {
    FILETIME lt;
    SYSTEMTIME st;

    if (!FileTimeToLocalFileTime(ft, &lt)) return false;
    if (!FileTimeToSystemTime(&lt, &st)) return false;
    memset(tm, 0, sizeof(tm));
    tm->tm_year = st.wYear - 1900;
    tm->tm_mon = st.wMonth - 1;
    tm->tm_mday = st.wDay;
    tm->tm_hour = st.wHour;
    tm->tm_min = st.wMinute;
    tm->tm_sec = st.wSecond;
    return true;
}
#endif

static std::string res_curtime(int diff = 0) {
	time_t tt = time(NULL) + diff;
	struct tm* p = gmtime(&tt);

	char buf[256];
	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		wdays[p->tm_wday],
		p->tm_mday,
		months[p->tm_mon],
		p->tm_year+1900,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	return buf;
}

static void my_perror(std::string mes) {
#ifdef _WIN32
	void*	pMsgBuf;
	FormatMessageA(
		 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 GetLastError(),
		 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		 (LPSTR) &pMsgBuf,
		 0,
		 NULL
	);

	std::string err = (LPSTR)pMsgBuf;
	LocalFree(pMsgBuf);
	err.erase(std::remove(err.begin(), err.end(), '\r'), err.end());
	err.erase(std::remove(err.begin(), err.end(), '\n'), err.end());
	fprintf(stderr, "%s: %s\n", mes.c_str(), err.c_str());
#else
	perror(mes.c_str());
#endif
}

#ifdef _WIN32
static RES_INFO* res_fopen(std::string file) {
	HANDLE hFile;
	hFile = CreateFileA(
		file.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	RES_INFO* res_info = new RES_INFO;
	res_info->read = hFile;
	res_info->write = 0;
	res_info->process = 0;
	return res_info;
}

static bool res_isdir(std::string file) {
	DWORD dwAttr = GetFileAttributesA(file.c_str());
	return (dwAttr != (DWORD)-1 && (dwAttr & FILE_ATTRIBUTE_DIRECTORY));
}

static bool res_isexe(std::string& file, std::string& path_info, std::string& script_name) {
	std::vector<std::string> split_path = split_string(file, "/");
	std::string path = "";
	std::string pathext = getenv("PATHEXT");
	std::transform(pathext.begin(), pathext.end(), pathext.begin(), ::tolower);

	std::vector<std::string> pathexts = split_string(pathext, ";");
	std::vector<std::string>::iterator itext;

	for (std::vector<std::string>::iterator it = split_path.begin(); it != split_path.end(); it++) {
		if (it->empty()) continue;
		if (!path.empty()) path += "/";
		path += *it;
    	struct stat	st;
		stat((char *)path.c_str(), &st);
		if (S_ISREG(st.st_mode) && stat((char *)path.c_str(), &st) == 0) {
			for (itext = pathexts.begin(); itext != pathexts.end(); itext++) {
				if (path.substr(path.size() - itext->size()) == *itext) {
					path_info = file.c_str() + path.size();
					script_name.resize(script_name.size() - path_info.size());
					file = path;
					if (path_info.empty()) script_name += *it;
					return true;
				}
			}
		}
		for (itext = pathexts.begin(); itext != pathexts.end(); itext++) {
			std::string tmp = path + *itext;
			stat((char *)tmp.c_str(), &st);
			if (S_ISREG(st.st_mode) && stat((char *)tmp.c_str(), &st) == 0) {
				path_info = file.c_str() + path.size();
				script_name.resize(script_name.size() - path_info.size());
				file = tmp;
				if (path_info.empty()) script_name += *it;
				return true;
			}
		}
	}
	return false;
}

static std::vector<server::ListInfo> res_flist(std::string path) {
	WIN32_FIND_DATAA fData;
	std::vector<server::ListInfo> ret;
	if (path.size() && path[path.size()-1] != '/')
		path += "/";
	std::string pattern = path + "*";
	HANDLE hFind = FindFirstFileA(pattern.c_str(), &fData);

	do {
		if (hFind == INVALID_HANDLE_VALUE) break;
		server::ListInfo listInfo;
		listInfo.name = fData.cFileName;
		listInfo.isdir = (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			? true : false;
		listInfo.size = fData.nFileSizeLow;
		filetime2unixtime(&fData.ftLastWriteTime, &listInfo.date);
		ret.push_back(listInfo);
	} while(FindNextFileA(hFind, &fData));
	if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
	sort(ret.begin(), ret.end());
	return ret;
}

static unsigned long res_fsize(RES_INFO* res_info) {
	return GetFileSize(res_info->read, NULL);
}

static std::string res_ftime(std::string file, int diff = 0) {
	HANDLE hFile;
	hFile = CreateFileA(
		file.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return "";
	FILETIME filetime = {0};
	SYSTEMTIME systemtime = {0};
	GetFileTime(hFile, NULL, NULL, &filetime);
	CloseHandle(hFile);
	FileTimeToSystemTime(&filetime, &systemtime);
	struct tm t = {0};
	t.tm_year = systemtime.wYear-1900;
	t.tm_mon = systemtime.wMonth;
	t.tm_mday = systemtime.wDay;
	t.tm_hour = systemtime.wHour;
	t.tm_min = systemtime.wMinute;
	t.tm_sec = systemtime.wSecond;
	t.tm_isdst = 0;
	time_t tt = mktime(&t) + diff;
	struct tm* p = localtime(&tt);
	//int offset= -(int)timezone;
	//offset = offset/60/60*100 + (offset/60)%60;

	char buf[256];
	//sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d %+05d",
	//	wdays[p->tm_wday],
	//	p->tm_mday,
	//	months[p->tm_mon-1],
	//	p->tm_year+1900,
	//	p->tm_hour,
	//	p->tm_min,
	//	p->tm_sec,
	//	offset);
	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		wdays[p->tm_wday],
		p->tm_mday,
		months[p->tm_mon],
		p->tm_year+1900,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	return buf;
}

static std::string res_fgets(RES_INFO* res_info) {
	std::string ret;
	while(true) {
		DWORD dwRead;
		char buf[2] = {0};
		if (ReadFile(res_info->read, buf, 1, &dwRead, NULL) == FALSE) break;
		if (buf[0] == '\n') break;
		if (buf[0] != '\r') ret += buf;
	}
	return ret;
}

static unsigned long res_write(RES_INFO* res_info, char* data, unsigned long size) {
	DWORD dwWrite = 0;
	WriteFile(res_info->write, data, size, &dwWrite, NULL);
	return dwWrite;
}

static unsigned long res_read(RES_INFO* res_info, char* data, unsigned long size) {
	DWORD dwRead;
	if (ReadFile(res_info->read, data, size, &dwRead, NULL) == FALSE)
		return 0;
	return dwRead;
}

static RES_INFO* res_popen(std::vector<std::string> args, std::vector<std::string> envs) {
	int envs_len = 1;
	int n;
	char *envs_ptr;
	char *ptr;
	std::vector<std::string>::const_iterator it;
	std::string command;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	HANDLE hClientOut_rd, hClientOut_wr; 
	HANDLE hClientIn_rd, hClientIn_wr;
  
	if(!CreatePipe(&hClientOut_rd, &hClientOut_wr, NULL, 0)) {
		return NULL;
	}

	if(!CreatePipe(&hClientIn_rd,&hClientIn_wr,NULL,0)) {
		CloseHandle(hClientOut_rd);
		CloseHandle(hClientOut_wr);
		return NULL;
	}

#ifdef NT
	SetHandleInformation(
		hClientOut_wr,
		HANDLE_FLAG_INHERIT,
		HANDLE_FLAG_INHERIT);
	SetHandleInformation(
		hClientIn_rd,
		HANDLE_FLAG_INHERIT,
		HANDLE_FLAG_INHERIT);
#else
	DuplicateHandle(
		GetCurrentProcess(),
		hClientOut_wr,
		GetCurrentProcess(),
		&hClientOut_wr,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
	DuplicateHandle(
		GetCurrentProcess(),
		hClientIn_rd,
		GetCurrentProcess(),
		&hClientIn_rd,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
#endif

	memset(&si,0,sizeof(STARTUPINFOA));
	si.cb         = sizeof(STARTUPINFOA);
	si.dwFlags    = STARTF_USESTDHANDLES;
	si.hStdInput  = hClientIn_rd;
	si.hStdOutput = hClientOut_wr;
	si.hStdError  = hClientOut_wr;

	for(it = args.begin(), n = 0; it != args.end(); it++, n++) {
		if (it != args.begin()) command += " ";
		command += *it;
	}

	for(it = envs.begin(); it != envs.end(); it++)
		envs_len += (int)it->size() + 1;
	envs_ptr = new char[envs_len];
	memset(envs_ptr, 0, envs_len);
	ptr = envs_ptr;
	for(it = envs.begin(); it != envs.end(); it++) {
		strcpy(ptr, it->c_str());
		ptr += it->size() + 1;
	}

	std::string path = args.size() > 1  ? args[1] : args[0];
	size_t end_pos = path.find_last_of('/');
	if (end_pos != std::string::npos) path.erase(end_pos);

	BOOL bRet = CreateProcessA(
			NULL,
			(char*)command.c_str(),
			NULL,
			NULL,
			TRUE,
			NORMAL_PRIORITY_CLASS | DETACHED_PROCESS,
			envs_ptr,
			path.c_str(),
			&si,
			&pi);
	delete envs_ptr;
	if(!bRet) {
		my_perror("CreateProcessA");
		CloseHandle(hClientIn_rd);
		CloseHandle(hClientOut_wr);
		return NULL;
	}
	
	CloseHandle(pi.hThread);

	CloseHandle(hClientIn_rd);
	CloseHandle(hClientOut_wr);

	RES_INFO* res_info = new RES_INFO;
	res_info->read = hClientOut_rd;
	res_info->write = hClientIn_wr;
	res_info->process = pi.hProcess;
	return res_info;
}

static void res_close(RES_INFO* res_info) {
	if (res_info) {
		if (res_info->read) CloseHandle(res_info->read);
		if (res_info->write) CloseHandle(res_info->write);
		if (res_info->process) CloseHandle(res_info->process);
		delete res_info;
	}
}
#else
static RES_INFO* res_fopen(std::string file) {
	FILE* fp = fopen(file.c_str(), "rb");
	if (!fp)
		return NULL;

	RES_INFO* res_info = new RES_INFO;
	res_info->read = fp;
	res_info->write = 0;
	res_info->process = 0;
	return res_info;
}

static bool res_isdir(std::string file) {
	struct stat statbuf = {0};
	stat(file.c_str(), &statbuf);
	return statbuf.st_mode & S_IFDIR;
}

static bool res_isexe(std::string& file, std::string& path_info, std::string& script_name) {
	std::vector<std::string> split_path = split_string(file, "/");
	std::string path = "";
	for (std::vector<std::string>::iterator it = split_path.begin(); it != split_path.end(); it++) {
		if (it->empty()) continue;
		path += "/";
		path += *it;
    	struct stat	st;
		if (stat((char *)path.c_str(), &st))
			continue;
    	if (S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0) {
			path_info = file.c_str() + path.size();
			script_name.resize(script_name.size() - path_info.size());
			file = path;
			if (path_info.empty()) script_name += *it;
			return true;
		}
	}
	return false;
}

static std::vector<server::ListInfo> res_flist(std::string path) {
	std::vector<server::ListInfo> ret;
	DIR* dir;
	struct dirent* dirp;
	if (path.size() && path[path.size()-1] != '/')
		path += "/";
	dir = opendir(path.c_str());
	while((dirp = readdir(dir))) {
		server::ListInfo listInfo;
		listInfo.name = dirp->d_name;
		std::string file = path + listInfo.name;
		struct stat statbuf = {0};
		stat(file.c_str(), &statbuf);
		listInfo.size = statbuf.st_size;
		memcpy(&listInfo.date, gmtime(&statbuf.st_mtime), sizeof(struct tm));
		listInfo.isdir = res_isdir(file);
		ret.push_back(listInfo);
	}
	closedir(dir);
	sort(ret.begin(), ret.end());
	return ret;
}

static unsigned long res_fsize(RES_INFO* res_info) {
	struct stat statbuf = {0};
	fstat(fileno(res_info->read), &statbuf);
	return statbuf.st_size;
}

static std::string res_ftime(std::string file, int diff = 0) {
	struct stat statbuf = {0};
	stat(file.c_str(), &statbuf);
	time_t tt = statbuf.st_mtime + diff;
	struct tm* p=gmtime(&tt);
	//int	offset;
	//int offset= -(int)timezone;
	//offset = offset/60/60*100 + (offset/60)%60;

	char buf[256];
	//sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d %+05d",
	//	wdays[p->tm_wday],
	//	p->tm_mday,
	//	months[p->tm_mon-1],
	//	p->tm_year+1900,
	//	p->tm_hour,
	//	p->tm_min,
	//	p->tm_sec,
	//	offset);
	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		wdays[p->tm_wday],
		p->tm_mday,
		months[p->tm_mon],
		p->tm_year+1900,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	return buf;
}

static std::string res_fgets(RES_INFO* res_info) {
	std::string ret;
	char buf[BUFSIZ], *ptr;
	memset(buf, 0, sizeof(buf));
	fgets(buf, sizeof(buf), res_info->read);
	ptr = strpbrk(buf, "\r\n");
	if (ptr) *ptr = 0;
	ret = buf;
	return ret;
}

static unsigned long res_write(RES_INFO* res_info, char* data, unsigned long size) {
	return fwrite(data, 1, size, res_info->write);
}

static unsigned long res_read(RES_INFO* res_info, char* data, unsigned long size) {
	if (feof(res_info->read)) return 0;
	return fread(data, 1, size, res_info->read);
}

static void res_popen_sigchild(int signo) {
    wait(0);
}

static RES_INFO* res_popen(std::vector<std::string> args, std::vector<std::string> envs) {
    int filedesr[2], filedesw[2];
    pid_t child;
    long flags;
    sigset_t newmask;
	char** args_ptr;
	char** envs_ptr;
	int n;
	std::vector<std::string>::iterator it;

	args_ptr = new char*[args.size() + 1];
	envs_ptr = new char*[envs.size() + 1];

	for(n = 0, it = args.begin(); it != args.end(); n++, it++)
		args_ptr[n] = (char*)it->c_str();
	args_ptr[args.size()] = 0;

	for(n = 0, it = envs.begin(); it != envs.end(); n++, it++)
		envs_ptr[n] = (char*)it->c_str();
	envs_ptr[envs.size()] = 0;

    pipe(filedesr);
    pipe(filedesw);
    child = fork();
    if (!child) {
		dup2(filedesw[0], 0);
		dup2(filedesr[1], 1);
		dup2(filedesr[1], 2);
		close(filedesw[1]);
		close(filedesr[0]);
		setsid();
		setpgid(0, 0);
		sigemptyset(&newmask);
		sigaddset(&newmask, SIGTERM);
		sigaddset(&newmask, SIGKILL);
		pthread_sigmask(SIG_UNBLOCK, &newmask, 0L);
		usleep(500);
		std::string path = args.size() > 1 && args[1].at(0) == '/' ?
			args[1] : args[0];
		int end_pos = path.find_last_of('/');
		if (end_pos)
			path.erase(end_pos);
		chdir(path.c_str());
		if (execve(args_ptr[0], args_ptr, envs_ptr) < 0) {
			my_perror("execv");
		}
		delete[] envs_ptr;
		delete[] args_ptr;
    } else {
		signal(SIGCHLD, res_popen_sigchild);
		sigemptyset(&newmask);
		sigaddset(&newmask, SIGCHLD);
		sigaddset(&newmask, SIGTERM);
		sigaddset(&newmask, SIGKILL);
		pthread_sigmask(SIG_UNBLOCK, &newmask, 0L);
		close(filedesw[0]);
		close(filedesr[1]);

		flags = fcntl(filedesw[1], F_GETFL, 0);
		flags |= O_NONBLOCK;
#ifndef BSD
		flags |= O_NDELAY;
#endif
		fcntl(filedesw[1], F_SETFL, flags);

		RES_INFO* res_info = new RES_INFO;
		res_info->read = fdopen(filedesr[0], "r");
		res_info->write = fdopen(filedesw[1], "w");
		res_info->process = child;
		return res_info;
    }
	return NULL;
}

static void res_close(RES_INFO* res_info) {
	if (res_info) {
		if (res_info->read) fclose(res_info->read);
		if (res_info->write) fclose(res_info->write);
		delete res_info;
	}
}

#endif

static std::string get_line(int fd) 
{
	char c = 0;
	char add[2] = {0};
	std::string	ret;
	ret.reserve(80);
	while (1) {
		if (recv(fd, &c, 1, 0) <= 0)
			break;
		if (c == '\r')
			continue;
		if (c == '\n')
			break;
		add[0] = c;
		ret += add;
	}
	return ret;
}

void* response_thread(void* param)
{
	server::HttpdInfo *pHttpdInfo = (server::HttpdInfo*)param;
	server *httpd = pHttpdInfo->httpd;
	int msgsock = (int)pHttpdInfo->msgsock;
	std::string address = pHttpdInfo->address;
	std::string str, req, ret;
	std::vector<std::string> vparam;
	std::vector<std::string> vauth;
	std::string refer;
	char length[256];
	std::string res_code;
	std::string res_type;
	std::string res_body;
	std::string res_head;
	std::string http_user_agent;
	std::string http_connection;
	std::string http_cookie;
	std::string http_authorization;
	std::string if_modified_since;
	unsigned long content_length;
	std::string content_type;
	char *post_data = NULL;
	unsigned long post_size = 0;
	RES_INFO* res_info = NULL;
	char buf[BUFSIZ];
	bool keep_alive;
	std::string key;

request_top:
	keep_alive = false;
	res_code = "";
	res_type = "text/html; charset=utf-8";
	res_head = "";
	res_body = "";
	res_info = NULL;
	http_user_agent = "";
	http_connection = "";
	http_cookie = "";
	http_authorization = "";
	content_type = "";
	content_length = 0;
	if_modified_since = "";
	vauth.clear();

	req = get_line(msgsock);
	if (req.size() == 0) {
		goto request_end;
	}
	str = req;
	if (VERBOSE(1)) printf("* %s\n", str.c_str());

	while(str.size()) {
		str = get_line(msgsock);
		if (str == "") break;
		if (VERBOSE(2)) printf("  %s\n", str.c_str());

		key = "connection:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			http_connection = trim_string(str.c_str() + key.size());
			if (!stricmp(http_connection.c_str(), "keep-alive"))
				keep_alive = true;
			continue;
		}
		key = "content-length:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			content_length = atol(str.c_str() + key.size());
			continue;
		}
		key = "user-agent:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			http_user_agent = trim_string(str.c_str() + key.size());
			continue;
		}
		key = "cookie:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			http_cookie = trim_string(str.c_str() + key.size());
			continue;
		}
		key = "if-modified-since:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			if_modified_since = trim_string(str.c_str() + key.size());
			continue;
		}
		key = "content-type:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			content_type = trim_string(str.c_str() + key.size());
			continue;
		}
		key = "Authorization:";
		if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
			http_authorization = trim_string(str.c_str() + key.size());
			continue;
		}
	}

	if (httpd->loggerfunc) {
		httpd->loggerfunc(pHttpdInfo, req);
	}

	vparam = split_string(req, " ");

	try {
		if (httpd->accept_ips.size() > 0 &&
				std::find(httpd->accept_ips.begin(), httpd->accept_ips.end(), address) == httpd->accept_ips.end()) {
			res_code = "HTTP/1.1 403 Forbidden";
			res_body = "Forbidden";
			throw res_code;
		} else
		if (httpd->basic_auths.size() && http_authorization.size() == 0) {
			res_code = "HTTP/1.1 401 Authorization Required";
			res_body = "Authorization Required";
			res_head = "WWW-Authenticate: Basic realm=\"Authorization Required\"\r\n";
			throw res_code;
		} else
		if (vparam.size() < 2 || vparam[1][0] != '/') {
			res_code = "HTTP/1.1 500 Bad Request";
			res_body = "Bad Request\n";
			throw res_code;
		} else {
			std::string auth_basic;
			if (http_authorization.size()) {
				if (!strnicmp(http_authorization.c_str(), "basic ", 6))
					auth_basic = base64_decode(http_authorization.c_str()+6);
				vauth = split_string(auth_basic, ":");
//				if (vauth.size() > 0 && httpd->basic_auths.size() > 0 && httpd->basic_auths[vauth[0]] != vauth[1]) {
//					res_code = "HTTP/1.1 401 Authorization Required";
//					res_body = "Authorization Required";
//					throw res_code;
//				}
			}
			if (vparam[0] == "GET" || vparam[0] == "POST" || vparam[0] == "HEAD") {
				std::string root = server::get_realpath(httpd->root + "/");
				std::string request_uri = vparam[1];
				std::string script_name = vparam[1];
				std::string query_string;
				std::string path_info = "/";
				size_t end_pos = vparam[1].find_first_of('?');
				if (end_pos != std::string::npos) {
					query_string = script_name.substr(end_pos+1);
					script_name = script_name.substr(0, end_pos);
					//request_uri = script_name;
					path_info = script_name;
				}

				server::RequestAliases::iterator it_alias;
				for(it_alias = httpd->request_aliases.begin(); it_alias != httpd->request_aliases.end(); it_alias++) {
					if (path_info == it_alias->first) {
						vparam[1] = it_alias->second;
						break;
					}
				}

				std::string before = root;
				if (before[before.size()-1] == '/')
					before.resize(before.size() - 1);
				before += tthttpd::url_decode(script_name);
				std::string path = server::get_realpath(before);
				if (before != path) {
					path = path.c_str() + root.size() - 1;
					res_code = "HTTP/1.1 301 Document Moved";
					res_body = "Document Moved\n";
					res_head = "Location: ";
					res_head += path;
					res_head += "\n";
					throw res_code;
				}
				if (strncmp(root.c_str(), path.c_str(), root.size())) {
					res_code = "HTTP/1.1 500 Bad Request";
					res_body = "Bad Request\n";
					throw res_code;
				}

				if (vauth.size() > 0) {
					server::AcceptAuths::iterator it_auth;
					for(it_auth = httpd->accept_auths.begin(); it_auth != httpd->accept_auths.end(); it_auth++) {
						if (!strncmp(it_auth->first.c_str(), script_name.c_str(), it_auth->first.size())) {
							if (std::find(it_auth->second.accept_list.begin(), it_auth->second.accept_list.end(), vauth[0]) == it_auth->second.accept_list.end()) {
								res_code = "HTTP/1.1 401 Authorization Required";
								res_head = "WWW-Authenticate: Basic realm=\"Authorization Required\"\r\n";
								res_body = "Authorization Required";
								throw res_code;
							}
						}
					}
				}

				server::DefaultPages::iterator it_page;
				std::string try_path = path;
				if (try_path[try_path.size()-1] != '/')
					try_path += "/";
				for(it_page = httpd->default_pages.begin(); it_page != httpd->default_pages.end(); it_page++) {
					std::string check_path = try_path + *it_page;
					res_info = res_fopen(check_path);
					if (res_info) {
						res_close(res_info);
						path = check_path;
						break;
					}
				}

				server::MimeTypes::iterator it_mime;
				std::string type;
				if (httpd->spawn_executable && res_isexe(path, path_info, script_name)) {
					type = "@";
				} else {
					for(it_mime = httpd->mime_types.begin(); it_mime != httpd->mime_types.end(); it_mime++) {
						std::string match = ".";
						match += it_mime->first;
						if (!strcmp(path.c_str()+path.size()-match.size(), match.c_str())) {
							type = it_mime->second;
							res_type = type;
						}
						if (it_mime->second[0] == '@') {
							match += "/";
							char *ptr = (char*)strstr(path.c_str(), match.c_str());
							if (ptr) {
								type = it_mime->second;
								res_type = type;
								path_info = ptr + match.size() - 1;
								path.resize(path.size() - path_info.size());
								script_name.resize(script_name.size() - path_info.size());
							} else {
								path_info = "";
							}
						}
						if (type.size()) break;
					}
				}

				if (res_isdir(path)) {
					if (vparam[1].size() && vparam[1][vparam[1].size()-1] != '/') {
						res_code = "HTTP/1.1 301 Document Moved";
						res_body = "Document Moved\n";
						res_head = "Location: ";
						res_head += vparam[1];
						res_head += "/\n";
					} else {
					if (VERBOSE(2)) printf("  listing %s\n", path.c_str());
						res_code = "HTTP/1.1 200 OK";
						res_type = "text/html";
						if (!httpd->fs_charset.empty()) {
							res_type += "; charset=";
							res_type += trim_string(httpd->fs_charset);
						}
						res_body = "<html><head><title>";
						res_body += script_name;
						res_body += "</title></head><body><h1>";
						res_body += script_name;
						res_body += "</h1><hr /><pre>";
						res_body += "<table border=0>";
						std::vector<server::ListInfo> flist = res_flist(path);
						std::vector<server::ListInfo>::iterator it;
						for(it = flist.begin(); it != flist.end(); it++) {
							res_body += "<tr><td><a href=\"";
							res_body += tthttpd::url_encode(it->name);
							res_body += "\">";
							res_body += it->name;
							res_body += "</a></td>";
							res_body += "<td>";
							struct tm tm = it->date;
							sprintf(buf, "%02d-%s-%04d %02d:%02d",
								tm.tm_mday,
								months[tm.tm_mon],
								tm.tm_year+1900,
								tm.tm_hour,
								tm.tm_min);
							res_body += buf;
							res_body += "</td>";
							res_body += "<td align=right>&nbsp;&nbsp;";
							if (!it->isdir) {
								if (it->size < 1000)
									sprintf(buf, "%d", (int)it->size);
								else
								if (it->size < 1000000)
									sprintf(buf, "%dK", (int)it->size/1000);
								else
									sprintf(buf, "%.1dM", (int)it->size/1000000);
								res_body += buf;
							} else
								res_body += "[DIR]";
							res_body += "</td></tr>";
						}
						res_body += "</table></pre ><hr /></body></html>";
					}
					throw res_code;
				}

				res_info = res_fopen(path);
				if (!res_info) {
					res_code = "HTTP/1.1 404 Not Found";
					res_body = "Not Found\n";
					throw res_code;
				}

				res_code = "HTTP/1.1 200 OK";
				if (type[0] != '@') {
					std::string file_time = res_ftime(path);
					sprintf(buf, "%d", (int)res_fsize(res_info));
					if (if_modified_since.size() && if_modified_since == file_time) {
						res_close(res_info);
						res_info = NULL;
						res_code = "HTTP/1.1 304 Not Modified";
						res_body = "";
						throw res_code;
					}
					//if (it_mime == httpd->mime_types.end())
					//	res_type = "application/octet-stream; charset=utf-8";
					res_head += "Content-Type: ";
					res_head += res_type;
					res_head += ";\r\n";
					res_head += "Content-Length: ";
					res_head += buf;
					res_head += "\r\n";
					res_head += "Last-Modified: ";
					res_head += file_time;
					res_head += "\r\n";
					res_head += "Date: ";
					res_head += res_curtime();
					res_head += "\r\n";
				} else {
					if (content_length > 0) {
						post_data = new char[content_length+1];
						memset(post_data, 0, content_length+1);
						if (recv(msgsock, post_data, content_length, 0) <= 0) {
							res_code = "HTTP/1.1 500 Bad Request";
							res_body = "Bad Request\n";
							throw res_code;
						}
						post_size = content_length;
						content_length = 0;
					}

					res_close(res_info);
					res_info = NULL;

					std::vector<std::string> envs;
					std::vector<std::string> args;

					if (type.size() == 1) {
						args.push_back(path);
					} else {
						args.push_back(type.substr(1));
						args.push_back(path);
					}
					if (query_string.size())
						args.push_back(query_string);

					std::string env;

					sprintf(buf, "HTTP_HOST=%s:%d", httpd->hostname.c_str(), httpd->port);
					env = buf;
					envs.push_back(env);

					env = "SERVER_NAME=";
					env += httpd->hostname;
					envs.push_back(env);

					sprintf(buf, "SERVER_PORT=%d", httpd->port);
					env = buf;
					envs.push_back(env);

					env = "REMOTE_ADDR=";
					env += httpd->hostaddr;
					envs.push_back(env);

					env = "HTTP_USER_AGENT=";
					env += http_user_agent;
					envs.push_back(env);

					env = "SERVER_PROTOCOL=HTTP/1.1";
					envs.push_back(env);

					env = "HTTP_CONNECTION=";
					env += http_connection;
					envs.push_back(env);

					env = "HTTP_AUTHORIZATION=";
					env += http_authorization;
					envs.push_back(env);

					env = "HTTP_COOKIE=";
					env += http_cookie;
					envs.push_back(env);

					env = "REQUEST_METHOD=";
					env += vparam[0];
					envs.push_back(env);

					env = "REQUEST_URI=";
					env += request_uri;
					envs.push_back(env);

					env = "SCRIPT_FILENAME=";
					env += path;
					envs.push_back(env);

					env = "SCRIPT_NAME=";
					env += script_name;
					envs.push_back(env);

					env = "QUERY_STRING=";
					env += query_string;
					envs.push_back(env);

					env = "PATH_INFO=";
					env += path_info;
					envs.push_back(env);

					env = "REDIRECT_STATUS=1";
					envs.push_back(env);

					env = "PATH=";
					env += getenv("PATH");
					envs.push_back(env);

#ifdef _WIN32
					GetWindowsDirectoryA(buf, sizeof(buf));
					env = "SystemRoot=";
					env += buf;
					envs.push_back(env);
#endif

					env = "SERVER_SOFTWARE=tinytinyhttpd";
					envs.push_back(env);

					env = "SERVER_PROTOCOL=HTTP/1.1";
					envs.push_back(env);

					env = "GATEWAY_INTERFACE=CGI/1.1";
					envs.push_back(env);

					server::RequestEnvironments::iterator it_env;
					for(it_env = httpd->request_environments.begin(); it_env != httpd->request_environments.end(); it_env++) {
						env = it_env->first + "=";
						env += it_env->second;
						envs.push_back(env);
					}

					if (vparam[0] == "POST") {
						env = "CONTENT_TYPE=";
						env += content_type;
						envs.push_back(env);

						sprintf(buf, "%d", (int)post_size);
						env = "CONTENT_LENGTH=";
						env += buf;
						envs.push_back(env);
					}

					if (VERBOSE(4)) {
						std::vector<std::string>::iterator it;
						printf("  --- ARGS ---\n");
						for(it = args.begin(); it != args.end(); it++)
							printf("  %s\n", it->c_str());
						printf("  --- ENVS ---\n");
						for(it = envs.begin(); it != envs.end(); it++)
							printf("  %s\n", it->c_str());
						printf("  ------------\n");
					}

					res_info = res_popen(args, envs);

					if (post_data) {
						res_write(res_info, post_data, post_size);
#ifndef _WIN32
						fflush((FILE*)res_info->write);
#endif
						delete[] post_data;
						post_data = NULL;
						post_size = 0;
					}
				}
			} else {
				res_code = "HTTP/1.1 500 Bad Request";
				res_body = "Bad Request\n";
				throw res_code;
			}
		}
	}
	catch(...) {
		if (res_code.size() == 0) {
			if (VERBOSE(1)) my_perror(" cached exception");
			res_code = "HTTP/1.1 500 Bad Request";
			res_body = "Internal Server Error\n";
		}
	}

	if (post_data) {
		delete[] post_data;
		post_data = NULL;
		post_size = 0;
	}

	if (content_length > 0) {
		while(content_length > 0) {
			int ret = recv(msgsock, buf, sizeof(buf), 0);
			if (ret < 0) {
				res_code = "HTTP/1.1 500 Bad Request";
				res_body = "Bad Request\n";
			}
			content_length -= ret;
		}
	}

	if (res_info && res_info->process) {
		bool res_keep_alive = false;
		res_head = "";

		str = req;
		while(str.size()) {
			memset(buf, 0, sizeof(buf));
			str = res_fgets(res_info);
			if (str.size() == 0) break;
			if (str[0] == '<') {
				// workaround for broken non-header response.
				send(msgsock, str.c_str(), str.size(), 0);
				res_code = "";
				break;
			}
			if (VERBOSE(2)) printf("  %s\n", str.c_str());
			key = "connection:";
			if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
				http_connection = trim_string(str.c_str() + key.size());
				if (!stricmp(http_connection.c_str(), "keep-alive"))
					res_keep_alive = true;
			}
			key = "WWW-Authenticate: Basic ";
			if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
				res_code = "HTTP/1.1 401 Unauthorized";
			}
			key = "Status:";
			if (!strnicmp(str.c_str(), key.c_str(), key.size())) {
				res_code = "HTTP/1.1" + str.substr(key.size());
			}
			res_head += str;
			res_head += "\r\n";
		}
		if (!res_keep_alive)
			keep_alive = false;
	}

	if (res_code.size()) {
		send(msgsock, res_code.c_str(), (int)res_code.size(), 0);
		send(msgsock, "\r\n", 2, 0);
	}

	if (res_head.size()) {
		send(msgsock, res_head.c_str(), (int)res_head.size(), 0);
	}

	if (res_info) {
		send(msgsock, "\r\n", 2, 0);
		while(true) {
			memset(buf, 0, sizeof(buf));
			unsigned long read = res_read(res_info, buf, sizeof(buf));
			if (read == 0) break;
			send(msgsock, buf, read, 0);
		}
		res_close(res_info);
		res_info = NULL;
	} else
	if (res_body.size()) {
		if (keep_alive)
			ret = "Connection: keep-alive\r\n";
		else
			ret = "Connection: close\r\n";
		send(msgsock, ret.c_str(), (int)ret.size(), 0);

		ret = "Content-Type: ";
		ret += res_type + "\r\n";
		send(msgsock, ret.c_str(), (int)ret.size(), 0);

		ret = res_body;
		sprintf(length, "%u", ret.size());
		ret = "Content-Length: ";
		ret += length;
		ret += "\r\n";
		send(msgsock, ret.c_str(), (int)ret.size(), 0);

		send(msgsock, "\r\n", 2, 0);

		if (vparam.size() > 0 && vparam[0] != "HEAD") {
			ret = res_body;
			send(msgsock, ret.c_str(), (int)ret.size(), 0);
		}
	}
	else
		send(msgsock, "\r\n", (int)2, 0);

#if 0
	fd_set fdset;
	struct timeval timeout;
	FD_ZERO(&fdset);
	FD_SET(msgsock , &fdset);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	while(select(msgsock+1, NULL, &fdset, NULL, &timeout) != 1) {
#ifdef _WIN32
		Sleep(100);
#else
		usleep(100000);
#endif
	}
#endif

	if (keep_alive)
		goto request_top;

request_end:
	shutdown(msgsock, SD_BOTH);
	closesocket(msgsock);

	return NULL;
}

void* watch_thread(void* param)
{

	server *httpd = (server*)param;
	int msgsock;
	struct sockaddr_in server;

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	httpd->sock = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd->sock < 0) { 
		my_perror("socket");
		return NULL;
	}
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	setsockopt(httpd->sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, optlen);

	server.sin_family = PF_INET;
	server.sin_addr.s_addr = INADDR_ANY; 
	server.sin_port = htons(httpd->port);

	if (bind(httpd->sock, (struct sockaddr *)&server, sizeof server) < 0) {
		my_perror("bind");
		return NULL;
	}

	if (listen(httpd->sock, SOMAXCONN) < 0) {
		my_perror("listen");
		return NULL;
	}

	char hostbuf[256] = {0};
	gethostname(hostbuf, sizeof(hostbuf));
	httpd->hostname = hostbuf;
	struct hostent* hostent = gethostbyname(hostbuf);
	if (hostent) {
		sprintf(hostbuf, "%d.%d.%d.%d",
			(unsigned char)*(hostent->h_addr_list[0]),
			(unsigned char)*(hostent->h_addr_list[0]+1),
			(unsigned char)*(hostent->h_addr_list[0]+2),
			(unsigned char)*(hostent->h_addr_list[0]+3));
		httpd->hostaddr = hostbuf;
	}

	if (VERBOSE(1)) printf("server started http://%s:%d/\n", hostbuf, httpd->port);
	for(;;) {
		struct sockaddr_in client;
		int client_len = sizeof(client);
		memset(&client, 0, sizeof(client));
		msgsock = accept(httpd->sock, (struct sockaddr *)&client, (socklen_t *)&client_len);
		if (msgsock == -1) {
			closesocket(httpd->sock);
			httpd->sock = -1;
			/*
			break;
			*/
			continue;
		}
		else {
			std::string address = inet_ntoa(client.sin_addr);

			server::HttpdInfo *pHttpdInfo = new server::HttpdInfo;
			pHttpdInfo->msgsock = msgsock;
			pHttpdInfo->httpd = httpd;
			pHttpdInfo->address = address;

#ifdef _WIN32
			_beginthread((void (*)(void*))response_thread, 0, (void*)pHttpdInfo);
#else
			pthread_t pth;
			pthread_create(&pth, NULL, response_thread, (void*)pHttpdInfo);
#endif
		}
	}
	return NULL;
}

bool server::start() {
#ifndef _WIN32
	set_priv(user.c_str(), chroot.c_str(), "tthttpd");
#endif
	if (thread)
		return false;
#ifdef _WIN32
	thread = (HANDLE)_beginthread((void (*)(void*))watch_thread, 0, (void*)this);
#else
	pthread_create(&thread, NULL, watch_thread, (void*)this);
#endif
	return thread ? true : false;
}

bool server::stop() {
	closesocket(sock);
	wait();
	return true;
}

bool server::wait() {
#ifdef _WIN32
	WaitForSingleObject(thread, INFINITE);
#else
	pthread_join(thread, NULL);
#endif
	return true;
}

}
