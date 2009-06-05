#ifndef _XMLRPCVALUE_H_
#define _XMLRPCVALUE_H_

#define XMLRPCVALUE_VERSION 0x0100

#ifdef _MSC_VER
#include <tchar.h>
#pragma warning(disable : 4530 4018 4786)
#pragma comment(lib, "wininet.lib")
#endif

#include <vector>
#include <map>
#include <string>
#include <algorithm>

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

namespace XmlRpc {

class XmlRpcException {
public:
	tstring message;
	int code;
	XmlRpcException(int _code, tstring _message) {
		code = _code;
		message = _message;
	}
};

class XmlRpcValue {
public:
	typedef std::vector<char> ValueBinary;
	typedef std::vector<XmlRpcValue> ValueList;
	typedef std::vector<XmlRpcValue> ValueArray;
	typedef std::map<tstring, XmlRpcValue> ValueStruct;
	enum Type {
	  TypeInvalid, TypeBoolean, TypeInt, TypeDouble, TypeTime,
	  TypeString, TypeBinary, TypeList, TypeArray, TypeStruct
	};
protected:
	tstring tag;
	typedef union {
		bool			asBool;
		int				asInt;
		double			asDouble;
		struct tm*		asTime;
		int				nchar;
		tstring*		asString;
		ValueBinary*	asBinary;
		ValueArray*		asArray;
		ValueStruct*	asStruct;
		void*			asUnknown;
	} Value;
public:
	Type type;
	Value value;
	tchar* operator=(tchar* x) {
		invalidate();
		value.asString = new tstring(x);
		type = TypeString;
		return (tchar*)value.asString;
	}
	const tchar* operator=(const tchar* x) {
		invalidate();
		value.asString = new tstring(x);
		type = TypeString;
		return (tchar*)value.asString;
	}
	tstring operator=(tstring x) {
		invalidate();
		value.asString = new tstring(x);
		type = TypeString;
		return (tchar*)value.asString;
	}
	bool operator=(bool x) {
		invalidate();
		value.asBool = x;
		type = TypeBoolean;
		return value.asBool;
	}
	int operator=(int x) {
		invalidate();
		value.asInt = x;
		type = TypeInt;
		return value.asInt;
	}
	double operator=(double x) {
		invalidate();
		value.asDouble = x;
		type = TypeDouble;
		return value.asDouble;
	}
	XmlRpcValue() {
		type = TypeInvalid;
		value.asUnknown = 0;
	};
	~XmlRpcValue() {
		invalidate();
	}
	XmlRpcValue(const tchar* _string) {
		value.asString = new tstring();
		*value.asString = _string;
		type = TypeString;
	}
	XmlRpcValue(tstring _string) {
		value.asString = new tstring();
		*value.asString = _string;
		type = TypeString;
	}
	XmlRpcValue(bool _bool) {
		value.asBool = _bool;
		type = TypeBoolean;
	}
	XmlRpcValue(int _int) {
		value.asInt = _int;
		type = TypeInt;
	}
	XmlRpcValue(long _long) {
		value.asInt = _long;
		type = TypeInt;
	}
	XmlRpcValue(double _double) {
		value.asDouble = _double;
		type = TypeDouble;
	}
	XmlRpcValue(struct tm _tm) {
		value.asTime = new struct tm(_tm);
		type = TypeTime;
	}
	XmlRpcValue(ValueBinary& _binary) {
		value.asBinary = new ValueBinary(_binary);
		type = TypeBinary;
	}
	XmlRpcValue(const char* _buffer, size_t _size) {
		value.asBinary = new ValueBinary();
		for(int n = 0; n < _size; n++)
			value.asBinary->push_back(_buffer[n]);
		type = TypeBinary;
	}
    Type const &getType() const {
		return type;
	}
	bool getBoolean() const {
		return value.asBool;
	}
	int getInt() const {
		return value.asInt;
	}
	double getDouble() const {
		return value.asDouble;
	}
	struct tm* getTime() const {
		return value.asTime;
	}
	tstring getString() const {
		return *(value.asString);
	}
	ValueBinary getBinary() const {
		return *(value.asBinary);
	}
	bool hasMember(const tstring& name) const {
		return type == TypeStruct && value.asStruct->find(name) != value.asStruct->end();
	}
	std::vector<tstring> listMembers() const {
		std::vector<tstring> ret;
		ValueStruct::const_iterator it;
		if (type == TypeStruct) {
			for(it = value.asStruct->begin(); it != value.asStruct->end(); it++)
				ret.push_back(it->first);
		}
		return ret;
	}
	size_t size() const {
		switch(type) {
		case TypeString:
			return int(value.asString->size());
		case TypeBinary:
			return int(value.asBinary->size());
		case TypeArray: 
			return int(value.asArray->size());
		case TypeStruct:
			return int(value.asStruct->size());
		default:
			break;
		}
		return 0;
	}
	tstring valueString() const {
		tstring ret;
		static tchar buf[256];
		XmlRpcValue::ValueArray::const_iterator itarray;
		XmlRpcValue::ValueStruct::const_iterator itstruct;
		switch(type) {
		case TypeString:
			ret = *value.asString;
			break;
		case TypeInt:
			_stprintf(buf, _T("%d"),
				value.asInt);
			ret = buf;
			break;
		case TypeTime:
			_stprintf(buf, _T("%04d/%02d/%02d %02d:%02d:%02d"),
				value.asTime->tm_year+1900,
				value.asTime->tm_mon,
				value.asTime->tm_mday,
				value.asTime->tm_hour,
				value.asTime->tm_min,
				value.asTime->tm_sec);
			ret = buf;
			break;
		case TypeBoolean:
			ret = value.asBool ? _T("true") : _T("false");
			break;
		case TypeArray:
			ret += _T("[");
			for(itarray = value.asArray->begin(); itarray != value.asArray->end(); itarray++) {
				if (itarray != value.asArray->begin())
					ret += _T(", ");
				ret += itarray->valueString();
			}
			ret += _T("]");
			break;
		case TypeStruct:
			ret += _T("[");
			for(itstruct = value.asStruct->begin(); itstruct != value.asStruct->end(); itstruct++) {
				if (itstruct != value.asStruct->begin())
					ret += _T(", ");
				ret += itstruct->first.c_str();
				ret += _T("=\"");
				ret += itstruct->second.valueString();
				ret += _T("\"");
			}
			ret += _T("]");
			break;
		}
		return ret;
	}

