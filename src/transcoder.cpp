/*
 * Copyright 2004-2005 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/helpers/transcoder.h>
#include <log4cxx/helpers/pool.h>
#include <stdlib.h>
//#include <log4cxx/config.h>

using namespace log4cxx;
using namespace log4cxx::helpers;



/**
*   Appends an external string to an
*     internal string.
*/
#if defined(LOG4CXX_LOGCHAR_IS_WCHAR)

//
//
//  TODO: Have observed strange behavior with C RTL implementations
//    conversion seems to be fine during static object
//    initialization then wcsnrtombs starts failing on simple strings
//    Probably have overwritten the heap and can eventually be
//    checked with Valgrind and the following #IF can be
//    modified.
//
#if 1 //!((HAVE_MBSNRTOWCS && HAVE_WCSNRTOMBS) || (LOG4CXX_HAVE_MBSNRTOWCS && LOG4CXX_HAVE_WCSNRTOMBS))

//
//   If the GNU RTL functions mbsnrtowcs and wcsnrtombs are not available
//      provide alternative implementations based on mbtowc and wctomb.
//   Presumable the library implementation of the counted string
//      wide-to-multi conversions are more efficient.
//
//   If both the C RTL and these implementations are available
//      these will be used.

namespace log4cxx {
      namespace helpers {

        struct mbstate_t {};

        size_t mbsnrtowcs(wchar_t *dest, const char **src,
            size_t srcLen, size_t destLen, mbstate_t *ps) {
            const char* srcEnd = *src + srcLen;
            wchar_t* current = dest;
            const wchar_t* destEnd = dest + destLen;
            while(*src < srcEnd && current < destEnd) {
              if (**src == 0) {
                *src = NULL;
                return current - dest;
              }
              size_t mblen = mbtowc(current, *src, srcEnd - *src);
              if (mblen == (size_t) -1) {
                return mblen;
              }
              *src += mblen;
              current++;
            }
            return current - dest;
        }

        size_t wcsnrtombs(char *dest, const wchar_t **src, size_t srcLen,
            size_t destLen, mbstate_t *ps) {
            const wchar_t* srcEnd = *src + srcLen;
            char* current = dest;
            const char* destEnd = dest + destLen;
            char buf[12];
            while(*src < srcEnd && current < destEnd) {
              if (**src ==  0) {
                *src = NULL;
                return current - dest;
              }
              size_t mblen = wctomb(buf, **src);
              //
              //   not representable
              if (mblen == (size_t) -1) {
                return mblen;
              }
              //
              //   if not enough space then return length so far
              //
              if(current + mblen > destEnd) {
                return current - dest;
              }
              //
              //   copy from temp buffer to destination
              //
              memcpy(current, buf, mblen);
              current += mblen;
              (*src)++;
            }
            return current - dest;
        }

    }
}
#endif


void Transcoder::decode(const char* src, size_t len, LogString& dst) {
  wchar_t buf[BUFSIZE];
  mbstate_t ps;
  const char* end = src + len;
  for(const char* in = src;
     in < end && in != NULL;) {
     const char* start = in;
     size_t rv = mbsnrtowcs(buf, &in, end - in, BUFSIZE, &ps);
     if (rv == (size_t) -1) {
       //
       //    bad sequence encounted
       //
       size_t convertableLength = in - start;
       in = start;
       if (convertableLength > 0) {
          rv = mbsnrtowcs(buf, &in, convertableLength, BUFSIZE, &ps);
          if (rv != (size_t) -1) {
            dst.append(buf, rv);
          }
       }
       dst.append(1, LOG4CXX_STR('?'));
       in++;
     } else {
       dst.append(buf, rv);
     }
  }
}

void Transcoder::decode(const wchar_t* src, size_t len, LogString& dst) {
  dst.append(src, len);
}

void Transcoder::encode(const LogString& src, std::string& dst) {
  char buf[BUFSIZE];
  mbstate_t ps;
  size_t srcLen = src.length();
  const wchar_t* pSrc = src.data();
  const wchar_t* pEnd = pSrc + srcLen;
  for(const wchar_t* in = pSrc;
      in < pEnd && in != NULL;) {
        const wchar_t* start = in;
        size_t toConvert = pEnd - in;
        size_t rv = wcsnrtombs(buf, &in, toConvert, BUFSIZE, &ps);
        //   illegal sequence, convert only the initial fragment
        if (rv == (size_t) -1) {
          size_t convertableLength = (in - start);
          in = start;
          if (convertableLength > 0) {
            rv = wcsnrtombs(buf, &in, convertableLength, BUFSIZE, &ps);
            if (rv != (size_t) -1) {
              dst.append(buf, rv);
            }
          }
          //
          //  represent character with an escape sequence
          //
          dst.append("\\u");
          const char* hexdigits = "0123456789ABCDEF";
          wchar_t unencodable = *in;
          dst.append(1, hexdigits[(unencodable >> 12) & 0x0F]);
          dst.append(1, hexdigits[(unencodable >> 8) & 0x0F]);
          dst.append(1, hexdigits[(unencodable >> 4) & 0x0F]);
          dst.append(1, hexdigits[unencodable & 0x0F]);
          in++;
        } else {
          dst.append(buf, rv);
        }
  }
}

void Transcoder::encode(const LogString& src, std::wstring& dst) {
  dst.append(src);
}


bool Transcoder::equals(const char* str1, size_t len, const LogString& str2) {
    if (len == str2.length()) {
        LogString decoded;
        decode(str1, len, decoded);
        return decoded == str2;
    }
    return false;
}


bool Transcoder::equals(const wchar_t* str1, size_t len, const LogString& str2) {
    if (len == str2.length()) {
        LogString decoded;
        decode(str1, len, decoded);
        return decoded == str2;
    }
    return false;
}

#endif



