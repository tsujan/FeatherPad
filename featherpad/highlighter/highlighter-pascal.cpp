/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2021-2024 <tsujan2000@gmail.com>
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

static const QRegularExpression pascalCommentStartExp ("\\(\\*|\\{");

bool Highlighter::isPascalQuoted (const QString &text, const int index,
                                  const int start) const
{
    if (index < 0) return false;
    int N = 0;
    int indx = start;
    while ((indx = text.indexOf (quoteMark, indx)) > -1 && indx < index)
    {
        if (format (indx) != commentFormat && format (indx) != urlFormat
            && format (indx) != regexFormat)
        {
            ++ N;
        }
        ++ indx;
    }
    return N % 2 != 0;
}
/*************************/
bool Highlighter::isPascalMLCommented (const QString &text, const int index,
                                       const int start) const
{
    if (index < 0 || start < 0 || index < start)
        return false;

    int prevState = previousBlockState();
    bool res = false;
    int pos = start - 1;
    int N;
    QRegularExpressionMatch commentMatch;
    QRegularExpression commentExpression;
    if (pos >= 0 || prevState != commentState)
    {
        N = 0;
        commentExpression = pascalCommentStartExp;
    }
    else
    {
        N = 1;
        res = true;
        bool oldComment = false;
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData());
            oldComment = prevData && prevData->getProperty();
        }
        if (oldComment)
            commentExpression.setPattern ("\\*\\)");
        else
            commentExpression.setPattern ("\\}");
    }

    while ((pos = text.indexOf (commentExpression, pos + 1, &commentMatch)) >= 0)
    {
        /* skip formatted quotations */
        if (format (pos) == quoteFormat) continue;

        ++N;

        if (index < pos + (N % 2 == 0 ? commentMatch.capturedLength() : 0))
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        if (N % 2 != 0)
        {
            if (text.at (pos) == '(')
                commentExpression.setPattern ("\\*\\)");
            else
                commentExpression.setPattern ("\\}");
            res = true;
        }
        else
        {
            commentExpression = pascalCommentStartExp;
            res = false;
        }
    }

    return res;

}
/*************************/
void Highlighter::singleLinePascalComment (const QString &text, const int start)
{
    QRegularExpression commentExp ("//.*");
    int startIndex = std::max (start, 0);
    startIndex = text.indexOf (commentExp, startIndex);
    /* skip quoted comments */
    while (format (startIndex) == quoteFormat // only for multiLinePascalComment()
           || isPascalQuoted (text, startIndex, std::max (start, 0)))
    {
        startIndex = text.indexOf (commentExp, startIndex + 1);
    }
    if (startIndex > -1)
    {
        int l = text.length();
        setFormat (startIndex, l - startIndex, commentFormat);

        /* also format urls and email addresses inside the comment */
        QString str = text.sliced (startIndex, l - startIndex);
        int pIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
        {
            setFormat (pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
            pIndex += urlMatch.capturedLength();
        }
        /* format note patterns too */
        pIndex = 0;
        while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
        {
            if (format (pIndex + startIndex) != urlFormat)
                setFormat (pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
            pIndex += urlMatch.capturedLength();
        }
    }
}
/*************************/
void Highlighter::pascalQuote (const QString &text, const int start)
{
    int index = start;
    index = text.indexOf (quoteMark, index);
    /* skip escaped start quotes and all comments */
    while (isPascalMLCommented (text, index))
        index = text.indexOf (quoteMark, index + 1);
    while (format (index) == commentFormat || format (index) == urlFormat) // single-line
        index = text.indexOf (quoteMark, index + 1);

    while (index >= 0)
    {
        QRegularExpressionMatch quoteMatch;
        int endIndex = text.indexOf (quoteMark, index + 1, &quoteMatch);
        if (endIndex == -1)
        {
            setFormat (index, text.length() - index, quoteFormat);
            return;
        }

        int quoteLength = endIndex - index
                          + quoteMatch.capturedLength();
        setFormat (index, quoteLength, quoteFormat);

        index = text.indexOf (quoteMark, index + quoteLength);
        while (isPascalMLCommented (text, index, endIndex + 1))
            index = text.indexOf (quoteMark, index + 1);
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteMark, index + 1);
    }
}
/*************************/
void Highlighter::multiLinePascalComment (const QString &text)
{
    int prevState = previousBlockState();
    int startIndex = 0;
    bool oldComment = false;

    QRegularExpression commentEndExp;
    QRegularExpressionMatch startMatch;

    if (prevState == commentState)
    {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData());
            oldComment = prevData && prevData->getProperty();
        }
    }
    else
    {
        startIndex = text.indexOf (pascalCommentStartExp, startIndex, &startMatch);
        /* skip quotations (all formatted to this point) */
        while (format (startIndex) == quoteFormat)
            startIndex = text.indexOf (pascalCommentStartExp, startIndex + 1, &startMatch);
        /* skip single-line comments */
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            return;
        oldComment = startIndex >= 0 && text.at (startIndex) == '(';
    }

    while (startIndex >= 0)
    {
        int endIndex;
        QRegularExpressionMatch endMatch;
        if (oldComment)
            commentEndExp.setPattern ("\\*\\)");
        else
            commentEndExp.setPattern ("\\}");

        if (prevState == commentState && startIndex == 0)
            endIndex = text.indexOf (commentEndExp, 0, &endMatch);
        else
        {
            endIndex = text.indexOf (commentEndExp,
                                     startIndex + startMatch.capturedLength(),
                                     &endMatch);
        }

        /* if there's a comment end ... */
        if (endIndex >= 0)
        {
            /* ... clear the comment format from there to reformat
               because a single-line comment may have changed now */
            int badIndex = endIndex + endMatch.capturedLength();
            bool hadSingleLineComment = false;
            int i = 0;
            for (i = badIndex; i < text.length(); ++i)
            {
                if (format (i) == commentFormat || format (i) == urlFormat)
                {
                    setFormat (i, text.length() - i, mainFormat);
                    hadSingleLineComment = true;
                    break;
                }
            }
            singleLinePascalComment (text, badIndex);
            if (hadSingleLineComment)
                pascalQuote (text, i);
        }

        bool compilerDirective (!(startIndex == 0 && prevState == commentState)
                                && text.length() > (startIndex + (oldComment ? 2 : 1))
                                && text.at (startIndex + (oldComment ? 2 : 1)) == '$');
        int commentLength;
        if (endIndex == -1)
        {
            commentLength = text.length() - startIndex;
            if (!compilerDirective)
            {
                setCurrentBlockState (commentState);
                if (oldComment)
                {
                    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                        data->setProperty (true);
                }
            }
        }
        else
            commentLength = endIndex - startIndex
                            + endMatch.capturedLength();

        setFormat (startIndex, commentLength, compilerDirective ? regexFormat
                                                                : commentFormat);

        if (!compilerDirective)
        {
            /* format urls and email addresses inside the comment */
            QString str = text.sliced (startIndex, commentLength);
            int pIndex = 0;
            QRegularExpressionMatch urlMatch;
            while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
            {
                setFormat (pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
                pIndex += urlMatch.capturedLength();
            }
            /* format note patterns too */
            pIndex = 0;
            while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
            {
                if (format (pIndex + startIndex) != urlFormat)
                    setFormat (pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
                pIndex += urlMatch.capturedLength();
            }
        }

        startIndex = text.indexOf (pascalCommentStartExp, startIndex + commentLength, &startMatch);
        while (format (startIndex) == quoteFormat)
            startIndex = text.indexOf (pascalCommentStartExp, startIndex + 1, &startMatch);
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            return;
        oldComment = startIndex >= 0 && text.at (startIndex) == '(';
    }
}

}
