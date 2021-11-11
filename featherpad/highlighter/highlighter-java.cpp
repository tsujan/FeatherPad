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

namespace FeatherPad {

bool Highlighter::isEscapedJavaQuote (const QString &text, const int pos,
                                      bool isStartQuote) const
{
    if (pos <= 0) return false;
    if (isStartQuote)
    { // the literal double quote character can be written as '"' or '\"'
        return (pos + 1 < text.length() && text.at (pos + 1) == '\''
                && (text.at (pos - 1) == '\''
                    || (pos - 2 >= 0 && text.at (pos - 1) == '\\'
                        && text.at (pos - 2) == '\'')));
    }
    int i = 0;
    while (pos - i > 0 && text.at (pos - i - 1) == '\\')
        ++i;
    return (i % 2 != 0);
}
/*************************/
bool Highlighter::isJavaSingleCommentQuoted (const QString &text, const int index,
                                             const int start) const
{
    if (index < 0 || start < 0 || index < start)
        return false;

    int pos = start - 1;

    bool res = false;
    int N;
    if (pos == -1)
    {
        if (previousBlockState() != doubleQuoteState)
            N = 0;
        else
        {
            N = 1;
            res = true;
        }
    }
    else N = 0; // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf (quoteMark, pos + 1)) >= 0)
    {
        /* skip formatted comments */
        QTextCharFormat fi = format (nxtPos);
        if (fi == commentFormat || fi == urlFormat
            || fi == commentBoldFormat || fi == regexFormat)
        {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0 // an escaped end quote...
             && isEscapedJavaQuote (text, nxtPos, false))
            || (N % 2 != 0 // ... or an escaped start quote
                && isEscapedJavaQuote (text, nxtPos, true)))
        {
            --N;
            pos = nxtPos;
            continue;
        }

        if (index < nxtPos)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0) res = false;
        else res = true;

