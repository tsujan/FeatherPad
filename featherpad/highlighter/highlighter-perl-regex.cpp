/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019-2020 <tsujan2000@gmail.com>
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

/* NOTE: We deal with four kinds of regex structures:

         1. Simple, single structures, like /.../ or q|...|;
         2. Single structures with braces, like {...} or q{...};
         3. Simple replacing operators like s/.../.../; and
         4. Replacing with braces, like s{...}...{...}

         We cheat and include "q", "qq", "qw", "qx" and "qr" here to have a simpler code.
         Moreover, the => operator is excluded. */
static const QRegularExpression rExp ("/|\\b(?<!(@|#|%|\\$))(m|qr|q|qq|qw|qx|qr|(?<!-)s|y|tr)\\s*(?!\\=>)[^\\w\\}\\)\\]>\\s]");
static const QRegularExpression delimiterExp ("[^\\w\\}\\)\\]>\\s]");
/* "e", "o" and "r" are substitution-specific modifiers. */
static const QString flags ("acdegilmnoprsux"); // previously "sgimx"

// This is only for the start.
bool Highlighter::isEscapedPerlRegex (const QString &text, const int pos)
{
    if (pos < 0) return false;

    if (format (pos) == quoteFormat || format (pos) == altQuoteFormat
        || format (pos) == commentFormat || format (pos) == urlFormat)
    {
        return true;
    }

    static QRegularExpression perlKeys;

    int i = pos - 1;
    if (i < 0) return false;

    if (text.at (pos) != '/')
    {
        if (format (i) == regexFormat)
            return true;
        return false;
    }

    /* FIXME: Why? */
    int slashes = 0;
    while (i >= 0 && format (i) != regexFormat && text.at (i) == '/')
    {
        -- i;
        ++ slashes;
    }
    if (slashes % 2 != 0)
        return true;

    i = pos - 1;
    while (i >= 0 && (text.at (i) == ' ' || text.at (i) == '\t'))
        --i;
    if (i >= 0)
    {
        QChar ch = text.at (i);
        if (format (i) != regexFormat && (ch.isLetterOrNumber() || ch == '_'
                                          || ch == ')' || ch == ']' || ch == '}' || ch == '#'
                                          || (i == pos - 1 && (ch == '$' || ch == '@'))
                                          || (i >= 1
                                              && (text.at (i - 1) == '$'
                                                  || (text.at (i - 1) == '@' && (ch == '+' || ch == '-'))
                                                  || (text.at (i - 1) == '%' && (ch == '+' || ch == '-' || ch == '!'))))
                                          /* after an escaped start quote */
                                          || (i > 0 && (ch == '\"' || ch == '\'' || ch == '`')
                                              && format (i) != quoteFormat && format (i) != altQuoteFormat)))
        {
            /* a regex isn't escaped if it follows a Perl keyword */
            if (perlKeys.pattern().isEmpty())
                perlKeys.setPattern (keywords (progLan).join ('|'));
            int len = std::min (12, i + 1);
            QString str = text.mid (i - len + 1, len);
            int j;
            QRegularExpressionMatch keyMatch;
            if ((j = str.lastIndexOf (perlKeys, -1, &keyMatch)) > -1 && j + keyMatch.capturedLength() == len)
                return false;
            /* check the flags too */
            if (ch.isLetter())
            {
                while (i > 0 && flags.contains (text.at (i)))
                    -- i;
                if (format (i) == regexFormat)
                    return false;
            }
            return true;
        }
    }

    return false;
}
/*************************/
// Takes care of open braces too.
int Highlighter::findDelimiter (const QString &text, const int index,
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
static inline QString getEndDelimiter (const QString &brace)
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
static inline QString startBrace (QString &brace)
{
    if (brace == ")")
        return  "(";
    if (brace == "}")
        return "{";
    if (brace == "]")
        return "[";
    if (brace == ">")
        return "<";
    return QString();
}
/*************************/
bool Highlighter::isInsidePerlRegex (const QString &text, const int index)
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
    bool searchedToReplace = false; // inside the first part of a replacement operator?
    bool replacing = false; // inside the second part of a replacement operator?
    bool isQuotingOperator = false;
    int N;

    if (pos == -1)
    {
        if (previousBlockState() != regexState
            && previousBlockState() != regexExtraState
            && previousBlockState() != regexSearchState)
        {
            exp = rExp;
            N = 0;
        }
        else
        {
            QString delimStr;
            bool between = false; // between search and replacement, like s(...)HERE(...)
            bool ro = false; // a replacement operator?
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                {
                    delimStr = prevData->labelInfo();
                    if (delimStr.startsWith("r"))
                    {
                        ro = true;
                        delimStr.remove (0, 1);
                    }
                    else if (delimStr == "b") // the real delimStr is found below
                    {
                        between = true;
                        ro = true;
                    }
                }
            }
            if (delimStr.size() != 1) return false; // impossible
            if (previousBlockState() == regexState
                || previousBlockState() == regexExtraState)
            {
                N = 1;

                if (previousBlockState() == regexState)
                {
                    if (between)
                    {
                        /* find the start of the replacement part */
                        pos = text.indexOf (delimiterExp, 0);
                        if (pos > -1)
                            setFormat (0, pos + 1, regexFormat);
                        else
                        {
                            setFormat (0, text.length(), regexFormat);
                            return true;
                        }
                        replacing = true;
                        delimStr = getEndDelimiter (QString (text.at (pos)));
                    }
                    else
                    {
                        pos = -2; // to know that the search in continued from the previous line
                        if (ro) replacing = true;
                    }
                }
                else
                {
                    pos = -2;
                    isQuotingOperator = true;
                }
            }
            else// if (previousBlockState() == regexSearchState) // for "s" and "tr"
            {
                N = 0;
                searchedToReplace = true;
            }
            exp.setPattern ("\\" + getEndDelimiter (delimStr));
            res = true;
        }
    }
    else // a new search from the last position
    {
        exp = rExp;
        N = 0;
    }

    int startPos = -1;
    int nxtPos, capturedLength;
    while ((nxtPos = findDelimiter (text, pos + 1, exp, capturedLength)) >= 0)
    {
        /* skip formatted comments and quotes */
        QTextCharFormat fi = format (nxtPos);
        if (N % 2 == 0)
        {
            isQuotingOperator = false;
            if (fi == commentFormat || fi == urlFormat
                || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat)
            {
                pos = nxtPos; // don't add capturedLength because "/" might match later
                continue;
            }
            if (!searchedToReplace)
            {
                isQuotingOperator = (text.at (nxtPos) == 'q'
                                     && (capturedLength < 2 || text.at (nxtPos + 1) != 'r'));
                startPos = nxtPos + capturedLength;
            }
        }

        ++N;
        if (N % 2 == 0
            ? isEscapedRegexEndSign (text,
                                     startPos < 0 ? std::max (0, pos + 1) : startPos,
                                     nxtPos,
                                     replacing || isQuotingOperator) // an escaped end delimiter
            : (capturedLength > 1
               ? isEscapedPerlRegex (text, nxtPos) // an escaped start sign
               : (searchedToReplace
                  ? isEscapedRegexEndSign (text,
                                           startPos < 0 ? std::max (0, pos + 1) : startPos,
                                           nxtPos) // an escaped middle delimiter
                  : isEscapedPerlRegex (text, nxtPos)))) // an escaped start slash
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

        if (N % 2 == 0 || searchedToReplace)
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedRegex (nxtPos + capturedLength);
            pos = std::max (pos, 0);
            setFormat (pos, nxtPos - pos + capturedLength, regexFormat);
        }

        if (index <= nxtPos) // they may be equal, as when "//" is at the end of "/...//"
        {
            if (N % 2 == 0)
            {
                res = true;
                break;
            }
            else if (!searchedToReplace)
            {
                res = false;
                break;
            }
            // otherwise, the formatting should continue until the replacement is done
        }

        if (searchedToReplace)
        {
            searchedToReplace = false;
            replacing = true;
        }
        else
            replacing = false;

        /* NOTE: We should set "res" below because "pos" might be negative next time. */
        if (N % 2 == 0)
        {
            exp = rExp;
            res = false;
        }
        else
        {
            res = true;
            if (capturedLength > 1)
            {
                exp.setPattern ("\\" + getEndDelimiter (QString (text.at (nxtPos + capturedLength - 1))));
                if (text.at (nxtPos) == 's' || text.at (nxtPos) == 't' || text.at (nxtPos) == 'y')
                {
                    --N;
                    searchedToReplace = true;
                }
            }
            else
            {
                QString d (text.at (nxtPos));
                if (!startBrace (d).isEmpty()) // between search and replacement
                {
                    /* find the start of the replacement part */
                    pos = text.indexOf (delimiterExp, nxtPos + 1);
                    if (pos > -1)
                    {
                        setFormat (nxtPos, pos - nxtPos + 1, regexFormat);
                        exp.setPattern ("\\" + getEndDelimiter (QString (text.at (pos))));
                        continue;
                    }
                    else
                    {
                        setFormat (nxtPos, text.length() - nxtPos, regexFormat);
                        return true;
                    }
                }
                // otherwise, the start delimiter of a simple operator has been found
            }
        }

        pos = nxtPos + capturedLength - 1;
    }

    return res;
}
/*************************/
void Highlighter::multiLinePerlRegex (const QString &text)
{
    int startIndex = 0;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;
    QRegularExpression endExp;
    QTextCharFormat fi;
    QString startDelimStr;
    bool isQuotingOperator = false;
    bool ro = false; // a replacement operator?
    bool replacing = false; // inside the second part of a replacement operator?
    bool afterHereDocDelimiter = false; // after a here-doc delimiter

    QTextCharFormat flagFormat;
    flagFormat.setFontWeight (QFont::Bold);
    flagFormat.setForeground (Magenta);

    int prevState = previousBlockState();
    if (prevState == regexState
        || prevState == regexExtraState
        || prevState == regexSearchState)
    {
        bool between = false; // between search and replacement, like s(...)HERE(...)
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
            {
                startDelimStr = prevData->labelInfo();
                if (startDelimStr.startsWith("r"))
                {
                    ro = true;
                    startDelimStr.remove (0, 1);
                }
                else if (startDelimStr == "b") // the real startDelimStr is found below
                {
                    between = true;
                    ro = true;
                }
            }
        }
        if (startDelimStr.size() != 1) return; // impossible
        if (prevState == regexState)
        {
            if (between)
            {
                /* find the start of the replacement part */
                startIndex = text.indexOf (delimiterExp, 0, &startMatch); // never escaped
                if (startIndex == -1)
                {
                    setCurrentBlockState (regexState);
                    setFormat (0, text.length(), regexFormat);
                    /* "b" distinguishes this state */
                    static_cast<TextBlockData *>(currentBlock().userData())->insertInfo ("b");
                    return;
                }
                setFormat (0, startIndex + startMatch.capturedLength(), regexFormat);
                startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
                replacing = true;
            }
            else if (ro)
                replacing = true;
        }
        else if (prevState == regexExtraState)
            isQuotingOperator = true;
    }
    else
    {
        startIndex = text.indexOf (rExp, startIndex, &startMatch);
        /* skip comments and quotations (all formatted to this point) */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedPerlRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            /* search from the next position, without considering
               the captured length, because "/" might match later */
            startIndex = text.indexOf (rExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0)
        {
            startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
            if (startMatch.capturedLength() > 1)
            {
                if (text.at (startIndex) == 'q' && text.at (startIndex + 1) != 'r')
                    isQuotingOperator = true;
                else if (text.at (startIndex) == 's' || text.at (startIndex) == 't' || text.at (startIndex) == 'y')
                    ro = true;
                /* use flagFormat for operators too */
                setFormat (startIndex, startMatch.capturedLength() - 1, flagFormat);
            }

            afterHereDocDelimiter = ((currentBlockState() >= endState || currentBlockState() < -1)
                                     && currentBlockState() % 2 == 0);
        }
    }

    while (startIndex >= 0)
    {
        /* continued from the previous line? */
        bool continued (startIndex == 0
                        && (prevState == regexState
                            || prevState == regexExtraState
                            || prevState == regexSearchState));

        endExp.setPattern ("\\" + getEndDelimiter (startDelimStr));
        int endLength;
        int endIndex = findDelimiter (text,
                                      continued
                                          ? -1 // to know that the search is continued from the previous line
                                          : startIndex + startMatch.capturedLength(),
                                      endExp,
                                      endLength);

        int indx = continued ? 0 : startIndex + startMatch.capturedLength();
        while (isEscapedRegexEndSign (text, indx, endIndex, replacing || isQuotingOperator))
            endIndex = findDelimiter (text, endIndex + 1, endExp, endLength);

        int len;
        int keywordLength = std::max (static_cast<int>(startMatch.capturedLength()) - 1, 0);
        if (endIndex == -1)
        {
            if (!afterHereDocDelimiter) // don't let an incorrect regex ruin a here-doc
            {
                if (!continued)
                {
                    if (ro && !replacing)
                        setCurrentBlockState (regexSearchState);
                    else
                        setCurrentBlockState (isQuotingOperator ? regexExtraState : regexState);
                }
                else
                    setCurrentBlockState (prevState);

                static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (ro ? "r" + startDelimStr
                                                                                        : startDelimStr);
                /* NOTE: The next block will be rehighlighted at highlightBlock()
                         (-> multiLineRegex (text, 0);) if the delimiter is changed. */
            }
            len = text.length() - startIndex;
        }
        else // endIndex is found
        {
            len = endIndex - startIndex + endLength;

            if (continued)
            {
                if (prevState == regexSearchState)
                {
                    replacing = true;
                    setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);
                    if (getEndDelimiter (startDelimStr) != startDelimStr) // regex replacement with braces
                    {
                        /* find the start of the replacement part */
                        startIndex = text.indexOf (QRegularExpression (delimiterExp), endIndex + 1, &startMatch);
                        if (startIndex == -1)
                        { // the line ends between search and replacement
                            setFormat (endIndex + 1, text.length() - endIndex - 1, regexFormat);
                            setCurrentBlockState (regexState);
                            /* the prefix "b" distinguishes this state */
                            static_cast<TextBlockData *>(currentBlock().userData())->insertInfo ("b");
                            return;
                        }
                        setFormat (endIndex + 1, startIndex + startMatch.capturedLength() - endIndex - 1, regexFormat);
                        startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
                    }
                    else
                    {
                        startMatch = QRegularExpressionMatch();
                        startIndex = endIndex + 1;
                    }
                    continue;
                }
                // otherwise, the end delimiter of a simple operator or replacement part has been found
            }
            else
            {
                if (ro && !replacing)
                {
                    replacing = true;
                    setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);
                    if (getEndDelimiter (startDelimStr) != startDelimStr) // regex replacement with braces
                    {
                        /* find the start of the replacement part */
                        startIndex = text.indexOf (QRegularExpression (delimiterExp), endIndex + 1, &startMatch);
                        if (startIndex == -1)
                        { // the line ends between search and replacement
                            setFormat (endIndex + 1, text.length() - endIndex - 1, regexFormat);
                            setCurrentBlockState (regexState);
                            /* the prefix "b" distinguishes this state */
                            static_cast<TextBlockData *>(currentBlock().userData())->insertInfo ("b");
                            return;
                        }
                        setFormat (endIndex + 1, startIndex + startMatch.capturedLength() - endIndex - 1, regexFormat);
                        startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
                    }
                    else // regex replacement with a middle delimiter
                    {
                        startMatch = QRegularExpressionMatch();
                        startIndex = endIndex + 1;
                    }
                    continue;
                }
                // otherwise, the end delimiter of a simple operator or replacement part has been found
            }
        }
        setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);

        /* format flags too */
        if (text.mid (startIndex + len).indexOf (QRegularExpression ("^[" + flags + "]+"), 0, &startMatch) == 0)
            setFormat (startIndex + len, startMatch.capturedLength(), flagFormat);

        /* start searching for a new regex (operator) */
        isQuotingOperator = false;
        ro = false;
        replacing = false;
        startIndex = text.indexOf (rExp, startIndex + len, &startMatch);
        /* skip comments and quotations again */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedPerlRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (rExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0)
        {
            startDelimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
            if (startMatch.capturedLength() > 1)
            {
                if (text.at (startIndex) == 'q' && text.at (startIndex + 1) != 'r')
                    isQuotingOperator = true;
                else if (text.at (startIndex) == 's' || text.at (startIndex) == 't' || text.at (startIndex) == 'y')
                    ro = true;
                setFormat (startIndex, startMatch.capturedLength() - 1, flagFormat);
            }
        }
    }
}

}