	operator int&() { return value.asInt; }
	operator struct tm*() { return value.asTime; }
	operator const tchar*() { return value.asString->c_str(); }
	operator tstring&() { return *value.asString; }
	operator ValueBinary&() { return *value.asBinary; }
	operator ValueArray&() { return *value.asArray; }
	operator ValueStruct&() { return *value.asStruct; }

	XmlRpcValue(XmlRpcValue const& rhs) {
		type = TypeInvalid;
		*this = rhs;
	}
	XmlRpcValue& operator=(XmlRpcValue const& rhs) {
		if (this != &rhs) {
			invalidate();
			type = rhs.type;
			switch (type) {
			case TypeBoolean:	value.asBool = rhs.value.asBool; break;
			case TypeInt:		value.asInt = rhs.value.asInt; break;
			case TypeDouble:	value.asDouble = rhs.value.asDouble; break;
			case TypeTime:		value.asTime = new struct tm(*rhs.value.asTime); break;
			case TypeString:	value.asString = new tstring(*rhs.value.asString); break;
			case TypeBinary:	value.asBinary = new ValueBinary(*rhs.value.asBinary); break;
			case TypeArray:		value.asArray = new ValueArray(*rhs.value.asArray); break;
			case TypeStruct:	value.asStruct = new ValueStruct(*rhs.value.asStruct); break;
			default:			value.asBinary = 0; break;
			}
		}
		return *this;
	}
	XmlRpcValue(ValueArray& _array) {
		//invalidate();
		value.asArray = new ValueArray(_array);
		type = TypeArray;
	}
	XmlRpcValue(ValueStruct& _struct) {
		//invalidate();
		value.asStruct = new ValueStruct(_struct);
		type = TypeStruct;
	}

    XmlRpcValue const& operator[](int i) const { assertArray(i+1); return value.asArray->at(i); }
    XmlRpcValue& operator[](int i)             { assertArray(i+1); return value.asArray->at(i); }

    XmlRpcValue& operator[](tstring const& k) { assertStruct(); return (*value.asStruct)[k]; }
    XmlRpcValue& operator[](const tchar* k) { assertStruct(); tstring s(k); return (*value.asStruct)[s]; }

	static bool tmEq(struct tm* const& t1, struct tm* const& t2) {
	return
		t1->tm_sec == t2->tm_sec && t1->tm_min == t2->tm_min &&
		t1->tm_hour == t2->tm_hour && t1->tm_mday == t1->tm_mday &&
		t1->tm_mon == t2->tm_mon && t1->tm_year == t2->tm_year;
	}

	bool operator==(XmlRpcValue const& other) const
	{
		if (type != other.type)
			return false;

		switch (type) {
		case TypeBoolean:  return ( !value.asBool && !other.value.asBool) ||
					( value.asBool && other.value.asBool);
		case TypeInt:      return value.asInt == other.value.asInt;
		case TypeDouble:   return value.asDouble == other.value.asDouble;
		case TypeTime:     return tmEq(value.asTime, other.value.asTime);
		case TypeString:   return *value.asString == *other.value.asString;
		case TypeBinary:   return *value.asBinary == *other.value.asBinary;
		case TypeArray:    return *value.asArray == *other.value.asArray;

		case TypeStruct:
			{
				if (value.asStruct->size() != other.value.asStruct->size())
					return false;

				ValueStruct::const_iterator it1=value.asStruct->begin();
				ValueStruct::const_iterator it2=other.value.asStruct->begin();
				while (it1 != value.asStruct->end()) {
					const XmlRpcValue& v1 = it1->second;
					const XmlRpcValue& v2 = it2->second;
					if ( ! (v1 == v2))
						return false;
					it1++;
					it2++;
				}
				return true;
			}
		default: break;
		}
		return true;
	}

	bool operator!=(XmlRpcValue const& other) const
	{
		return !(*this == other);
	}

    void clear() {
		invalidate();
	}

protected:
	void invalidate() {
		if (type == TypeTime)
			delete value.asTime;
		if (type == TypeString)
			delete value.asString;
		if (type == TypeBinary)
			delete value.asBinary;
		if (type == TypeArray)
			delete value.asArray;
		if (type == TypeStruct)
			delete value.asStruct;
		type = TypeInvalid;
	}
    void assertArray(int size) const {
		if (type != TypeArray)
		  throw XmlRpcException(4, _T("type error: expected an array"));
		else if (int(value.asArray->size()) < size)
		  throw XmlRpcException(4, _T("range error: array index too large"));
	}
	void assertArray(int size)
	{
		if (type == TypeInvalid) {
			type = TypeArray;
			value.asArray = new ValueArray(size);
		} else if (type == TypeArray) {
			if (int(value.asArray->size()) < size)
			value.asArray->resize(size);
		} else
			throw XmlRpcException(4, _T("type error: expected an array"));
	}
	void assertStruct()
	{
		if (type == TypeInvalid) {
			type = TypeStruct;
			value.asStruct = new ValueStruct();
		}
	}
};

}

#endif /* _XMLRPCVALUE_H_ */
