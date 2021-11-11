/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
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

// multi/single-line quote highlighting for bash.
void Highlighter::SH_MultiLineQuote (const QString &text)
{
    int index = 0;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteMark;
    int initialState = currentBlockState();
    int prevState = previousBlockState();

    bool wasDQuoted (prevState == doubleQuoteState
                     || prevState == SH_MixedDoubleQuoteState || prevState == SH_MixedSingleQuoteState);
    bool wasQuoted (wasDQuoted || prevState == singleQuoteState);
    QTextBlock prevBlock = currentBlock().previous();
    TextBlockData *prevData = nullptr;
    if (prevBlock.isValid())
    {
        prevData = static_cast<TextBlockData *>(prevBlock.userData());
        if (prevData && prevData->getProperty())
        { // at the end delimiter of a here-doc starting like VAR="$(cat<<EOF
            wasQuoted = wasDQuoted = true;
        }
    }

    TextBlockData *curData = static_cast<TextBlockData *>(currentBlock().userData());
    int hereDocDelimPos = -1;
    if (!curData->labelInfo().isEmpty()) // the label is delimStr
    {
        hereDocDelimPos = text.indexOf (hereDocDelimiter);
        while (hereDocDelimPos > -1 && isQuoted (text, hereDocDelimPos, true))
        { // see isHereDocument()
            hereDocDelimPos = text.indexOf (hereDocDelimiter, hereDocDelimPos + 2);
        }
    }

    /* find the start quote */
    if (!wasQuoted)
    {
        index = text.indexOf (quoteExpression);
        /* skip escaped start quotes and all comments */
        while (SH_SkipQuote (text, index, true))
            index = text.indexOf (quoteExpression, index + 1);

        /* check if the first quote is after the here-doc start delimiter */
        if (index >= 0 && hereDocDelimPos > -1 && index > hereDocDelimPos)
            index = -1;

        /* if the start quote is found... */
        if (index >= 0)
        {
            /* ... distinguish between double and single quotes */
            if (text.at (index) == quoteMark.pattern().at (0))
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }
    }
    else // but if we're inside a quotation
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        if (wasDQuoted)
            quoteExpression = quoteMark;
        else
            quoteExpression = singleQuoteMark;
    }

    while (index >= 0)
    {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteMark)
        {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed */
            if (text.at (index) == quoteMark.pattern().at (0))
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0 && wasQuoted)
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteExpression, 0, &quoteMatch);
        }
        else // otherwise, search for the end quote from the start quote
            endIndex = text.indexOf (quoteExpression, index + 1, &quoteMatch);

        /* check if the end quote is escaped */
        while (SH_SkipQuote (text, endIndex, false))
            endIndex = text.indexOf (quoteExpression, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1)
        {
            if (quoteExpression != quoteMark || hereDocDelimPos == -1)
            {
                setCurrentBlockState (quoteExpression == quoteMark
                                                         ? initialState == SH_DoubleQuoteState
                                                           ? SH_MixedDoubleQuoteState
                                                           : initialState == SH_SingleQuoteState
                                                             ? SH_MixedSingleQuoteState
                                                             : doubleQuoteState
                                                         : singleQuoteState);
            }
            else if (curData->openNests() > 0) // like VAR="$(cat<<EOF
                curData->setProperty (true); // initialState is always that of here-doc
            quoteLength = text.length() - index;
        }
        else
        {
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength(); // 1
        }
        if (quoteExpression == quoteMark)
            setFormatWithoutOverwrite (index, quoteLength, quoteFormat, neutralFormat);
        else
            setFormat (index, quoteLength, altQuoteFormat);

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        QString str = text.mid (index, quoteLength);
#else
        QString str = text.sliced (index, quoteLength);