        pos = nxtPos;
    }

    return res;
}
/*************************/
bool Highlighter::isJavaStartQuoteMLCommented (const QString &text, const int index,
                                               const int start) const
{
    if (index < 0 || start < 0 || index < start)
        return false;

    bool res = false;
    int pos = start - 1;
    int N;
    QRegularExpressionMatch commentMatch;
    QRegularExpression commentExpression;
    if (pos >= 0 || previousBlockState() != commentState)
    {
        N = 0;
        commentExpression = commentStartExpression;
    }
    else
    {
        N = 1;
        res = true;
        commentExpression = commentEndExpression;
    }

    while ((pos = text.indexOf (commentExpression, pos + 1, &commentMatch)) >= 0)
    {
        /* skip formatted quotations */
        QTextCharFormat fi = format (pos);
        if (fi == quoteFormat || fi == urlInsideQuoteFormat)
            continue;

        ++N;

        if (index < pos + (N % 2 == 0 ? commentMatch.capturedLength() : 0))
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        if (N % 2 != 0)
        {
            commentExpression = commentEndExpression;
            res = true;
        }
        else
        {
            commentExpression = commentStartExpression;
            res = false;
        }
    }

    return res;
}
/*************************/
// NOTE: Java has only double quotes.
// Single quotes are just used for characters in javaMainFormatting().
void Highlighter::JavaQuote (const QString &text, const int start)
{
    int index = start;
    QRegularExpressionMatch quoteMatch;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != doubleQuoteState || index > 0)
    {
        index = text.indexOf (quoteMark, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedJavaQuote (text, index, true)
               || isJavaStartQuoteMLCommented (text, index))
        {
            index = text.indexOf (quoteMark, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat) // single-line
            index = text.indexOf (quoteMark, index + 1);
    }

    while (index >= 0)
    {
        int endIndex;
        /* if there's no start quote ... */
        if (index == 0 && prevState == doubleQuoteState)
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteMark, 0, &quoteMatch);
        }
        else // otherwise, search from the start quote
            endIndex = text.indexOf (quoteMark, index + 1, &quoteMatch);

        /* check if the end quote is escaped */
        while (isEscapedJavaQuote (text, endIndex, false))
            endIndex = text.indexOf (quoteMark, endIndex + 1, &quoteMatch);

        /* multiline quotes need backslash */
        if (endIndex == -1 && !textEndsWithBackSlash (text))
            endIndex = text.length();

        int quoteLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (doubleQuoteState);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength(); // 1 or 0 (open quotation without ending backslash)
        setFormat (index, quoteLength, quoteFormat);

        /* also format urls inside the quotation */
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

        index = text.indexOf (quoteMark, index + quoteLength);
        while (isEscapedJavaQuote (text, index, true)
               || isJavaStartQuoteMLCommented (text, index, endIndex + 1))
        {
            index = text.indexOf (quoteMark, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteMark, index + 1);
    }
}
/*************************/
void Highlighter::singleLineJavaComment (const QString &text, const int start)
{
    for (const HighlightingRule &rule : qAsConst (highlightingRules))
    {
        if (rule.format == commentFormat)
        {
            int startIndex = qMax (start, 0);
            startIndex = text.indexOf (rule.pattern, startIndex);
            /* skip quoted comments (and, automatically, those inside multiline python comments) */
            while (startIndex > -1
                       /* check quote formats (only for multiLineJavaComment()) */
                   && (format (startIndex) == quoteFormat
                       || format (startIndex) == urlInsideQuoteFormat
                       /* check whether the comment sign is quoted or inside regex */
                       || isJavaSingleCommentQuoted (text, startIndex, qMax (start, 0))))
            {
                startIndex = text.indexOf (rule.pattern, startIndex + 1);
            }
            if (startIndex > -1)
            {
                int l = text.length();
                setFormat (startIndex, l - startIndex, commentFormat);

                /* also format urls and email addresses inside the comment */
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
                QString str = text.mid (startIndex, l - startIndex);
#else
                QString str = text.sliced (startIndex, l - startIndex);
#endif
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
            break;
        }
    }
}
/*************************/
void Highlighter::multiLineJavaComment (const QString &text)
{
    static const QRegularExpression javaDocTags ("@(author|deprecated|exception|param|return|see|serial|serialData|serialField|since|throws|version)|\\{@(code|docRoot|inheritDoc|link|linkplain|value)\\s[^\\}]*\\}");
    static const QRegularExpression javaDocBracedTags ("\\{@(code|docRoot|inheritDoc|link|linkplain|value)\\s[^\\}]*\\}");
    static const QRegularExpression descriptionEnd ("^\\s*\\**\\s*@(author|deprecated|exception|param|return|see|serial|serialData|serialField|since|throws|version)|\\.(?=\\s+|<br>|<p>|$)|<br>|<p>");
    static const QRegularExpression htmlTags ("</?(a|abbr|acronym|address|applet|area|article|aside|audio|b|base|basefont|bdi|bdo|big|blockquote|body|br|button|canvas|caption|center|cite|code|col|colgroup|data|datalist|dd|del|details|dfn|dialog|dir|div|dl|dt|em|embed|fieldset|figcaption|figure|font|footer|form|frame|frameset|h1|h2|h3|h4|h5|h6|head|header|hr|html|i|iframe|img|input|ins|kbd|label|legend|li|link|main|map|mark|meta|meter|nav|noframes|noscript|object|ol|optgroup|option|output|p|param|picture|pre|progress|q|rp|rt|ruby|s|samp|script|section|select|small|source|span|strike|strong|style|sub|summary|sup|svg|table|tbody|td|template|textarea|tfoot|th|thead|time|title|tr|track|tt|u|ul|var|video|wbr)\\s*>");

    int startIndex = 0;
    bool description = false;

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    int prevState = previousBlockState();
    if (prevState != commentState)
    {
        startIndex = text.indexOf (commentStartExpression, startIndex, &startMatch);
        /* skip quotations (all formatted to this point) */
        QTextCharFormat fi = format (startIndex);
        while (fi == quoteFormat || fi == urlInsideQuoteFormat)
        {
            startIndex = text.indexOf (commentStartExpression, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        /* skip single-line comments */
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            startIndex = -1;
        if (startIndex >= 0 && text.length() > startIndex + 2 && text.at (startIndex + 2) == '*')
            description = true;
    }
    else
    {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData());
            if (prevData && prevData->getProperty())
                description = true;
        }
    }

    while (startIndex >= 0)
    {
        int endIndex;
        /* when the comment start is in the prvious line
           and the search for the comment end has just begun... */
        if (prevState == commentState && startIndex == 0)
            /* ... search for the comment end from the line start */
            endIndex = text.indexOf (commentEndExpression, 0, &endMatch);
        else
            endIndex = text.indexOf (commentEndExpression,
                                     startIndex + startMatch.capturedLength(),
                                     &endMatch);

        /* skip quotations (needed?) */
        QTextCharFormat fi = format (endIndex);
        while (fi == quoteFormat || fi == urlInsideQuoteFormat)
        {
            endIndex = text.indexOf (commentEndExpression, endIndex + 1, &endMatch);
            fi = format (endIndex);
        }

        if (endIndex >= 0)
        {
            /* because multiline commnets weren't taken into account in
               singleLineJavaComment(), that method should be used here again */
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
            singleLineJavaComment (text, badIndex);
            /* because the previous single-line comment may have been
               removed now, quotes should be checked again from its start */
            if (hadSingleLineComment)
                JavaQuote (text, i);
        }

        int commentLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (commentState);
            commentLength = text.length() - startIndex;
        }
        else
            commentLength = endIndex - startIndex
                            + endMatch.capturedLength();
        setFormat (startIndex, commentLength, commentFormat);

        /* also find and format the description */
        int indx = -1;
        if (description)
        {
            indx = text.indexOf (descriptionEnd, startIndex);
            if (indx > -1 && (endIndex == - 1 || indx < endIndex))
                setFormat (startIndex, indx - startIndex, commentBoldFormat);
            else
            {
                if (endIndex == -1)
                {
                    setFormat (startIndex, text.length() - startIndex, commentBoldFormat);
                    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                        data->setProperty (true);
                }
                else
                    setFormat (startIndex, endIndex - startIndex, commentBoldFormat);
            }
        }

        /* highlight tags */
        if (!description || indx > -1)
        {
            int i = !description ? startIndex : indx;
            while ((i = text.indexOf (javaDocTags, i, &endMatch)) > -1 && i < startIndex + commentLength)
            {
                setFormat (i, endMatch.capturedLength(), regexFormat);
                i += endMatch.capturedLength();
            }
        }
        if (description)
        {
            if (indx == -1)
                indx = startIndex + commentLength;
            int i = startIndex;
            while ((i = text.indexOf (javaDocBracedTags, i, &endMatch)) > -1 && i < indx)
            {
                setFormat (i, endMatch.capturedLength(), regexFormat);
                i += endMatch.capturedLength();
            }
        }
        indx = startIndex;
        while ((indx = text.indexOf (htmlTags, indx, &endMatch)) > -1 && indx < startIndex + commentLength)
        {
            setFormat (indx, endMatch.capturedLength(), codeBlockFormat);
            indx += endMatch.capturedLength();
        }

        /* format urls and email addresses inside the comment */
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        QString str = text.mid (startIndex, commentLength);
#else
        QString str = text.sliced (startIndex, commentLength);
#endif
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

        startIndex = text.indexOf (commentStartExpression, startIndex + commentLength, &startMatch);

        /* skip single-line comments and quotations again */
        fi = format (startIndex);
        while (fi == quoteFormat || fi == urlInsideQuoteFormat)
        {
            startIndex = text.indexOf (commentStartExpression, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            startIndex = -1;
        if (startIndex >= 0 && text.length() > startIndex + 2 && text.at (startIndex + 2) == '*')
            description = true;
        else
            description = false;
    }
}
/*************************/
void Highlighter::javaMainFormatting (const QString &text)
{
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    if (data == nullptr) return;

    int index;
    QTextCharFormat fi;
    data->setHighlighted(); // completely highlighted
    for (const HighlightingRule &rule : qAsConst (highlightingRules))
    {
        /* single-line comments are already formatted */
        if (rule.format == commentFormat)
            continue;

        QRegularExpressionMatch match;
        index = text.indexOf (rule.pattern, 0, &match);
        /* skip quotes and all comments */
        if (rule.format != whiteSpaceFormat)
        {
            fi = format (index);
            while (index >= 0
                    && (fi == quoteFormat || fi == urlInsideQuoteFormat
                        || fi == commentFormat || fi == urlFormat
                        || fi == commentBoldFormat || fi== regexFormat || fi == codeBlockFormat))
            {
                index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                fi = format (index);
            }
        }

        while (index >= 0)
        {
            int length = match.capturedLength();
            setFormat (index, length, rule.format);

            index = text.indexOf (rule.pattern, index + length, &match);
            if (rule.format != whiteSpaceFormat)
            {
                fi = format (index);
                while (index >= 0
                        && (fi == quoteFormat || fi == urlInsideQuoteFormat
                            || fi == commentFormat || fi == urlFormat
                            || fi == commentBoldFormat || fi== regexFormat || fi == codeBlockFormat))
                {
                    index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    fi = format (index);
                }
            }
        }
    }
}
/*************************/
void Highlighter::javaBraces (const QString &text)
{
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    if (data == nullptr) return;

    /* NOTE: "altQuoteFormat" is used for characters in javaMainFormatting(). */

    /* left parenthesis */
    int index = text.indexOf ('(');
    QTextCharFormat fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf ('(', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = '(';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('(', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf ('(', index + 1);
            fi = format (index);
        }
    }

    /* right parenthesis */
    index = text.indexOf (')');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf (')', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = ')';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (')', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf (')', index + 1);
            fi = format (index);
        }
    }

    /* left brace */
    index = text.indexOf ('{');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf ('{', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '{';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('{', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf ('{', index + 1);
            fi = format (index);
        }
    }

    /* right brace */
    index = text.indexOf ('}');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf ('}', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '}';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('}', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf ('}', index + 1);
            fi = format (index);
        }
    }

    /* left bracket */
    index = text.indexOf ('[');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf ('[', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = '[';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('[', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf ('[', index + 1);
            fi = format (index);
        }
    }

    /* right bracket */
    index = text.indexOf (']');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == commentBoldFormat || fi== regexFormat))
    {
        index = text.indexOf (']', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = ']';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (']', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == commentBoldFormat || fi== regexFormat))
        {
            index = text.indexOf (']', index + 1);
            fi = format (index);
        }
    }
}

}
