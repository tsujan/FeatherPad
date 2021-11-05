/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2021 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
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
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include "loading.h"
#include "encoding.h"
#include <QFile>
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif

namespace FeatherPad {

Loading::Loading (const QString& fname, const QString& charset, bool reload,
                  int restoreCursor, int posInLine,
                  bool forceUneditable, bool multiple) :
    fname_ (fname),
    charset_ (charset),
    reload_ (reload),
    restoreCursor_ (restoreCursor),
    posInLine_ (posInLine),
    forceUneditable_ (forceUneditable),
    multiple_ (multiple),
    skipNonText_ (true)
{}
/*************************/
Loading::~Loading() {}
/*************************/
void Loading::run()
{
    if (!QFile::exists (fname_))
    {
        emit completed (QString(), fname_,
                        charset_.isEmpty() ? "UTF-8" : charset_,
                        false, false, 0, 0, false, multiple_);
        return;
    }

    QFile file (fname_);
    if (file.size() > 100*1024*1024) // don't open files with sizes > 100 Mib
    {
        emit completed (QString(), fname_);
        return;
    }
    if (!file.open (QFile::ReadOnly))
    {
        emit completed();
        return;
    }

    /* read the file character by character to know
       if it includes null (and because that's faster) */
    bool enforced = !charset_.isEmpty();
    bool hasNull = false;
    QByteArray data;
    char c;
    qint64 charSize = sizeof (char); // 1
    int num = 0;
    if (enforced)
    { // no need to check for the null character here
        while (file.read (&c, charSize) > 0)
        {
            if (c == '\n' || c == '\r')
                num = 0;
            if (num < 500004) // a multiple of 4 (for UTF-16/32)
                data.append (c);
            else
                forceUneditable_ = true;
            ++num;
        }
    }
    else
    {
        unsigned char C[4];
        /* checking 4 bytes is enough to guess
           whether the encoding is UTF-16 or UTF-32 */
        while (num < 4 && file.read (&c, charSize) > 0)
        {
            data.append (c);
            if (c == '\0')
                hasNull = true;
            C[num] = static_cast<unsigned char>(c);
            ++ num;
        }
        if (num == 2 && ((C[0] != '\0' && C[1] == '\0') || (C[0] == '\0' && C[1] != '\0')))
            charset_ = "UTF-16"; // single character
        else if (num == 4)
        {
            bool readMore (true);
            if (hasNull)
            {
                if ((C[0] == 0xFF && C[1] == 0xFE && C[2] != '\0' && C[3] == '\0') // le
                    || (C[0] == 0xFE && C[1] == 0xFF && C[2] == '\0' && C[3] != '\0') // be
                    || (C[0] != '\0' && C[1] == '\0' && C[2] != '\0' && C[3] == '\0') // le
                    || (C[0] == '\0' && C[1] != '\0' && C[2] == '\0' && C[3] != '\0')) // be
                {
                    charset_ = "UTF-16";
                }
                /*else if ((C[0] == 0xFF && C[1] == 0xFE && C[2] == '\0' && C[3] == '\0')
                          || (C[0] == '\0' && C[1] == '\0' && C[2] == 0xFE && C[3] == 0xFF))*/
                else if ((C[0] != '\0' && C[1] != '\0' && C[2] == '\0' && C[3] == '\0') // le
                         || (C[0] == '\0' && C[1] == '\0' && C[2] != '\0' && C[3] != '\0')) // be
                {
                    charset_ = "UTF-32";
                }
            }
            else if ((C[0] == 0xFF && C[1] == 0xFE) || (C[0] == 0xFE && C[1] == 0xFF))
            { // check special cases of UTF-16
                while (num < 8 && file.read (&c, charSize) > 0)
                {
                    data.append (c);
                    if (c == '\0')
                        hasNull = true;
                    ++ num;
                }
                if (hasNull && (num == 6 || num == 8))
                    charset_ = "UTF-16";
                if (num < 8)
                    readMore = false;
            }

            if (readMore)
            {
                /* reading may still be possible */
                if (charset_.isEmpty() && !hasNull)
                {
                    ++ num; // 4 or 8 characters are already read
                    while (file.read (&c, charSize) > 0)
                    {
                        if (c == '\0')
                        {
                            if (!hasNull)
                            {
                                if (skipNonText_)
                                {
                                    file.close();
                                    emit completed (QString(), QString(), "UTF-8"); // shows that a non-text file is skipped
                                    return;
                                }
                                hasNull = true;
                            }
                        }
                        else if (c == '\n' || c == '\r')
                            num = 0;
                        if (num <= 500000)
                            data.append (c);
                        else if (num == 500001)
                        {
                            data += QByteArray ("    HUGE LINE TRUNCATED: NO LINE WITH MORE THAN 500000 CHARACTERS");
                            forceUneditable_ = true;
                        }
                        ++num;
                    }
                }
                else
                { // the meaning of null characters was determined before
                    if (skipNonText_ && hasNull && charset_.isEmpty())
                    {
                        file.close();
                        emit completed (QString(), QString(), "UTF-8");
                        return;
                    }
                    num = 0;
                    while (file.read (&c, charSize) > 0)
                    {
                        if (c == '\n' || c == '\r')
                            num = 0;
                        if (num < 500004) // a multiple of 4 (for UTF-16/32)
                            data.append (c);
                        else
                            forceUneditable_ = true;
                        ++num;
                    }
                }
            }
        }
    }
    file.close();
    if (skipNonText_ && hasNull && charset_.isEmpty())
    {
        emit completed (QString(), QString(), "UTF-8");
        return;
    }

    if (charset_.isEmpty())
    {
        if (hasNull)
        {
            forceUneditable_ = true;
            charset_ = "UTF-8"; // always open non-text files as UTF-8
        }
        else
            charset_ = detectCharset (data);
    }

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    QTextCodec *codec = QTextCodec::codecForName (charset_.toUtf8()); // or charset_.toStdString().c_str()
    if (!codec) // prevent any chance of crash if there's a bug
    {
        charset_ = "UTF-8";
        codec = QTextCodec::codecForName ("UTF-8");
    }
    QString text = codec->toUnicode (data);
#else
    /* Legacy encodings aren't supported by Qt6. */
    auto decoder = QStringDecoder (charset_ == "UTF-8"  ? QStringConverter::Utf8 :
                                   charset_ == "UTF-16" ? QStringConverter::Utf16 :
                                   charset_ == "UTF-32" ? QStringConverter::Utf32 :
                                                          QStringConverter::Latin1);
    QString text = decoder.decode (data);
#endif

    emit completed (text,
                    fname_,
                    charset_,
                    enforced,
                    reload_,
                    restoreCursor_,
                    posInLine_,
                    forceUneditable_,
                    multiple_);
}

}
