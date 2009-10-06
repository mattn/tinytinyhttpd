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
#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef _MSC_VER
#include <tchar.h>
#pragma warning(disable : 4530 4018 4786)
#pragma comment(lib, "wininet.lib")
#endif

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace tthttpd {

#ifndef _TSTRDEF
#ifndef _MSC_VER
#define _T(x) (x)
#define _stprintf sprintf
#define _sntprintf snprintf
#define _tprintf printf
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcschr strchr
#define _tcsstr strstr
#define _ttol atol
#define _tstat stat
#define _tfopen fopen
#define _stscanf sscanf
#define _tcsftime strftime
#define stricmp strcasecmp
#define _ftprintf fprintf
#ifndef SD_BOTH
# define SD_BOTH SHUT_RDWR
#endif
#endif

#ifdef _UNICODE
typedef std::wstring tstring;
typedef std::wostream tstream;
typedef wchar_t tchar;
#define tcout std::wcout
#define tcerr std::wcerr
#else
typedef std::string tstring;
typedef std::ostream tstream;
typedef char tchar;
#define tcout std::cout
#define tcerr std::cerr
#endif
#define _TSTRDEF
#endif

#ifdef USE_MLANG
#define l2u(x) tthttpd::convert_string(x, _T("char"), _T("utf-8"))
#define u2l(x) tthttpd::convert_string(x, _T("utf-8"), _T("char"))
#else
#define l2u(x) tthttpd::string_to_utf8(x)
#define u2l(x) tthttpd::utf8_to_string(x)
#endif

#ifdef _WIN32
# define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

tstring getTimeString(struct tm* _cur = NULL);
std::string tstring2string(tstring strSrc);
tstring string2tstring(std::string strSrc);
tstring string2tstring(std::string strSrc);
std::string string_to_utf8(std::string str);
std::string utf8_to_string(std::string str);
#ifdef _UNICODE
tstring string_to_utf8(tstring str);
tstring utf8_to_string(tstring str);
#endif
int utf_char2bytes(int c, unsigned char* buf);
int utf_bytes2char(unsigned char* p);

tstring convert_string(const tstring str, const tstring from_codeset, const tstring to_codeset);
std::string cut_string(std::string str, int cells, std::string padding = "...");
std::string cut_string_r(std::string str, int cells, std::string padding = "...");

std::vector<std::string> split_string(const std::string strSrc, const std::string strKey);
void split_string(const std::string strSrc, const std::string strKey, std::vector<std::string>& res);
std::string trim_string(const std::string strSrc);
std::string& replace_string(std::string& strSrc, const std::string strFrom, const std::string strTo);
#ifdef UNICODE
std::vector<tstring> split_string(tstring strSrc, tstring strKey);
tstring trim_string(tstring strSrc);
tstring& replace_string(tstring& strSrc, tstring strFrom, tstring strTo);
#endif

std::string md5_string(const std::string& input);
std::string string_to_hex(const std::string& input);
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& encoded_string);
std::vector<char> base64_decode_binary(std::string const& encoded_string);
std::string url_decode(const std::string& url);
std::string url_encode(const std::string& url);
std::string html_decode(const std::string& html);
std::string html_encode(const std::string& html);
std::map<std::string, std::string> parse_querystring(const std::string& query_string);

void set_priv(const char *, const char *, const char *);

}

#endif /* _UTILS_H_ */
