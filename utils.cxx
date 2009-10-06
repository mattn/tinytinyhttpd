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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _CRT_SECURE_NO_DEPRECATE
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

#include "utils.h"
#include <sstream>

namespace tthttpd {

const std::string base64_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";
#define is_base64(c) ( \
		isalnum((unsigned char)c) || \
		((unsigned char)c == '+') || \
		((unsigned char)c == '/'))

struct interval {
  unsigned short first;
  unsigned short last;
};

static char utf8len_tab[256] =
{
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};


int utf_bytes2char(unsigned char* p)
{
	int		len;

	if (p[0] < 0x80)	/* be quick for ASCII */
		return p[0];

	len = utf8len_tab[p[0]];
	if ((p[1] & 0xc0) == 0x80) {
		if (len == 2)
			return ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
		if ((p[2] & 0xc0) == 0x80) {
			if (len == 3)
				return ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
					+ (p[2] & 0x3f);
			if ((p[3] & 0xc0) == 0x80) {
				if (len == 4)
					return ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
						+ ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
				if ((p[4] & 0xc0) == 0x80) {
					if (len == 5)
						return ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18)
							+ ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
							+ (p[4] & 0x3f);
					if ((p[5] & 0xc0) == 0x80 && len == 6)
						return ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24)
							+ ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
							+ ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
				}
			}
		}
	}
	/* Illegal value, just return the first byte */
	return p[0];
}

int utf_char2bytes(int c, unsigned char* buf)
{
	if (c < 0x80) {			/* 7 bits */
		buf[0] = c;
		return 1;
	}
	if (c < 0x800) {		/* 11 bits */
		buf[0] = 0xc0 + ((unsigned)c >> 6);
		buf[1] = 0x80 + (c & 0x3f);
		return 2;
	}
	if (c < 0x10000) {		/* 16 bits */
		buf[0] = 0xe0 + ((unsigned)c >> 12);
		buf[1] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[2] = 0x80 + (c & 0x3f);
		return 3;
	}
	if (c < 0x200000) {		/* 21 bits */
		buf[0] = 0xf0 + ((unsigned)c >> 18);
		buf[1] = 0x80 + (((unsigned)c >> 12) & 0x3f);
		buf[2] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[3] = 0x80 + (c & 0x3f);
		return 4;
	}
	if (c < 0x4000000) {	/* 26 bits */
		buf[0] = 0xf8 + ((unsigned)c >> 24);
		buf[1] = 0x80 + (((unsigned)c >> 18) & 0x3f);
		buf[2] = 0x80 + (((unsigned)c >> 12) & 0x3f);
		buf[3] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[4] = 0x80 + (c & 0x3f);
		return 5;
	}

	/* 31 bits */
	buf[0] = 0xfc + ((unsigned)c >> 30);
	buf[1] = 0x80 + (((unsigned)c >> 24) & 0x3f);
	buf[2] = 0x80 + (((unsigned)c >> 18) & 0x3f);
	buf[3] = 0x80 + (((unsigned)c >> 12) & 0x3f);
	buf[4] = 0x80 + (((unsigned)c >> 6) & 0x3f);
	buf[5] = 0x80 + (c & 0x3f);
	return 6;
}

static int bisearch(wchar_t ucs, const struct interval *table, int max) {
  int min = 0;
  int mid;

  if (ucs < table[0].first || ucs > table[max].last)
    return 0;
  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > table[mid].last)
      min = mid + 1;
    else if (ucs < table[mid].first)
      max = mid - 1;
    else
      return 1;
  }

  return 0;
}

