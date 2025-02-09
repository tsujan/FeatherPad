/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2021 <tsujan2000@gmail.com>
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

#include <algorithm>

namespace FeatherPad {

/* we include "%", "%Q", "%q", "%W", "%w", "%x" ans "%s" here to have a simpler code */
static const QRegularExpression rubyRegex ("/|%(Q|q|W|w|x|r|s)?\\s*[^\\w\\}\\)\\]>\\s]");

// This is only for the start.
bool Highlighter::isEscapedRubyRegex (const QString &text, const int pos)
{
    if (pos < 0) return false;
    if (format (pos) == quoteFormat || format (pos) == altQuoteFormat
        || format (pos) == commentFormat || format (pos) == urlFormat)
    {
        return true;
    }
    if (text.at (pos) == '/' && isEscapedChar (text, pos))
        return true;

    QChar ch;
    int i = pos - 1;
    if (i > -1)
    {
        ch = text.at (i);
        if (ch.isLetterOrNumber() || ch == '_')
            return true;
    }

    while (i >= 0 && (text.at (i) == ' ' || text.at (i) == '\t'))
        --i;
    if (i > -1)
    {
        ch = text.at (i);
        if (ch == ')' || ch == ']' || ch == '}')
            return true;
        bool isString (false);
        if (ch.isNumber())
        { // check if this is really a number
            --i;
            while (i >= 0 && text.at (i).isNumber())
                --i;
            if (i == -1 || (!text.at (i).isLetter() && text.at (i) != '_'))
                return true;
            isString = true;
        }
        /* as with Kate */
        if ((isString || ch.isLetter() || ch == '_')
            && pos + 1 < text.length() && text.at (pos + 1).isSpace())
        {
            return true;
        }
    }

    return false;
}
/*************************/
// Takes care of open braces too.
int Highlighter::findRubyDelimiter (const QString &text, const int index,
                                    const QRegularExpression &delimExp, int &capturedLength) const
{
    int i = std::max (index, 0);
    const QString pattern = delimExp.pattern();
    if (pattern.startsWith ("\\")
        && (pattern.endsWith (")") || pattern.endsWith ("}")
            || pattern.endsWith ("]") || pattern.endsWith (">")))
    { // the end delimiter should be found
        int N = 1;
        if (index < 0)
        {
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    N = prevData->openNests();
            }
        }
        QChar endBrace = pattern.at (pattern.size() - 1);
        QChar startBrace = endBrace == ')' ? '('
                           : endBrace == '}' ? '{'
                           : endBrace == ']' ? '['
                           : '<';
        const int L = text.length();
        while (N > 0 && i < L)
        {
            QChar c = text.at (i);
            if (c == endBrace)
            {
                if (!isEscapedChar (text, i))
                    -- N;
            }
            else if (c == startBrace && !isEscapedChar (text, i))
                ++ N;
            ++ i;
        }
        if (N == 0)
        {
            if (i == 0) // doesn't happen
            {
                int res = text.indexOf (delimExp, 0);
                capturedLength = res == -1 ? 0 : 1;
                return res;
            }
            else
            {
                capturedLength = 1;
                return i - 1;
            }
        }
        else
        {
            static_cast<TextBlockData *>(currentBlock().userData())->insertNestInfo (N);
            capturedLength = 0;
            return -1;
        }
    }
    else
    {
        QRegularExpressionMatch match;
        int res = text.indexOf (delimExp, i, &match);
        capturedLength = match.capturedLength();
        return res;
    }
}
/*************************/
static inline QString getRubyEndDelimiter (const QString &brace)
{
    if (brace == "(")
        return ")";
    if (brace == "{")
        return "}";
    if (brace == "[")
        return "]";
    if (brace == "<")
        return ">";
    return brace;
}
/*************************/
bool Highlighter::isInsideRubyRegex (const QString &text, const int index)
{
    if (index < 0) return false;

    int pos = -1;

    if (format (index) == regexFormat)
        return true;
    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
    {
        pos = data->lastFormattedRegex() - 1;
        if (index <= pos) return false;
    }

    QRegularExpression exp;
    bool res = false;
    int N;

    if (pos == -1)
    {
        if (previousBlockState() != regexState)
        {
            exp = rubyRegex;
            N = 0;
        }
        else
        {
            QString delimStr;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    delimStr = prevData->labelInfo();
            }
            if (delimStr.isEmpty()) return false; // impossible

            N = 1;
            pos = -2; // to know that the search in continued from the previous line
            exp.setPattern ("\\" + getRubyEndDelimiter (delimStr));
            res = true;
        }
    }
    else // a new search from the last position
    {
        exp = rubyRegex;
        N = 0;
    }

    int nxtPos, capturedLength;
    while ((nxtPos = findRubyDelimiter (text, pos + 1, exp, capturedLength)) >= 0)
    {
        /* skip formatted comments and quotes */
        QTextCharFormat fi = format (nxtPos);
        if (N % 2 == 0)
        {
            if (fi == commentFormat || fi == urlFormat
                || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat)
            {
                pos = nxtPos; // don't add capturedLength because "/" might match later
                continue;
            }
        }

        ++N;
        if (N % 2 == 0
            ? isEscapedChar (text, nxtPos) // an escaped end delimiter
            : isEscapedRubyRegex (text, nxtPos)) // an escaped start delmiter
        {
            if (res)
            {
                pos = std::max (pos, 0);
                setFormat (pos, nxtPos - pos + capturedLength, regexFormat);
            }
            if (N % 2 != 0 && capturedLength > 1)
                pos = nxtPos; // don't add capturedLength because "/" might match later
            else
                pos = nxtPos + capturedLength - 1;
            --N;
            continue;
        }

        if (N % 2 == 0)
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedRegex (nxtPos + capturedLength);
            pos = std::max (pos, 0);
            setFormat (pos, nxtPos - pos + capturedLength, regexFormat);
        }

        if (index <= nxtPos)
        {
            if (N % 2 == 0)
            {
                res = true;
                break;
            }
            else
            {
                res = false;
                break;
            }
        }

        /* we should set "res" below because "pos" might be negative next time. */
        if (N % 2 == 0)
        {
            exp = rubyRegex;
            res = false;
        }
        else
        {
            exp.setPattern ("\\" + getRubyEndDelimiter (QString (text.at (nxtPos + capturedLength - 1))));
            res = true;
        }

        pos = nxtPos + capturedLength - 1;
    }

    return res;
}
/*************************/
void Highlighter::multiLineRubyRegex (const QString &text)
{
    int startIndex = 0;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;
    QRegularExpression endExp;
    QTextCharFormat fi;
    QString startDelimStr;

    int prevState = previousBlockState();
    if (prevState == regexState)
    {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                startDelimStr = prevData->labelInfo();
        }
        if (startDelimStr.isEmpty()) return; // impossible
    }
    else
    {
        startIndex = text.indexOf (rubyRegex, startIndex, &startMatch);
        /* skip comments and quotations (all formatted to this point) */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedRubyRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            /* search from the next position, without considering
               the captured length, because "/" might match later */
            startIndex = text.indexOf (rubyRegex, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0)
            startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
    }

    while (startIndex >= 0)
    {
        endExp.setPattern ("\\" + getRubyEndDelimiter (startDelimStr));
        int endLength;
        int endIndex = findRubyDelimiter (text,
                                          (startIndex == 0 && prevState == regexState)
                                              ? -1 // to know that the search is continued from the previous line
                                              : startIndex + startMatch.capturedLength(),
                                          endExp,
                                          endLength);

        while (endIndex > -1 && isEscapedChar (text, endIndex))
            endIndex = findRubyDelimiter (text, endIndex + 1, endExp, endLength);

        int len;
        int keywordLength = std::max (static_cast<int>(startMatch.capturedLength()) - 1, 0);
        if (endIndex == -1)
        {
            len = text.length() - startIndex;
            setCurrentBlockState (regexState);

            static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (startDelimStr);
            /* NOTE: The next block will be rehighlighted at highlightBlock()
                        (-> multiLineRegex (text, 0);) if the delimiter is changed. */
        }
        else
            len = endIndex - startIndex + endLength;
        setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);

        startIndex = text.indexOf (rubyRegex, startIndex + len, &startMatch);
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedRubyRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (rubyRegex, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0)
            startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
    }
}

}
