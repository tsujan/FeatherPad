/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-2.0+ <https://spdx.org/licenses/GPL-2.0+.html>
 */

/* This file was adapted from
 * Leafpad - GTK+ based simple text editor
 * File: src/encoding.c
 * Copyright: 2004-2005 Tarot Osuji
 * License: GPL-2.0+
 * Homepage: http://tarot.freeshell.org/leafpad/
 */


#include <stdlib.h> // getenv
#include <langinfo.h> // CODESET, nl_langinfo
#include <stdint.h> // uint8_t, uint32_t
#include <locale.h> // needed by FreeBSD for setlocale
#include "encoding.h"

namespace FeatherPad {

#define MAX_COUNTRY_NUM 10
enum
{
    IANA = 0,
    OPENI18N,
    CODEPAGE,
    ENCODING_MAX_ITEM_NUM
};
/*------------------------*/
/* encoding numbers */
enum
{
    LATIN1 = 0,
    LATIN2,
    LATIN3,
    LATIN4,
    LATINC,
    LATINC_UA,
    LATINC_TJ,
    LATINA,
    LATING,
    LATINH,
    LATIN5,
    CHINESE_CN,
    CHINESE_TW,
    CHINESE_HK,
    JAPANESE,
    KOREAN,
    VIETNAMESE,
    THAI,
    GEORGIAN,
    TOTAL_NUM // 19
};
/*------------------------*/
static const std::string countryTable[TOTAL_NUM][MAX_COUNTRY_NUM] =
{
                        /* list of countries using each encoding set */
    /* LATIN1 */        {""  ,    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATIN2 */        {"cs",    "hr", "hu", "pl", "ro", "sk", "sl", "sq", "sr", "uz"},
    /* LATIN3 */        {"eo",    "mt", "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATIN4 */        {"et",    "lt", "lv", "mi", "",   "",   "",   "",   "",   ""  },
    /* LATINC */        {"be",    "bg", "ky", "mk", "mn", "ru", "tt", "",   "",   ""  },
    /* LATINC_UA */     {"uk",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATINC_TJ */     {"tg",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATINA */        {"ar",    "fa", "ur", "",   "",   "",   "",   "",   "",   ""  },
    /* LATING */        {"el",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATINH */        {"he",    "yi", "",   "",   "",   "",   "",   "",   "",   ""  },
    /* LATIN5 */        {"az",    "tr", "",   "",   "",   "",   "",   "",   "",   ""  },
    /* CHINESE_CN */    {"zh_CN", "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* CHINESE_TW */    {"zh_TW", "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* CHINESE_HK */    {"zh_HK", "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* JAPANESE */      {"ja",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* KOREAN */        {"ko",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* VIETNAMESE */    {"vi",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* THAI */          {"th",    "",   "",   "",   "",   "",   "",   "",   "",   ""  },
    /* GEORGIAN */      {"ka",    "",   "",   "",   "",   "",   "",   "",   "",   ""  }
};
/*------------------------*/
static const std::string encodingTable[TOTAL_NUM][ENCODING_MAX_ITEM_NUM] =
{
                        /*  IANA            OpenI18N            CODEPAGE */
    /* LATIN1 */        { "ISO-8859-1",     "ISO-8859-15",      "CP1252" },
    /* LATIN2 */        { "ISO-8859-2",     "ISO-8859-16",      "CP1250" },
    /* LATIN3 */        { "ISO-8859-3",     "",                 ""       },
    /* LATIN4 */        { "ISO-8859-4",     "ISO-8859-13",      "CP1257" },
    /* LATINC */        { "ISO-8859-5",     "KOI8-R",           "CP1251" },
    /* LATINC_UA */     { "ISO-8859-5",     "KOI8-U",           "CP1251" },
    /* LATINC_TJ */     { "ISO-8859-5",     "KOI8-T",           "CP1251" },
    /* LATINA */        { "ISO-8859-6",     "",                 "CP1256" },
    /* LATING */        { "ISO-8859-7",     "",                 "CP1253" },
    /* LATINH */        { "ISO-8859-8",     "",                 "CP1255" },
    /* LATIN5 */        { "ISO-8859-9",     "",                 "CP1254" },
    /* CHINESE_CN */    { "GB2312",         "GB18030",          "CP936"  },
    /* CHINESE_TW */    { "BIG5",           "EUC-TW",           "CP950"  },
    /* CHINESE_HK */    { "BIG5",           "BIG5-HKSCS",       "CP950"  },
    /* JAPANESE */      { "ISO-2022-JP",    "EUC-JP",           "CP932"  },
    /* KOREAN */        { "ISO-2022-KR",    "EUC-KR",           "CP949"  },
    /* VIETNAMESE */    { "",               "VISCII",           "CP1258" },
    /* THAI */          { "",               "TIS-620",          "CP874"  },
    /* GEORGIAN */      { "",               "GEORGIAN-PS",      ""       }
};
/*************************/
static unsigned int getLocaleNum()
{
    static unsigned int code = 0; // for me
    std::string env;
    const char *lcAll = getenv ("LC_ALL");
    const char *lang = getenv ("LANG");
    int j = 1;

    if (lcAll)
        env = lcAll;
    if (env.empty() && lang != NULL)
        /* if there's no value for all locales,
           get the native language, e.g. en_US.utf8*/
        env = lang;
    if (!env.empty() && env.length() >= 2)
        while (code == 0 && j < TOTAL_NUM)
        {
            for (int i = 0; i < MAX_COUNTRY_NUM; i++)
            {
                if (countryTable[j][i].empty())
                    break;
                if (env.compare (0, countryTable[j][i].length(), countryTable[j][i]) == 0)
                {
                    code = j;
                    break;
                }
            }
            j++;
        }

    return code;
}
/*************************/
static unsigned int localeNum = getLocaleNum();
static const std::string encodingItem[ENCODING_MAX_ITEM_NUM] = {encodingTable[localeNum][0],
                                                                encodingTable[localeNum][1],
                                                                encodingTable[localeNum][2]};
/*************************/
static const std::string detectCharsetLatin (const char *text)
{
    uint8_t c = *text;
    bool noniso = false;
    bool noniso15 = false;
    uint32_t xl = 0, xa = 0, xac = 0, xcC = 0, xcC1 = 0, xcS = 0, xcna = 0;
    /* the OpenI18N for the locale ("ISO-8859-15" for me) */
    std::string charset = encodingItem[OPENI18N];

    while (c != '\0')
    {
        if (c >= 0x41 && c <= 0x7A)
            /* ordinary Latin letters */
            xl ++;
        else if (c >= 0x80 && c <= 0x9F)
        {
            noniso = true;
        }
        else if (c >= 0xC0)
        {
            /* Arabic or Cyrillic letters */
            xac ++;
            if (c >= 0xC0 && c <= 0xCF)
                /* Cyrillic capital letters */
                xcC++;
            else if (c >= 0xD0 && c <= 0xDF)
            {
                /* Cyrillic capital letters again */
                xcC1++;
                if (c == 0xDE || c == 0xDF)
                    /* not used in ISO-8859-15 (Icelandic or German) */
                    noniso15 = true;
            }
            else if (c >= 0xE0)
            {
                /* Cyrillic small letters */
                xcS++;
                /* Cyrillic but not Arabic letters */
                if (c == 0xE0 || c == 0xE2 || (c >= 0xE7 && c <= 0xEB)
                    || c == 0xEE || c == 0xEF || c == 0xF4 || c == 0xF9
                    || c == 0xFB || c == 0xFC)
                    xcna ++;
                /* Arabic LAM to HEH */
                else if (c == 0xE1 || c == 0xE3 || c == 0xE4
                         || c == 0xE5 || c == 0xE6)
                    xa++;
            }
        }
        c = *text++;
    }

    /* when there is a difference fom ISO-8859-1 and ISO-8859-15,
       and ordinary Latin letters are more than Arabic ones,
       and the text isn't Cyrillic KOI8-U */
    if (noniso && xl >= xac && (xcC1 + xcS >= xcC || xcna == 0))
        charset = "CP1252"; // Windows-1252
    else // if (xl < xac)
    {
        if (!noniso && xcC + xcS < xcC1)
            charset = "ISO-8859-15"; // FIXME: ISO-8859-5 ?
        /* this is very tricky and I added it later */
        else if (!noniso && xcC + xcC1 + xa >= xcS - xa && !(xcC1 + xcS < xcC && xcna > 0))
            charset = "ISO-8859-1";
        else if (xcC + xcC1 < xcS && xcna > 0)
        {
            if (noniso || noniso15) // FIXME: this is very inefficient
                charset = "CP1251"; // Cyrillic-1251
            else
                charset = "ISO-8859-15";
        }
        else if (xcC1 + xcS < xcC && xcna > 0)
            charset = "KOI8-U"; // Cyrillic-KOI
        /* this should cover most cases */
        else if (noniso || xcC + xcC1 + xa >= xcS - xa)
            charset = "CP1256"; // MS Windows Arabic
    }

    return charset;
}
/*************************/
static const std::string detectCharsetCyrillic (const char *text)
{
    uint8_t c = *text;
    bool noniso = false;
    uint32_t xl = 0, xa = 0, xac = 0, xcC = 0, xcC1 = 0, xcS = 0, xcna = 0;
    std::string charset = encodingItem[OPENI18N];

    while (c != '\0')
    {
        if (c >= 0x41 && c <= 0x7A)
            xl ++;
        if (c >= 0x80 && c <= 0x9F)
            noniso = true;
        else if (c >= 0xC0)
        {
            xac ++;
            if (c >= 0xC0 && c <= 0xCF)
                xcC++;
            else if (c >= 0xD0 && c <= 0xDF)
                xcC1++;
            else if (c >= 0xE0)
            {
                xcS++;
                if (c == 0xE0 || c == 0xE2 || (c >= 0xE7 && c <= 0xEB)
                    || c == 0xEE || c == 0xEF || c == 0xF4 || c == 0xF9
                    || c == 0xFB || c == 0xFC)
                    xcna ++;
                else if (c == 0xE1 || c == 0xE3 || c == 0xE4
                         || c == 0xE5 || c == 0xE6)
                    xa++;
            }
        }
        c = *text++;
    }

    if (xl < xac)
    {
        if (!noniso && (xcC + xcS < xcC1))
            charset = "ISO-8859-5";
        else if (xcC + xcC1 < xcS && xcna > 0)
            charset = "CP1251";
        else if (xcC1 + xcS < xcC && xcna > 0)
            charset = "KOI8-U";
        else if (noniso || xcC + xcC1 + xa >= xcS - xa)
            charset = "CP1256";
    }

    return charset;
}
/*************************/
static const std::string detectCharsetWinArabic (const char *text)
{
    uint8_t c = *text;
    uint32_t xl = 0, xa = 0;
    std::string charset = encodingItem[IANA];

    while (c != '\0')
    {
        if (c >= 0x41 && c <= 0x7A)
            xl ++;
        else if (c >= 0xC0)
            xa ++;
        c = *text++;
    }

    if (xl < xa)
        charset = "CP1256";

    return charset;
}
/*************************/
static const std::string detectCharsetChinese (const char *text)
{
    uint8_t c = *text;
    std::string charset = encodingItem[IANA];

    while ((c = *text++) != '\0')
    {
        if (c >= 0x81 && c <= 0x87)
        {
            charset = "GB18030";
            break;
        }
        else if (c >= 0x88 && c <= 0xA0)
        {
            c = *text++;
            if ((c >= 0x30 && c <= 0x39) || (c >= 0x80 && c <= 0xA0))
            {
                charset = "GB18030";
                break;
            } //else GBK/Big5-HKSCS cannot determine
        }
        else if ((c >= 0xA1 && c <= 0xC6) || (c >= 0xC9 && c <= 0xF9))
        {
            c = *text++;
            if (c >= 0x40 && c <= 0x7E)
                charset = "BIG5";
            else if ((c >= 0x30 && c <= 0x39) || (c >= 0x80 && c <= 0xA0))
            {
                charset = "GB18030";
                break;
            }
        }
        else if (c >= 0xC7)
        {
            c = *text++;
            if ((c >= 0x30 && c <= 0x39) || (c >= 0x80 && c <= 0xA0))
            {
                charset = "GB18030";
                break;
            }
        }
    }

    return charset;
}
/*************************/
static const std::string detectCharsetJapanese (const char *text)
{
    uint8_t c = *text;
    std::string charset = "";

    while (charset.empty() && (c = *text++) != '\0')
    {
        if (c >= 0x81 && c <= 0x9F)
        {
            if (c == 0x8E) /* SS2 */
            {
                c = *text++;
                if ((c >= 0x40 && c <= 0xA0) || (c >= 0xE0 && c <= 0xFC))
                    charset = "CP932";
            }
            else if (c == 0x8F) /* SS3 */
            {
                c = *text++;
                if (c >= 0x40 && c <= 0xA0)
                    charset = "CP932";
                else if (c >= 0xFD)
                    break;
            }
            else
                charset = "CP932";
        }
        else if (c >= 0xA1 && c <= 0xDF)
        {
            c = *text++;
            if (c <= 0x9F)
                charset = "CP932";
            else if (c >= 0xFD)
                break;
        }
        else if (c >= 0xE0 && c <= 0xEF)
        {
            c = *text++;
            if (c >= 0x40 && c <= 0xA0)
                charset = "CP932";
            else if (c >= 0xFD)
                break;
        }
        else if (c >= 0xF0)
            break;
    }

    if (charset.empty())
        charset = "EUC-JP";

    return charset;
}
/*************************/
static const std::string detectCharsetKorean (const char *text)
{
    uint8_t c = *text;
    bool noneuc = false;
    bool nonjohab = false;
    std::string charset = "";

    while (charset.empty() && (c = *text++) != '\0')
    {
        if (c >= 0x81 && c < 0x84)
        {
            charset = "CP949";
        }
        else if (c >= 0x84 && c < 0xA1)
        {
            noneuc = true;
            c = *text++;
            if ((c > 0x5A && c < 0x61) || (c > 0x7A && c < 0x81))
                charset = "CP1361";
            else if (c == 0x52 || c == 0x72 || c == 0x92 || (c > 0x9D && c < 0xA1)
                     || c == 0xB2 || (c > 0xBD && c < 0xC1) || c == 0xD2
                     || (c > 0xDD && c < 0xE1) || c == 0xF2 || c == 0xFE)
                charset = "CP949";
        }
        else if (c >= 0xA1 && c <= 0xC6)
        {
            c = *text++;
            if (c < 0xA1)
            {
                noneuc = true;
                if ((c > 0x5A && c < 0x61) || (c > 0x7A && c < 0x81))
                    charset = "CP1361";
                else if (c == 0x52 || c == 0x72 || c == 0x92 || (c > 0x9D && c < 0xA1))
                    charset = "CP949";
                else if (c == 0xB2 || (c > 0xBD && c < 0xC1) || c == 0xD2
                         || (c > 0xDD && c < 0xE1) || c == 0xF2 || c == 0xFE)
                    nonjohab = true;
            }
        }
        else if (c > 0xC6 && c <= 0xD3)
        {
            c = *text++;
            if (c < 0xA1)
                charset = "CP1361";
        }
        else if (c > 0xD3 && c < 0xD8)
        {
            nonjohab = true;
            c = *text++;
        }
        else if (c >= 0xD8)
        {
            c = *text++;
            if (c < 0xA1)
                charset = "CP1361";
        }
        if (noneuc && nonjohab)
            charset = "CP949";
    }

    if (charset.empty())
    {
        if (noneuc)
            charset = "CP949";
        else
            charset = "EUC-KR";
    }

    return charset;

}
/*************************/
// The character set of the locale
// ("UTF-8" for me)
static const std::string getDefaultCharset()
{
    if (setlocale (LC_ALL, "") == NULL)
        return "UTF-8"; // something's wrong; fall back to UTF-8
    const std::string charset = nl_langinfo (CODESET);
    return charset;
}
/*************************/
static bool detect_noniso (const char *text)
{
    uint8_t c = *text;

    while ((c = *text++) != '\0')
    {
        if (c >= 0x80 && c <= 0x9F)
            return true;
    }
    return false;
}
/*************************/
/* In the GTK+ version, I used g_utf8_validate()
   but this function validates UTF-8 directly
   and seems faster than using QTextCodec::ConverterState
   with QTextCodec::toUnicode(), which may give incorrect results. */
bool validateUTF8 (const QByteArray byteArray)
{
    const char *string = byteArray.constData();

    const unsigned char *bytes = (const unsigned char*)string;
    unsigned int cp; // code point
    int bn; // bytes number

    while (*bytes != 0x00)
    {
        /* assuming that UTF-8 maps a sequence of 1-4 bytes,
           we find the code point and the number of bytes */
        if ((*bytes & 0x80) == 0x00)
        { // 0xxxxxxx, all ASCII characters (0-127)
            cp = (*bytes & 0x7F);
            bn = 1;
        }
        else if ((*bytes & 0xE0) == 0xC0)
        { // 110xxxxx 10xxxxxx
            cp = (*bytes & 0x1F);
            bn = 2;
        }
        else if ((*bytes & 0xF0) == 0xE0)
        { // 1110xxxx 10xxxxxx 10xxxxxx
            cp = (*bytes & 0x0F);
            bn = 3;
        }
        else if ((*bytes & 0xF8) == 0xF0)
        { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            cp = (*bytes & 0x07);
            bn = 4;
        }
        else
            return false;

        bytes += 1;
        for (int i = 1; i < bn; ++i)
        {
            /* all the other bytes should be of the form 10xxxxxx */
            if ((*bytes & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (*bytes & 0x3F);
            bytes += 1;
        }

        if (cp > 0x10FFFF // max code point by definition
            /* the range from 0xd800 to 0xdfff is reserved
               for use with UTF-16 and excluded from UTF-8 */
            || (cp >= 0xD800 && cp <= 0xDFFF)
            /* logically impossible situations */
            || (cp <= 0x007F && bn != 1)
            || (cp >= 0x0080 && cp <= 0x07FF && bn != 2)
            || (cp >= 0x0800 && cp <= 0xFFFF && bn != 3)
            || (cp >= 0x10000 && cp <= 0x1FFFFF && bn != 4))
        {
            return false;
        }
    }

    return true;
}
/*************************/
const QString detectCharset (const QByteArray& byteArray)
{
    const char* text = byteArray.constData();
    uint8_t c = *text;
    std::string charset;

    if (validateUTF8 (byteArray))
    {
        while ((c = *text++) != '\0')
        {
            if (c > 0x7F)
            {
                charset = "UTF-8";
                break;
            }
            if (c == 0x1B) /* ESC */
            {
                c = *text++;
                if (c == '$')
                {
                    c = *text++;
                    switch (c)
                    {
                    case 'B': // JIS X 0208-1983
                    case '@': // JIS X 0208-1978
                        charset = "ISO-2022-JP";
                        continue;
                    case 'A': // GB2312-1980
                        charset = "ISO-2022-JP-2";
                        break;
                    case '(':
                        c = *text++;
                        switch (c)
                        {
                        case 'C': // KSC5601-1987
                        case 'D': // JIS X 0212-1990
                            charset = "ISO-2022-JP-2";
                        }
                        break;
                    case ')':
                        c = *text++;
                        if (c == 'C')
                            charset = "ISO-2022-KR"; // KSC5601-1987
                    }
                    break;
                }
            }
        }
        if (charset.empty())
            charset = getDefaultCharset();
    }

    if (charset.empty())
    {
        switch (localeNum)
        {
            case LATIN1:
                /* Windows-1252 */
                charset = detectCharsetLatin (text);
                break;
            case LATINC:
            case LATINC_UA:
            case LATINC_TJ:
                /* Cyrillic */
                charset = detectCharsetCyrillic (text);
                break;
            case LATINA:
                /* MS Windows Arabic */
                charset = detectCharsetWinArabic (text);
                break;
            case CHINESE_CN:
            case CHINESE_TW:
            case CHINESE_HK:
                charset = detectCharsetChinese (text);
                break;
            case JAPANESE:
                charset = detectCharsetJapanese (text);
                break;
            case KOREAN:
                charset = detectCharsetKorean (text);
                break;
            case VIETNAMESE:
            case THAI:
            case GEORGIAN:
                charset = encodingItem[OPENI18N];
                break;
            default:
                if (getDefaultCharset() != "UTF-8")
                    charset = getDefaultCharset();
                else if (detect_noniso (text))
                    charset = encodingItem[CODEPAGE];
                else
                    charset = encodingItem[OPENI18N];
                if (charset.empty())
                    charset = encodingItem[IANA];
        }
    }

    return QString::fromStdString (charset);
}

}