int wcwidth(wchar_t ucs)
{
  /* sorted list of non-overlapping intervals of non-spacing characters */
  static const struct interval combining[] = {
    { 0x0300, 0x034E }, { 0x0360, 0x0362 }, { 0x0483, 0x0486 },
    { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
    { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
    { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
    { 0x07A6, 0x07B0 }, { 0x0901, 0x0902 }, { 0x093C, 0x093C },
    { 0x0941, 0x0948 }, { 0x094D, 0x094D }, { 0x0951, 0x0954 },
    { 0x0962, 0x0963 }, { 0x0981, 0x0981 }, { 0x09BC, 0x09BC },
    { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 },
    { 0x0A02, 0x0A02 }, { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 },
    { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 },
    { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 },
    { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 },
    { 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 },
    { 0x0B4D, 0x0B4D }, { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 },
    { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 },
    { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
    { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
    { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA },
    { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 },
    { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 },
    { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD },
    { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 },
    { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 },
    { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC },
    { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 }, { 0x1032, 0x1032 },
    { 0x1036, 0x1037 }, { 0x1039, 0x1039 }, { 0x1058, 0x1059 },
    { 0x1160, 0x11FF }, { 0x17B7, 0x17BD }, { 0x17C6, 0x17C6 },
    { 0x17C9, 0x17D3 }, { 0x180B, 0x180E }, { 0x18A9, 0x18A9 },
    { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x206A, 0x206F },
    { 0x20D0, 0x20E3 }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
    { 0xFB1E, 0xFB1E }, { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF },
    { 0xFFF9, 0xFFFB }
  };

  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, combining,
	       sizeof(combining) / sizeof(struct interval) - 1))
    return 0;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  return 1 +
    (ucs >= 0x1100 &&
     (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
      (ucs >= 0x2e80 && ucs <= 0xa4cf && (ucs & ~0x0011) != 0x300a &&
       ucs != 0x303f) ||                  /* CJK ... Yi */
      (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
      (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff5f) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
#ifndef _WIN32
      (ucs >= 0x20000 && ucs <= 0x2ffff)
#else
	  0
#endif
	  ));
}

int wcswidth(const wchar_t *pwcs, size_t n)
{
  int w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = wcwidth(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}

static int wcwidth_cjk(wchar_t ucs)
{
  /* sorted list of non-overlapping intervals of East Asian Ambiguous
   * characters */
  static const struct interval ambiguous[] = {
    { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
    { 0x00AA, 0x00AA }, { 0x00AD, 0x00AE }, { 0x00B0, 0x00B4 },
    { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
    { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
    { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
    { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
    { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
    { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
    { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
    { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
    { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
    { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
    { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
    { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
    { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
    { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
    { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
    { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0300, 0x034E },
    { 0x0360, 0x0362 }, { 0x0391, 0x03A1 }, { 0x03A3, 0x03A9 },
    { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 }, { 0x0401, 0x0401 },
    { 0x0410, 0x044F }, { 0x0451, 0x0451 }, { 0x2010, 0x2010 },
    { 0x2013, 0x2016 }, { 0x2018, 0x2019 }, { 0x201C, 0x201D },
    { 0x2020, 0x2022 }, { 0x2024, 0x2027 }, { 0x2030, 0x2030 },
    { 0x2032, 0x2033 }, { 0x2035, 0x2035 }, { 0x203B, 0x203B },
    { 0x203E, 0x203E }, { 0x2074, 0x2074 }, { 0x207F, 0x207F },
    { 0x2081, 0x2084 }, { 0x20AC, 0x20AC }, { 0x2103, 0x2103 },
    { 0x2105, 0x2105 }, { 0x2109, 0x2109 }, { 0x2113, 0x2113 },
    { 0x2116, 0x2116 }, { 0x2121, 0x2122 }, { 0x2126, 0x2126 },
    { 0x212B, 0x212B }, { 0x2153, 0x2155 }, { 0x215B, 0x215E },
    { 0x2160, 0x216B }, { 0x2170, 0x2179 }, { 0x2190, 0x2199 },
    { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 }, { 0x21D4, 0x21D4 },
    { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 }, { 0x2202, 0x2203 },
    { 0x2207, 0x2208 }, { 0x220B, 0x220B }, { 0x220F, 0x220F },
    { 0x2211, 0x2211 }, { 0x2215, 0x2215 }, { 0x221A, 0x221A },
    { 0x221D, 0x2220 }, { 0x2223, 0x2223 }, { 0x2225, 0x2225 },
    { 0x2227, 0x222C }, { 0x222E, 0x222E }, { 0x2234, 0x2237 },
    { 0x223C, 0x223D }, { 0x2248, 0x2248 }, { 0x224C, 0x224C },
    { 0x2252, 0x2252 }, { 0x2260, 0x2261 }, { 0x2264, 0x2267 },
    { 0x226A, 0x226B }, { 0x226E, 0x226F }, { 0x2282, 0x2283 },
    { 0x2286, 0x2287 }, { 0x2295, 0x2295 }, { 0x2299, 0x2299 },
    { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF }, { 0x2312, 0x2312 },
    { 0x2329, 0x232A }, { 0x2460, 0x24BF }, { 0x24D0, 0x24E9 },
    { 0x2500, 0x254B }, { 0x2550, 0x2574 }, { 0x2580, 0x258F },
    { 0x2592, 0x2595 }, { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 },
    { 0x25B2, 0x25B3 }, { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD },
    { 0x25C0, 0x25C1 }, { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB },
    { 0x25CE, 0x25D1 }, { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF },
    { 0x2605, 0x2606 }, { 0x2609, 0x2609 }, { 0x260E, 0x260F },
    { 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
    { 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
    { 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
    { 0x273D, 0x273D }, { 0x3008, 0x300B }, { 0x3014, 0x3015 },
    { 0x3018, 0x301B }, { 0xFFFD, 0xFFFD }
  };

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, ambiguous,
	       sizeof(ambiguous) / sizeof(struct interval) - 1))
    return 2;

  return wcwidth(ucs);
}


int wcswidth_cjk(const wchar_t *pwcs, size_t n)
{
  int w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = wcwidth_cjk(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}

tstring getTimeString(struct tm* _cur) {
	struct tm* cur;
	if (_cur)
		cur = _cur;
	else {
		time_t tim;
		time(&tim);
		cur = localtime(&tim);
	}

	tchar buf[256];
	_stprintf(buf, _T("%04d/%02d/%02d %02d:%02d:%02d\n"),
		cur->tm_year+1900,
		cur->tm_mon,
		cur->tm_mday,
		cur->tm_hour,
		cur->tm_min,
		cur->tm_sec);
	return buf;
}

std::string tstring2string(tstring strSrc) {
	std::string strRet;
#ifdef _UNICODE
	UINT codePage = GetACP();
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, NULL, 0, NULL, NULL);
	char* pszStr = new char[mbssize+1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, pszStr, mbssize, NULL, NULL);
	pszStr[mbssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	strRet = strSrc;
#endif
	return strRet;
}

tstring string2tstring(std::string strSrc) {
	tstring strRet;
#ifdef _UNICODE
	size_t in_len = strlen(strSrc.c_str());
	UINT codePage = GetACP();
	size_t wcssize = MultiByteToWideChar(codePage, 0, strSrc.c_str(),in_len,  NULL, 0);
	wchar_t* pszStr = new wchar_t[wcssize + 1];
	MultiByteToWideChar(codePage, 0, strSrc.c_str(), in_len, pszStr, wcssize + 1);
	pszStr[wcssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	strRet = strSrc;
#endif
	return strRet;
}

std::string string_to_utf8(std::string str) {
	std::string strRet;
	std::wstring strSrc;

#ifdef _WIN32
	UINT codePage;
	size_t in_len = str.size();
	codePage = GetACP();
	size_t wcssize = MultiByteToWideChar(codePage, 0, str.c_str(),in_len,  NULL, 0);
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	wcssize = MultiByteToWideChar(codePage, 0, str.c_str(), in_len, pszStrWC, wcssize + 1);
	pszStrWC[wcssize] = '\0';
	strSrc = pszStrWC;
	delete [] pszStrWC;

	codePage = CP_UTF8;
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, NULL, 0, NULL, NULL);
	char* pszStr = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, pszStr, mbssize, NULL, NULL);
	pszStr[mbssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	size_t n = 0, clen = 0, len = 0;
	int tmp = mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
	wcssize = n;
	for(n = 0; n < wcssize; n++) {
		unsigned char bytes[8] = {0};
		utf_char2bytes(pszStrWC[n], bytes);
		strRet += (char*)bytes;
	}
	delete[] pszStrWC;
#endif

	return strRet;
}

#ifdef _UNICODE
tstring string_to_utf8(tstring str) {
	return str;
}
#endif

std::string utf8_to_string(std::string str) {
	std::string strRet;

#ifdef _WIN32
	UINT codePage = CP_UTF8;
	const char* ptr = str.c_str();
	if (str[0] == (char)0xef && str[1] == (char)0xbb && str[2] == (char)0xbf)
		ptr += 3;
	size_t wcssize = MultiByteToWideChar(codePage, 0, ptr, -1,  NULL, 0);
	wchar_t* pszStr = new wchar_t[wcssize + 1];
	wcssize = MultiByteToWideChar(codePage, 0, ptr, -1, pszStr, wcssize + 1);
	pszStr[wcssize] = '\0';

	codePage = GetACP();
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)pszStr,-1,NULL,0,NULL,NULL);
	char* pszStrMB = new char[mbssize+1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)pszStr, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = '\0';
	strRet = pszStrMB;
	delete [] pszStrMB;

	delete [] pszStr;
#else
	char* ptr = (char*)str.c_str();
	if (ptr[0] == (char)0xef && ptr[1] == (char)0xbb && ptr[2] == (char)0xbf)
		ptr += 3;
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	size_t n = 0, clen = 0, len = 0;
	while(len < mbssize) {
		int c = utf_bytes2char((unsigned char*)ptr);
		if (c == 0x301c) c = 0xff5e;
		if (c == 0x2016) c = 0x2225;
		if (c == 0x2212) c = 0xff0d;
		if (c == 0x00a2) c = 0xffe0;
		if (c == 0x00a3) c = 0xffe1;
		if (c == 0x00ac) c = 0xffe2;
		pszStrWC[n++] = c;
		clen = utf8len_tab[(unsigned char)*ptr];
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
	wcssize = n;
	mbssize = wcslen(pszStrWC)*8;
	char* pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
	delete[] pszStrWC;
	strRet = pszStrMB;
	delete[] pszStrMB;
#endif

	return strRet;
}

#ifdef _UNICODE
tstring utf8_to_string(tstring str) {
	return str;
}
#endif

#ifdef HAVE_ICONV
std::string convert_string(const std::string str, const std::string from_codeset, const std::string to_codeset) {
	char *dest = NULL;
	iconv_t cd;
	char *outp;
	char *p = (char *) str.c_str();
	size_t inbytes_remaining = strlen (p);
	size_t outbuf_size = inbytes_remaining + 1;
	size_t outbytes_remaining;
	size_t err;
	int have_error = 0;

	outbuf_size *= MB_LEN_MAX;
	outbytes_remaining = outbuf_size - 1;

	if (strcmp(to_codeset.c_str(), from_codeset.c_str()) == 0)
		return str.c_str();

	cd = iconv_open(to_codeset.c_str(), from_codeset.c_str());
	if (cd == (iconv_t) -1)
		return str.c_str();

	outp = dest = (char *) malloc (outbuf_size);
	if (dest == NULL)
		goto out;

again:
#if defined(_LIBICONV_VERSION) && _LIBICONV_VERSION <= 0x010A
	err = iconv(cd, (const char**)&p, &inbytes_remaining, &outp, &outbytes_remaining);
#else
	err = iconv(cd, (char**)&p, &inbytes_remaining, &outp, &outbytes_remaining);
#endif
	if (err == (size_t) - 1) {
		switch (errno) {
			case EINVAL:
				break;
			case E2BIG:
				{
					size_t used = outp - dest;
					size_t newsize = outbuf_size * 2;
					char *newdest;

					if (newsize <= outbuf_size) {
						errno = ENOMEM;
						have_error = 1;
						goto out;
					}
					newdest = (char *) realloc (dest, newsize);
					if (newdest == NULL) {
						have_error = 1;
						goto out;
					}
					dest = newdest;
					outbuf_size = newsize;

					outp = dest + used;
					outbytes_remaining = outbuf_size - used - 1;        /* -1 for NUL */

					goto again;
				}
				break;
			case EILSEQ:
				have_error = 1;
				break;
			default:
				have_error = 1;
				break;
		}
	}

	*outp = '\0';

out:
	{
		int save_errno = errno;

		if (iconv_close (cd) < 0 && !have_error) {
			/* If we didn't have a real error before, make sure we restore
			   the iconv_close error below. */
			save_errno = errno;
			have_error = 1;
		}

		if (have_error && dest) {
			free (dest);
			dest = (char*)str.c_str();
			errno = save_errno;
		}
	}

	return dest;
}
#endif

std::string cut_string(std::string str, int cells, std::string padding) {
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	size_t n = 0;
#ifdef _WIN32
	n = MultiByteToWideChar(GetACP(), 0, ptr, mbssize, pszStrWC, wcssize + 1);
	pszStrWC[n] = 0;
#else
	size_t clen = 0, len = 0;
	int tmp = mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
#endif
	wcssize = n;
	int width = 0;
	for(n = 0; n < wcssize; n++) {
		int c = wcwidth_cjk(pszStrWC[n]);
		if (width + c > cells) {
			pszStrWC[n] = 0;
			break;
		}
		width += c;
	}
	char* pszStrMB;
#ifdef _WIN32
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, NULL, 0, NULL, NULL);
	pszStrMB = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = 0;
#else
	wcssize = wcslen(pszStrWC);
	mbssize = wcssize*8;
	pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
#endif
	delete[] pszStrWC;
	std::string ret = pszStrMB;
	delete[] pszStrMB;
	if (ret.size() < str.size())
		ret += padding;
	return ret;
}

std::string cut_string_r(std::string str, int cells, std::string padding) {
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	size_t n = 0;
#ifdef _WIN32
	n = MultiByteToWideChar(GetACP(), 0, ptr, mbssize, pszStrWC, wcssize + 1);
	pszStrWC[n] = 0;
#else
	size_t clen = 0, len = 0;
	int tmp = mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			tmp = mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
#endif
	wcssize = n;
	int width = 0;
	for(n = wcssize-1; n >= 0; n--) {
		int c = wcwidth_cjk(pszStrWC[n]);
		if (width + c > cells) {
			pszStrWC[n] = 0;
			wcscpy(pszStrWC, pszStrWC+n+1);
			break;
		}
		width += c;
	}
	char* pszStrMB;
#ifdef _WIN32
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, NULL, 0, NULL, NULL);
	pszStrMB = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = 0;
#else
	wcssize = wcslen(pszStrWC);
	mbssize = wcssize*8;
	pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
#endif
	delete[] pszStrWC;
	std::string ret = pszStrMB;
	delete[] pszStrMB;
	if (ret.size() < str.size())
		ret = padding + ret;
	return ret;
}

std::vector<std::string> split_string(std::string strSrc, std::string strKey) {
	std::vector<std::string> vecLines;
	std::string strTmp = strSrc;

	size_t iIndex = 0;
	while (iIndex < strTmp.length()) {
		int iOldIndex = iIndex;
		iIndex = strTmp.find(strKey, iIndex);
		if(iIndex != std::string::npos) {
			vecLines.push_back(strTmp.substr(iOldIndex, iIndex - iOldIndex));
		} else {
			vecLines.push_back(strTmp.substr(iOldIndex));
			break;
		}
		iIndex += strKey.length();
	}
	return vecLines;
}

void split_string(std::string strSrc, std::string strKey, std::vector<std::string>& vecLines) {
	std::string strTmp = strSrc;

	vecLines.clear();
	size_t iIndex = 0;
	while (iIndex < strTmp.length()) {
		int iOldIndex = iIndex;
		iIndex = strTmp.find(strKey, iIndex);
		if(iIndex != std::string::npos) {
			std::string item = strTmp.substr(iOldIndex, iIndex - iOldIndex);
			vecLines.push_back(item);
		} else {
			std::string item = strTmp.substr(iOldIndex);
			vecLines.push_back(item);
			break;
		}
		iIndex += strKey.length();
	}
}

#ifdef _UNICODE
std::vector<tstring> split_string(tstring strSrc, tstring strKey) {
	std::vector<tstring> vecLines;
	tstring strTmp = strSrc;

	int iIndex = 0;
	while (iIndex < (int)strTmp.length()) {
		int iOldIndex = iIndex;
		iIndex = strTmp.find(strKey, iIndex);
		if(iIndex != tstring::npos) {
			tstring item = strTmp.substr(iOldIndex, iIndex - iOldIndex);
			vecLines.push_back(item);
		} else {
			tstring item = strTmp.substr(iOldIndex);
			vecLines.push_back(item);
			break;
		}
		iIndex += strKey.length();
	}
	return vecLines;
}
#endif

std::string trim_string(const std::string strSrc) {
	std::string ret = strSrc;
	int end_pos = ret.find_last_not_of(" \n");
	if(end_pos != -1) {
		ret.erase(end_pos + 1);
		end_pos = ret.find_first_not_of(" \n");
		if(end_pos != -1) ret.erase(0, end_pos);
	}
	else ret.erase(ret.begin(), ret.end());
	return ret;
}
#ifdef _UNICODE
tstring trim_string(const tstring strSrc) {
	tstring ret = strSrc;
	int end_pos = ret.find_last_not_of(' ');
	if(end_pos != -1) {
		ret.erase(end_pos + 1);
		end_pos = ret.find_first_not_of(' ');
		if(end_pos != -1) ret.erase(0, end_pos);
	}
	else ret.erase(ret.begin(), ret.end());
	return ret;
}
#endif

std::string& replace_string(std::string& strSrc, const std::string strFrom, const std::string strTo) {
	std::string::size_type n, nb = 0;

	while((n = strSrc.find(strFrom, nb)) != std::string::npos) {
		strSrc.replace(n, strFrom.size(), strTo);
		nb = n + strTo.size();
	}
	return strSrc;
}
#ifdef _UNICODE
tstring& replace_string(tstring& strSrc, const tstring strFrom, const tstring strTo) {
	tstring::size_type n, nb = 0;

	while((n = strSrc.find(strFrom, nb)) != tstring::npos) {
		strSrc.replace(n, strFrom.size(), strTo);
		nb = n + strTo.size();
	}
	return strSrc;
}
#endif

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3] = {0};
	unsigned char char_array_4[4] = {0};

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for(i = 0; (i <4) ; i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i) {
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while((i++ < 3))
			ret += '=';
	}

	return ret;
}

std::string base64_decode(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4] = {0};
	unsigned char char_array_3[3] = {0};
	std::string ret;

	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret += char_array_3[j];
	}

	return ret;
}

std::vector<char> base64_decode_binary(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4] = {0};
	unsigned char char_array_3[3] = {0};
	std::vector<char> ret;

	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret.push_back(char_array_3[i]);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret.push_back(char_array_3[j]);
	}

	return ret;
}

std::string url_decode(const std::string& url) {
	std::ostringstream rets;
	std::string hexstr;
	for(size_t n = 0; n < url.size(); n++) {
		switch(url[n]) {
		case '+':
			rets << ' ';
			break;
		case '%':
			hexstr = url.substr(n+1, 2);
			n += 2;
			if (hexstr == "26" || hexstr == "3D")
				rets << '%' << hexstr;
			else {
				std::istringstream hexstream(hexstr);
				int hexint;
				hexstream >> std::hex >> hexint;
				rets << static_cast<char>(hexint);
			}
			break;
		default:
			rets << url[n];
			break;
		}
	}
	return rets.str();
}

std::string url_encode(const std::string& url) {
	std::ostringstream rets;
	for(size_t n = 0; n < url.size(); n++) {
		unsigned char c = (unsigned char)url[n];
		if (isalnum(c) || c == '_' || c == '.' || c == '/' )
			rets << c;
		else {
			char buf[8];
			sprintf(buf, "%02x", (int)c);
			rets << '%' << buf[0] << buf[1];
		}
	}
	return rets.str();
}

std::string html_encode(const std::string& html) {
	std::string ret = html;
	replace_string(ret, "&", "&amp;");
	replace_string(ret, "<", "&lt;");
	replace_string(ret, ">", "&gt;");
	return ret;
}

std::string html_decode(const std::string& html) {
	std::string ret = html;
	replace_string(ret, "&gt;", ">");
	replace_string(ret, "&lt;", "<");
	replace_string(ret, "&nbsp;", " ");
	replace_string(ret, "&amp;", "&");
	return ret;
}

std::map<std::string, std::string> parse_querystring(const std::string& query_string) {
	std::vector<std::string> params = split_string(query_string, "&");
	std::vector<std::string>::iterator it;
	std::map<std::string, std::string> ret;
	for (it = params.begin(); it != params.end(); it++) {
		size_t end_pos = it->find_first_of("=");
		if (end_pos != std::string::npos)
			ret[it->substr(0, end_pos)] = url_decode(it->substr(end_pos + 1));
		else
			ret[*it] = "";
	}
	return ret;
}

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

#ifndef uint64
# ifdef _WIN32
#  ifdef __GNUC__
#   define uint64 unsigned __int64
#  else
#   define uint64 unsigned _int64
#  endif
# else
#  define uint64 unsigned long long
# endif
#endif

#ifndef byte
# define byte unsigned char
#endif

#ifndef _WIN32
#define _rotl(x, y) ((x<<y)|(x>>(32-y)))
#endif

#define F1(X, Y, Z) ((Z) ^ ((X) & ((Y) ^ (Z))))
#define F2(X, Y, Z) ((Y) ^ ((Z) & ((X) ^ (Y))))
#define F3(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define F4(X, Y, Z) ((Y) ^ ((X) | ~(Z)))
#define ROUND(r, a, b, c, d, F, i, s) { a = b + _rotl(a + F(b, c, d) + reinterpret_cast<const uint32 *>(block)[i] + SIN##r, s); }
#define SIN1	3614090360ul
#define SIN2	3905402710ul
#define SIN3	606105819ul
#define SIN4	3250441966ul
#define SIN5	4118548399ul
#define SIN6	1200080426ul
#define SIN7	2821735955ul
#define SIN8	4249261313ul
#define SIN9	1770035416ul
#define SIN10	2336552879ul
#define SIN11	4294925233ul
#define SIN12	2304563134ul
#define SIN13	1804603682ul
#define SIN14	4254626195ul
#define SIN15	2792965006ul
#define SIN16	1236535329ul
#define SIN17	4129170786ul
#define SIN18	3225465664ul
#define SIN19	643717713ul
#define SIN20	3921069994ul
#define SIN21	3593408605ul
#define SIN22	38016083ul
#define SIN23	3634488961ul
#define SIN24	3889429448ul
#define SIN25	568446438ul
#define SIN26	3275163606ul
#define SIN27	4107603335ul
#define SIN28	1163531501ul
#define SIN29	2850285829ul
#define SIN30	4243563512ul
#define SIN31	1735328473ul
#define SIN32	2368359562ul
#define SIN33	4294588738ul
#define SIN34	2272392833ul
#define SIN35	1839030562ul
#define SIN36	4259657740ul
#define SIN37	2763975236ul
#define SIN38	1272893353ul
#define SIN39	4139469664ul
#define SIN40	3200236656ul
#define SIN41	681279174ul
#define SIN42	3936430074ul
#define SIN43	3572445317ul
#define SIN44	76029189ul
#define SIN45	3654602809ul
#define SIN46	3873151461ul
#define SIN47	530742520ul
#define SIN48	3299628645ul
#define SIN49	4096336452ul
#define SIN50	1126891415ul
#define SIN51	2878612391ul
#define SIN52	4237533241ul
#define SIN53	1700485571ul
#define SIN54	2399980690ul
#define SIN55	4293915773ul
#define SIN56	2240044497ul
#define SIN57	1873313359ul
#define SIN58	4264355552ul
#define SIN59	2734768916ul
#define SIN60	1309151649ul
#define SIN61	4149444226ul
#define SIN62	3174756917ul
#define SIN63	718787259ul
#define SIN64	3951481745ul

typedef struct {
	size_t bufused;
	uint64 nbits;
	uint32 state[4];
	uint8 buffer[64];
} md5_context;

void md5_process(uint32 state[4], const byte block[64]) {
	uint32 A = state[0], B = state[1], C = state[2], D = state[3];
	ROUND(1, A, B, C, D, F1, 0, 7);		ROUND(2, D, A, B, C, F1, 1, 12);
	ROUND(3, C, D, A, B, F1, 2, 17);	ROUND(4, B, C, D, A, F1, 3, 22);
	ROUND(5, A, B, C, D, F1, 4, 7);		ROUND(6, D, A, B, C, F1, 5, 12);
	ROUND(7, C, D, A, B, F1, 6, 17);	ROUND(8, B, C, D, A, F1, 7, 22);
	ROUND(9, A, B, C, D, F1, 8, 7);		ROUND(10, D, A, B, C, F1, 9, 12);
	ROUND(11, C, D, A, B, F1, 10, 17);	ROUND(12, B, C, D, A, F1, 11, 22);
	ROUND(13, A, B, C, D, F1, 12, 7);	ROUND(14, D, A, B, C, F1, 13, 12);
	ROUND(15, C, D, A, B, F1, 14, 17);	ROUND(16, B, C, D, A, F1, 15, 22);
	ROUND(17, A, B, C, D, F2, 1, 5);	ROUND(18, D, A, B, C, F2, 6, 9);
	ROUND(19, C, D, A, B, F2, 11, 14);	ROUND(20, B, C, D, A, F2, 0, 20);
	ROUND(21, A, B, C, D, F2, 5, 5);	ROUND(22, D, A, B, C, F2, 10, 9);
	ROUND(23, C, D, A, B, F2, 15, 14);	ROUND(24, B, C, D, A, F2, 4, 20);
	ROUND(25, A, B, C, D, F2, 9, 5);	ROUND(26, D, A, B, C, F2, 14, 9);
	ROUND(27, C, D, A, B, F2, 3, 14);	ROUND(28, B, C, D, A, F2, 8, 20);
	ROUND(29, A, B, C, D, F2, 13, 5);	ROUND(30, D, A, B, C, F2, 2, 9);
	ROUND(31, C, D, A, B, F2, 7, 14);	ROUND(32, B, C, D, A, F2, 12, 20);
	ROUND(33, A, B, C, D, F3, 5, 4);	ROUND(34, D, A, B, C, F3, 8, 11);
	ROUND(35, C, D, A, B, F3, 11, 16);	ROUND(36, B, C, D, A, F3, 14, 23);
	ROUND(37, A, B, C, D, F3, 1, 4);	ROUND(38, D, A, B, C, F3, 4, 11);
	ROUND(39, C, D, A, B, F3, 7, 16);	ROUND(40, B, C, D, A, F3, 10, 23);
	ROUND(41, A, B, C, D, F3, 13, 4);	ROUND(42, D, A, B, C, F3, 0, 11);
	ROUND(43, C, D, A, B, F3, 3, 16);	ROUND(44, B, C, D, A, F3, 6, 23);
	ROUND(45, A, B, C, D, F3, 9, 4);	ROUND(46, D, A, B, C, F3, 12, 11);
	ROUND(47, C, D, A, B, F3, 15, 16);	ROUND(48, B, C, D, A, F3, 2, 23);
	ROUND(49, A, B, C, D, F4, 0, 6);	ROUND(50, D, A, B, C, F4, 7, 10);
	ROUND(51, C, D, A, B, F4, 14, 15);	ROUND(52, B, C, D, A, F4, 5, 21);
	ROUND(53, A, B, C, D, F4, 12, 6);	ROUND(54, D, A, B, C, F4, 3, 10);
	ROUND(55, C, D, A, B, F4, 10, 15);	ROUND(56, B, C, D, A, F4, 1, 21);
	ROUND(57, A, B, C, D, F4, 8, 6);	ROUND(58, D, A, B, C, F4, 15, 10);
	ROUND(59, C, D, A, B, F4, 6, 15);	ROUND(60, B, C, D, A, F4, 13, 21);
	ROUND(61, A, B, C, D, F4, 4, 6);	ROUND(62, D, A, B, C, F4, 11, 10);
	ROUND(63, C, D, A, B, F4, 2, 15);	ROUND(64, B, C, D, A, F4, 9, 21);
	state[0] += A; state[1] += B; state[2] += C; state[3] += D;
}

void md5_starts(md5_context *ctx) {
	ctx->nbits = 0;
	ctx->bufused = 0;

	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;
}

void md5_update(md5_context *ctx, uint8 *input, uint32 length) {
	if (ctx->bufused != 0) {
		size_t bufremain = 64 - ctx->bufused;
		if (length < bufremain) {
			memcpy(ctx->buffer + ctx->bufused, input, length);
			ctx->bufused += length;
			return;
		}
		else {
			memcpy(ctx->buffer + ctx->bufused, input, bufremain);
			md5_process(ctx->state, ctx->buffer);
			length -= bufremain;
			input = reinterpret_cast<uint8 *>(input) + bufremain;
		}
	}
	while (length >= 64) {
		md5_process(ctx->state, reinterpret_cast<uint8 *>(input));
		length -= 64;
		input = reinterpret_cast<uint8 *>(input) + 64;
		ctx->nbits += 512;
	}
	if ((ctx->bufused = length) != 0) {
		memcpy(ctx->buffer, input, length);
	}
}

void md5_finish(md5_context *ctx, byte digest[16]) {
	memcpy(digest, ctx->state, 16);
	memset(ctx->buffer + ctx->bufused, 0, 64 - ctx->bufused);
	ctx->buffer[ctx->bufused] = 0x80;
	if (ctx->bufused < 56) {
		reinterpret_cast<uint64 *>(ctx->buffer)[7] = ctx->nbits + ctx->bufused * 8;
		md5_process(reinterpret_cast<uint32 *>(digest), ctx->buffer);
	}
	else {
		byte extra[64];
		md5_process(reinterpret_cast<uint32 *>(digest), ctx->buffer);
		memset(extra, 0, 56);
		reinterpret_cast<uint64 *>(extra)[7] = ctx->nbits + ctx->bufused * 8;
		md5_process(reinterpret_cast<uint32 *>(digest), extra);
	}
}

std::string md5_string(const std::string& input) {
	std::string digest;

	md5_context ctx;
	md5_starts(&ctx);
	md5_update(&ctx, (uint8*)&input[0], input.size());
	digest.resize(16);
	md5_finish(&ctx, (uint8*)&digest[0]);

	return digest;
}

std::string string_to_hex(const std::string& input) {
	const static char hex_table[] = "0123456789abcdef";
	std::string temp;
	temp.resize(input.size() * 2);
	std::string::size_type i;
	std::string::const_iterator itr = input.begin();
	for (i = 0; itr != input.end(); itr++, i++) {
		temp[i++] = hex_table[(*itr & 0xF0) >> 4];
		temp[i] = hex_table[*itr & 0x0F];
	}
	return temp;
}

void set_priv(const char *chuser_name, const char *chroot_dir, const char *title) {
#ifndef _WIN32
	struct passwd *pw;

	if (chuser_name[0] == '\0' || chroot_dir[0] == '\0')
		return;

	if ((pw = getpwnam(chuser_name)) == NULL) {
		fprintf(stderr, "failed to get user \"%s\"\n", chuser_name);
		fflush(stderr);
		exit(255);
	}
	if (chroot(chroot_dir) != 0) {
		fprintf(stderr, "can't chroot %s\n", chroot_dir);
		fflush(stderr);
		exit(255);
	}
	if (chdir("/") != 0) {
		fprintf(stderr, "can't change directory '/'\n");
		fflush(stderr);
		exit(255);
	}

#ifdef HAVE_SETPROCTITLE
	if (title != NULL)
		setproctitle("%s [%s]", pw->pw_name, title);
#endif

	/*
	 * Drop privileges and clear the group access list
	 */
	if (setgroups(1, &pw->pw_gid) == -1 ||
#if defined(HAVE_SETRESUID) && !defined(BROKEN_SETRESUID)
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1
#elif defined(HAVE_SETREUID) && !defined(BROKEN_SETREUID)
	    setregid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
	    setreuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1
#else
# ifndef SETEUID_BREAKS_SETUID
	    setegid(pw->pw_gid) == -1 ||
	    seteuid(pw->pw_uid) == -1
# else
	    setgid(pw->pw_gid) == -1 ||
	    setuid(pw->pw_uid) == -1
# endif
#endif
	   ) {
		fprintf(stderr, "can't drop privileges\n");
		fflush(stderr);
		exit(255);
	}
#else
	fprintf(stderr, "win32 does not support set_priv()\n");
	fflush(stderr);
	exit(255);
#endif
}

}