#endif
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
        {
             setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
             urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteMark;
        index = text.indexOf (quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (SH_SkipQuote (text, index, true))
            index = text.indexOf (quoteExpression, index + 1);

        /* check if the first quote is after the here-doc start delimiter */
        if (hereDocDelimPos > -1 && index > hereDocDelimPos)
            index = -1;
    }
}
/*************************/
// The bash quotes that should be skipped while multiline quotes are being highlighted.
bool Highlighter::SH_SkipQuote (const QString &text, const int pos, bool isStartQuote)
{
    if (isEscapedQuote (text, pos, isStartQuote))
        return true;
    QTextCharFormat fi = format (pos);
    return (fi == neutralFormat // not needed
            || fi == commentFormat
            || fi == urlFormat
            || fi == quoteFormat
            || fi == altQuoteFormat
            || fi == urlInsideQuoteFormat);
}
/*************************/
// Formats the text inside a command substitution variable character by character,
// considering single and double quotes, code block start and end, and comments.
int Highlighter::formatInsideCommand (const QString &text,
                                      const int minOpenNests, int &nests, QSet<int> &quotes,
                                      const bool isHereDocStart, const int index)
{
    int p = 0;
    int indx = index;
    bool doubleQuoted (quotes.contains (nests));
    bool comment (false);
    int initialOpenNests = nests;
    while (nests > minOpenNests && indx < text.length())
    {
        while (format (indx) == commentFormat)
            ++ indx;
        if (indx == text.length())
            break;
        const QChar c = text.at (indx);
        if (c == '\'')
        {
            if (comment)
            {
                setFormat (indx, 1, commentFormat);
                ++ indx;
            }
            else
            {
                if (doubleQuoted)
                {
                    setFormat (indx, 1, quoteFormat);
                    ++ indx;
                }
                else if (isEscapedQuote (text, indx, true))
                    ++ indx;
                else
                {
                    int end = text.indexOf (singleQuoteMark, indx + 1);
                    while (isEscapedQuote (text, end, false))
                        end = text.indexOf (singleQuoteMark, end + 1);
                    if (end == -1)
                    {
                        setFormat (indx, text.length() - indx, altQuoteFormat);
                        if (!isHereDocStart)
                            setCurrentBlockState (SH_SingleQuoteState);
                        indx = text.length();
                    }
                    else
                    {
                        setFormat (indx, end + 1 - indx, altQuoteFormat);
                        indx = end + 1;
                    }
                }
            }
        }
        else if (c == '\"')
        {
            if (comment)
                setFormat (indx, 1, commentFormat);
            else if (!isEscapedQuote (text, indx, true))
            {
                doubleQuoted = !doubleQuoted;
                setFormat (indx, 1, quoteFormat);
            }
            ++ indx;
        }
        else if (c == '$') // may start a new code block
        {
            if (comment)
            {
                setFormat (indx, 1, commentFormat);
                ++ indx;
            }
            else
            {
                if (text.mid (indx, 2) == "$(")
                {
                    setFormat (indx, 2, neutralFormat);

                    int Nests = nests + 1;
                    indx = formatInsideCommand (text,
                                                nests, Nests, quotes,
                                                isHereDocStart, indx + 2);
                    nests = Nests;
                }
                else
                {
                    if (doubleQuoted)
                        setFormat (indx, 1, quoteFormat);
                    else
                        setFormat (indx, 1, neutralFormat);
                    ++ indx;
                }
            }
        }
        else if (c == '(')
        {
            if (doubleQuoted)
                setFormat (indx, 1, quoteFormat);
            else
            {
                if (comment)
                    setFormat (indx, 1, commentFormat);
                else
                    setFormat (indx, 1, neutralFormat);
                if (!isEscapedChar (text, indx))
                    ++ p;
            }
            ++ indx;
        }
        else if (c == ')') // may end a code block
        {
            if (doubleQuoted)
                setFormat (indx, 1, quoteFormat);
            else
            {
                if (!isEscapedChar (text, indx))
                {
                    -- p;
                    if (p < 0)
                    {
                        setFormat (indx, 1, neutralFormat); // never commented
                        quotes.remove (initialOpenNests);
                        -- nests;
                        initialOpenNests = nests;
                        doubleQuoted = quotes.contains (nests);
                        p = 0;
                    }
                    else if (comment)
                        setFormat (indx, 1, commentFormat);
                    else
                        setFormat (indx, 1, neutralFormat);
                }
                else if (comment)
                    setFormat (indx, 1, commentFormat);
                else
                    setFormat (indx, 1, neutralFormat);
            }
            ++ indx;
        }
        else if (c == '#') // may be comment sign
        {
            if (comment)
                setFormat (indx, 1, commentFormat);
            else if (doubleQuoted)
                setFormat (indx, 1, quoteFormat);
            else
            {
                if (indx == 0)
                {
                    comment = true;
                    setFormat (indx, 1, commentFormat);
                }
                else
                {
                    if (text.at (indx - 1) == QChar (QChar::Space))
                    {
                        comment = true;
                        setFormat (indx, 1, commentFormat);
                    }
                    else
                        setFormat (indx, 1, neutralFormat);
                }
            }
            ++ indx;
        }
        else // any non-special character
        {
            if (comment)
                setFormat (indx, 1, commentFormat);
            else if (doubleQuoted)
                setFormat (indx, 1, quoteFormat);
            else
                setFormat (indx, 1, neutralFormat);
            ++ indx;
        }
    }

    if (nests < minOpenNests) nests = minOpenNests; // impossible
    /* save the states */
    if (doubleQuoted)
    {
        if (!isHereDocStart
            /* an open subcommand may have already set the state */
            && currentBlockState() != SH_SingleQuoteState)
        { // FIXME: This state is redundant. remove it later!
            setCurrentBlockState (SH_DoubleQuoteState);
        }
        quotes.insert (initialOpenNests);
    }
    else
        quotes.remove (initialOpenNests);
    return indx;
}
/*************************/
// This function highlights command substitution variables $(...).
bool Highlighter::SH_CmndSubstVar (const QString &text,
                                   TextBlockData *currentBlockData,
                                   int oldOpenNests, const QSet<int> &oldOpenQuotes)
{
    if (progLan != "sh" || !currentBlockData) return false;

    int prevState = previousBlockState();
    int curState = currentBlockState();
    bool isHereDocStart = (curState < -1 || curState >= endState);

    int N = 0;
    QSet<int> Q;
    QTextBlock prevBlock = currentBlock().previous();
    /* get the data about open nests and their (double) quotes */
    if (prevBlock.isValid())
    {
       if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
       {
            N = prevData->openNests();
            Q = prevData->openQuotes();
       }
    }

    int indx = 0; int end = 0;

    if (N > 0 && (prevState == SH_SingleQuoteState
                  || prevState == SH_DoubleQuoteState
                  || prevState == SH_MixedDoubleQuoteState
                  || prevState == SH_MixedSingleQuoteState))
    { // there was an unclosed quote in the previous block
        if (prevState == SH_SingleQuoteState
            || prevState == SH_MixedSingleQuoteState)
        {
            end = text.indexOf (singleQuoteMark);
            while (isEscapedQuote (text, end, false))
                end = text.indexOf (singleQuoteMark, end + 1);
            if (end == -1)
            {
                setFormat (0, text.length(), altQuoteFormat);
                if (!isHereDocStart)
                    setCurrentBlockState (SH_SingleQuoteState);
                goto FINISH;
            }
            else
            {
                setFormat (0, end + 1, altQuoteFormat);
                indx = end + 1;
            }
        }
        // else there's an open double quote, about which we know
    }
    else if (N == 0
             || prevState < SH_DoubleQuoteState
             || prevState > SH_MixedSingleQuoteState)
    {
        if (N > 0) // there was an unclosed code block
            indx = 0;
        // else search for "$(" below
    }

    while (indx < text.length())
    {
        if (N == 0)
        { // search for the first code block (after the previous one is closed)
            static const QRegularExpression codeBlockStart ("\\$\\(");
            int start = text.indexOf (codeBlockStart, indx);
            if (start == -1 || format (start) == commentFormat)
                goto FINISH;
            else
            { // a new code block
                ++ N;
                indx = start + 2;
                setFormat (start, 2, neutralFormat);
            }
        }
        indx = formatInsideCommand (text,
                                    0, N, Q,
                                    isHereDocStart, indx);
    }

FINISH:
    if (!Q.isEmpty())
        currentBlockData->insertOpenQuotes (Q);
    if (N > 0)
    {
        currentBlockData->insertNestInfo (N);
        if (isHereDocStart)
        {
            /* change the state to reflect the number of open code blocks
               (the open quotes info is considered at highlightBlock())*/
            curState > 0 ? curState += 2*(N + 3) : curState -= 2*(N + 3); // see isHereDocument()
            setCurrentBlockState (curState);
        }
    }
    if (N != oldOpenNests || Q != oldOpenQuotes)
    { // forced highlighting of the next block is necessary
        return true;
    }
    else return false;
}

}
