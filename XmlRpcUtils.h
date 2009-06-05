#ifndef _XMLRPCUTIL_H_
#define _XMLRPCUTIL_H_

#define XMLRPCUTILS_VERSION 0x0100

#include <iostream>
#include "XmlRpcValue.h"

namespace XmlRpc {

#ifdef USE_MLANG
#define l2u(x) XmlRpc::convert_string(x, _T("char"), _T("utf-8"))
#define u2l(x) XmlRpc::convert_string(x, _T("utf-8"), _T("char"))
#else
#define l2u(x) XmlRpc::string_to_utf8(x)
#define u2l(x) XmlRpc::utf8_to_string(x)
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
std::string trim_string(const std::string strSrc);
std::string& replace_string(std::string& strSrc, const std::string strFrom, const std::string strTo);
#ifdef UNICODE
std::vector<tstring> split_string(tstring strSrc, tstring strKey);
tstring trim_string(tstring strSrc);
tstring& replace_string(tstring& strSrc, tstring strFrom, tstring strTo);
#endif

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& encoded_string);
std::vector<char> base64_decode_binary(std::string const& encoded_string);
tstream& operator<<(tstream& os, XmlRpcValue& v);
bool isFaultResult(tstring& strXml);
bool isFaultResult(XmlRpcValue& res);
tstring getMethodFromXml(std::string& strXml);
XmlRpcValue getXmlRpcValueFromXml(std::string& strXml);
std::string getXmlFromRequests(tstring method, std::vector<XmlRpcValue>& requests, tstring encoding = _T("utf-8"));
std::string getXmlFromResponse(XmlRpcValue& response, tstring encoding = _T("utf-8"));
std::string getXmlFromXmlRpcValue(XmlRpcValue& response, tstring encoding = _T("utf-8"));
std::string getXmlFromException(XmlRpcException e);
std::string queryXml(std::string& strXml, std::string query, std::string defaultNs = "");
int postXmlRpc(tstring url, tstring userid, tstring passwd, tstring method, std::string requestXml, std::string& responseXml, tstring user_agent);
tstring getFaultStringFromXml(std::string& strXml);
tstring getFaultResult(XmlRpcValue& res);
int execXmlRpc(tstring url, tstring method, std::vector<XmlRpcValue>& requests, XmlRpcValue& response, tstring userid = _T(""), tstring passwd = _T(""), tstring encoding = _T("utf-8"), tstring user_agent = _T("XmlRpcClient"));
XmlRpcValue::ValueBinary loadBinaryFromFile(tstring filename);
bool saveBinaryToFile(tstring filename, XmlRpcValue::ValueBinary valuebinary);
std::string urldecode(const std::string& url);
std::string urlencode(const std::string& url);
std::string htmldecode(const std::string& html);
std::string htmlencode(const std::string& html);
}

#endif /* _XMLRPCUTIL_H_ */
