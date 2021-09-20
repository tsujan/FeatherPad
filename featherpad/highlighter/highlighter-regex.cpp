/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2018-2020 <tsujan2000@gmail.com>
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

#include "highlighter.h"

namespace FeatherPad {

static const QRegularExpression regexStartExp ("/");
static const QRegularExpression regexEndExp ("/[A-Za-z0-9_]*");

// This is only for the starting "/".
bool Highlighter::isEscapedRegex (const QString &text, const int pos)
{
    if (pos < 0) return false;
    if (progLan != "javascript" && progLan != "qml")
        return false;

    if (format (pos) == quoteFormat || format (pos) == altQuoteFormat
        || format (pos) == commentFormat || format (pos) == urlFormat)
    {
        return true;
    }

    if (isMLCommented (text, pos, commentState)
        || isMLCommented (text, pos, htmlJavaCommentState))
    {
        return true;
    }

    /* escape "<.../>", "</...>", the single-line comment sign
       and the start multiline comment sign
       FIXME: In this way and with what follows, "/>/g" isn't highlighted. */
    if ((text.length() > pos + 1 && ((progLan == "javascript" && text.at (pos + 1) == '>')
                                     || text.at (pos + 1) == '/' || text.at (pos + 1) == '*'))
        || (pos > 0 && progLan == "javascript" && text.at (pos - 1) == '<'))
    {
        return true;
    }

    QRegularExpressionMatch keyMatch;
    static QRegularExpression jsKeys, qmlKeys;

    int i = pos - 1;
    while (i >= 0 && (text.at (i) == ' ' || text.at (i) == '\t'))
        --i;
    if (i == -1) // examine the previous line(s)
    {
        /* NOTE: regexExtraState is already applied to:
                 1) Single-line comments;
                 2) Lines ending with single/double quotations
                    but without end quote; and
                 3) Lines ending with regex plus spaces. */
        QTextBlock prev = currentBlock().previous();
        if (!prev.isValid()) return false;
        QString txt = prev.text();
        QRegularExpression nonSpace ("[^\\s]+");
        while (txt.indexOf (nonSpace, 0) == -1)
        {
            if (prev.userState() == regexExtraState)
            { // a quoted line with only witespaces (backslashed mutil-line quote)
                return false;
            }
            prev.setUserState (updateState); // update the next line if this one changes
            prev = prev.previous();
            if (!prev.isValid()) return false;
            txt = prev.text();
        }
        int last = txt.length() - 1;
        QChar ch = txt.at (last);
        while (ch == ' ' || ch == '\t')
        {
            --last;
            ch = txt.at (last);
        }
        if (prev.userState() == regexExtraState)
        {
            /* a regex isn't escaped if it follows another one,
               a single-line comment or a quotation (without an end quote) */
            return false;
        }
        else
        {
            prev.setUserState (updateState); // update the next line if this one changes
            if (ch.isNumber() || ch == '_'
                /* as with Kate */
                || ch == ')' || ch == ']' || ch == '$' || ch == '\"' || ch == '\'' || ch == '`'
                /* also skip "/>" */
                || (last > 0 && ch == '>' && txt.at (last - 1) == '/' && progLan == "javascript"))
            {
                return true;
            }
            if (ch.isLetter())
            { // a regex isn't escaped if it follows a JavaScript keyword
                if (progLan == "javascript")
                {
                    if (jsKeys.pattern().isEmpty())
                        jsKeys.setPattern (keywords (progLan).join ('|'));
                }
                else
                {
                    if (qmlKeys.pattern().isEmpty())
                        qmlKeys.setPattern (keywords (progLan).join ('|'));
                }
                int len = qMin (12, last + 1);
                QString str = txt.mid (last - len + 1, len);
                int j;
                if ((j = str.lastIndexOf (progLan == "javascript" ? jsKeys : qmlKeys, -1, &keyMatch)) > -1
                    && j + keyMatch.capturedLength() == len)
                {
                    return false;
                }
                return true;
            }
        }
    }
    else
    { // a regex isn't escaped if it follows another one or a JavaScript keyword
        if (format (i) == regexFormat)
            return false;
        QChar ch = text.at (i);
        if (/* as with Kate */
            ch == ')' || ch == ']' || ch == '$' || ch == '\"' || ch == '\'' || ch == '`'
            /* also skip "/>" */
            || (i > 0 && ch == '>' && text.at (i - 1) == '/' && progLan == "javascript"))
        {
            return true;
        }
        if (ch.isLetterOrNumber() || ch == '_')
        {
            int j;
            if ((j = text.lastIndexOf (QRegularExpression("/\\w+"), i + 1, &keyMatch)) > -1
                && j + keyMatch.capturedLength() == i + 1 && format (j) == regexFormat)
            {
                return false;
            }
            if (ch.isLetter())
            {
                if (progLan == "javascript")
                {
                    if (jsKeys.pattern().isEmpty())
                        jsKeys.setPattern (keywords (progLan).join ('|'));
                }
                else
                {
                    if (qmlKeys.pattern().isEmpty())
                        qmlKeys.setPattern (keywords (progLan).join ('|'));
                }
                int len = qMin (12, i + 1);
                QString str = text.mid (i - len + 1, len);
                if ((j = str.lastIndexOf (progLan == "javascript" ? jsKeys : qmlKeys, -1, &keyMatch)) > -1
                    && j + keyMatch.capturedLength() == len)
                {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}
/*************************/
// May be used for middle signs (e.g., "/") too. FIXME: Multi-line classes aren't supported.
bool Highlighter::isEscapedRegexEndSign (const QString &text, const int start, const int pos,
                                         bool ignoreClasses) const
{
    if (pos < 1) return false;
    if (isEscapedChar (text, pos))
        return true;
    if (!ignoreClasses)
    {
        /* check if it's inside a class */
        int i = pos - 1;
        while (i >= start)
        {
            if (text.at (i) == ']' && !isEscapedChar (text, i))
                return false;
            if (text.at (i) == '[' && !isEscapedChar (text, i))
                return true;
            --i;
        }
    }
    return false;
}
/*************************/
// For faster processing with very long lines, this function also highlights regex patterns.
// (It should be used with care because it gives correct results only in special places.)
bool Highlighter::isInsideRegex (const QString &text, const int index)
{
    if (progLan == "perl") return isInsidePerlRegex (text, index);
    if (progLan == "ruby") return isInsideRubyRegex (text, index);

    if (index < 0) return false;
    if (progLan != "javascript" && progLan != "qml")
        return false;

    int pos = -1;

    if (format (index) == regexFormat)
        return true;
    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
    {
        pos = data->lastFormattedRegex() - 1;
        if (index <= pos) return false;
    }

    QRegularExpressionMatch match;
    QRegularExpression exp;
    bool res = false;
    int N;

    if (pos == -1)
    {
        if (previousBlockState() != regexState)
        {
            exp = regexStartExp;
            N = 0;
        }
        else
        {
            exp = regexEndExp;
            N = 1;
            res = true;
        }
    }
    else // a new search from the last position
    {
        exp = regexStartExp;
        N = 0;
    }

    int nxtPos;
    while ((nxtPos = text.indexOf (exp, pos + 1, &match)) >= 0)
    {
        /* skip formatted comments and quotes */
        QTextCharFormat fi = format (nxtPos);
        if (N % 2 == 0
            && (fi == commentFormat || fi == urlFormat
                || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0 && isEscapedRegexEndSign (text, pos + 1, nxtPos)) // an escaped end sign
            || (N % 2 != 0 && isEscapedRegex (text, nxtPos))) // or an escaped start sign
        {
            if (res)
            {
                pos = qMax (pos, 0);
                setFormat (pos, nxtPos - pos + match.capturedLength(), regexFormat);
            }
            --N;
            pos = nxtPos;
            continue;
        }

        if (N % 2 == 0)
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedRegex (nxtPos + match.capturedLength());
            pos = qMax (pos, 0);
            setFormat (pos, nxtPos - pos + match.capturedLength(), regexFormat);
        }

        if (index <= nxtPos) // they may be equal, as in "/...//" or "/.../*"
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        if (N % 2 == 0)
        {
            exp = regexStartExp;
            res = false;
        }
        else
        {
            exp = regexEndExp;
            res = true;
        }

        pos = nxtPos;
    }

    return res;
}
/*************************/
void Highlighter::multiLineRegex(const QString &text, const int index)
{
    if (progLan == "perl")
    {
        multiLinePerlRegex (text);
        return;
    }
    if (progLan == "ruby")
    {
        multiLineRubyRegex (text);
        return;
    }

    if (index < 0) return;
    if (progLan != "javascript" && progLan != "qml")
        return;

    int prevState = previousBlockState();

    if (index == 0 && prevState == -1 && text.startsWith ("#!"))
    { // a special case at the start of the first line (hash-bang)
        setFormat (0, text.length(), commentFormat);
        return;
    }

    int startIndex = index;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;
    QTextCharFormat fi;

    if (prevState != regexState || startIndex > 0)
    {
        startIndex = text.indexOf (regexStartExp, startIndex, &startMatch);
        /* skip comments and quotations (all formatted to this point) */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (regexStartExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
    }

    while (startIndex >= 0)
    {
        int endIndex, indx;
        /* when the start sign is in the prvious line
           and the search for the end sign has just begun,
           search for the end sign from the line start */
        if (prevState == regexState && startIndex == 0)
        {
            indx = 0;
            endIndex = text.indexOf (regexEndExp, 0, &endMatch);
        }
        else
        {
            indx = startIndex + startMatch.capturedLength();
            endIndex = text.indexOf (regexEndExp, indx, &endMatch);
        }

        while (isEscapedRegexEndSign (text, indx, endIndex))
            endIndex = text.indexOf (regexEndExp, endIndex + 1, &endMatch);

        int len;
        if (endIndex == -1)
        {
            setCurrentBlockState (regexState);
            len = text.length() - startIndex;
        }
        else
        {
            len = endIndex - startIndex
                  + endMatch.capturedLength();
        }
        setFormat (startIndex, len, regexFormat);

        startIndex = text.indexOf (regexStartExp, startIndex + len, &startMatch);

        /* skip comments and quotations again */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (regexStartExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
    }

    /* If this line ends with a regex plus spaces, give it a special
       state to decide about the probable regex of its following line
       and also for that line to be updated when this state is toggled.
       See isEscapedRegex(). */
    if (currentBlockState() == 0 && !text.isEmpty())
    {
        int last = text.length() - 1;
        QChar ch = text.at (last);
        while (ch == ' ' || ch == '\t')
        {
            --last;
            if (last < 0) return;
            ch = text.at (last);
        }
        if (format (last) == regexFormat)
            setCurrentBlockState (regexExtraState);
    }
}

}
