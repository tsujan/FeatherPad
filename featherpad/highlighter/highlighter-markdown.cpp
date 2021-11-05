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

// This function formats Markdown's block quotes and code blocks.
// The start and end expressions always include the line start.
// The end expression includes the line end too.
bool Highlighter::markdownMultiLine (const QString &text,
                                     const QString &oldStartPattern,
                                     const QRegularExpression &startExp, const QRegularExpression &endExp,
                                     const int state,
                                     const QTextCharFormat &txtFormat)
{
    QRegularExpression endRegex;
    bool isBlockQuote = false;

    if (startExp.pattern().startsWith ("^\\s{0,"))
    {
        isBlockQuote = true;
        endRegex = endExp;
    }

    int prevState = previousBlockState();

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (prevState != state)
    {
        int startIndex = text.indexOf (startExp, 0, &startMatch);
        if (startIndex == -1)
            return false; // nothing to format
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat
            || format (startIndex) == quoteFormat)
        {
            return false; // this is a comment or quote
        }
    }

    bool res = false;

    if (isBlockQuote)
    {
        /* don't continue the previous block quote if this line is a list */
        if (prevState == state)
        {
            int extraBlockIndentation = 0;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            { // the label info is about indentation in this case
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    extraBlockIndentation = prevData->labelInfo().length();
            }
            QRegularExpression listRegex (QStringLiteral ("^ {0,")
                                          + QString::number (3 + extraBlockIndentation)
                                          + QStringLiteral ("}((\\*\\s+){1,}|(\\+\\s+){1,}|(\\-\\s+){1,}|\\d+\\.\\s+|\\d+\\)\\s+)"));
            if (text.indexOf (listRegex) > -1)
                return false;
        }
    }
    else
    {
        if (prevState == state)
        {
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            { // the label info is about end regex in this case
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    endRegex.setPattern (prevData->labelInfo());
            }
        }
        else
        { // get the end regex from the start regex
            QString str = startMatch.captured(); // is never empty
            str += QString (str.at (0));
            endRegex.setPattern (QStringLiteral ("^\\s*\\K") + str + QStringLiteral ("*(?!\\s*\\S)"));
        }
    }

    int endIndex = !isBlockQuote && prevState != state ? // the start of a code block can be ```
                       -1 : text.indexOf (endRegex, 0, &endMatch);
    int L;
    if (endIndex == -1)
    {
        L = text.length();
        setCurrentBlockState (state);
        if (!isBlockQuote)
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
            {
                data->insertInfo (endRegex.pattern());
                if (data->lastState() == state && oldStartPattern != endRegex.pattern())
                    res = true;
            }
        }
    }
    else
        L = endIndex + endMatch.capturedLength();
    setFormat (0, L, txtFormat);

    /* format urls and email addresses inside block quotes and code blocks */
    QString str = text.mid (0, L);
    int pIndex = 0;
    QRegularExpressionMatch urlMatch;
    while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
    {
        setFormat (pIndex, urlMatch.capturedLength(), urlFormat);
        pIndex += urlMatch.capturedLength();
    }
    /* format note patterns too */
    pIndex = 0;
    while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
    {
        if (format (pIndex) != urlFormat)
            setFormat (pIndex, urlMatch.capturedLength(), noteFormat);
        pIndex += urlMatch.capturedLength();
    }

    return res;
}
/*************************/
void Highlighter::markdownFonts (const QString &text)
{
    QTextCharFormat boldFormat = neutralFormat;
    boldFormat.setFontWeight (QFont::Bold);

    QTextCharFormat italicFormat = neutralFormat;
    italicFormat.setFontItalic (true);

    QTextCharFormat boldItalicFormat = italicFormat;
    boldItalicFormat.setFontWeight (QFont::Bold);

    /* NOTE: Apparently, all browsers use expressions similar to the following ones.
             However, these patterns aren't logical. For example, escaping an asterisk
             isn't always equivalent to its removal with regard to boldness/italicity.
             It also seems that five successive asterisks are ignored at start. */

    QRegularExpressionMatch italicMatch;
    static const QRegularExpression italicExp ("(?<!\\\\|\\*{4})\\*([^*]|(?:(?<!\\*)\\*\\*))+\\*|(?<!\\\\|_{4})_([^_]|(?:(?<!_)__))+_"); // allow double asterisks inside

    QRegularExpressionMatch boldcMatch;
    //const QRegularExpression boldExp ("\\*\\*(?!\\*)(?:(?!\\*\\*).)+\\*\\*|__(?:(?!__).)+__}");
    static const QRegularExpression boldExp ("(?<!\\\\|\\*{3})\\*\\*([^*]|(?:(?<!\\*)\\*))+\\*\\*|(?<!\\\\|_{3})__([^_]|(?:(?<!_)_))+__"); // allow single asterisks inside

    static const QRegularExpression boldItalicExp ("(?<!\\\\|\\*{2})\\*{3}([^*]|(?:(?<!\\*)\\*))+\\*{3}|(?<!\\\\|_{2})_{3}([^_]|(?:(?<!_)_))+_{3}");

    QRegularExpressionMatch expMatch;
    const QRegularExpression exp (boldExp.pattern() + "|" + italicExp.pattern() + "|" + boldItalicExp.pattern());

    int index = 0;
    while ((index = text.indexOf (exp, index, &expMatch)) > -1)
    {
        if (format (index) == mainFormat && format (index + expMatch.capturedLength() - 1) == mainFormat)
        {
            if (index == text.indexOf (boldItalicExp, index))
            {
                setFormatWithoutOverwrite (index, expMatch.capturedLength(), boldItalicFormat, whiteSpaceFormat);
            }
            else if (index == text.indexOf (boldExp, index, &boldcMatch) && boldcMatch.capturedLength() == expMatch.capturedLength())
            {
                setFormatWithoutOverwrite (index, expMatch.capturedLength(), boldFormat, whiteSpaceFormat);
                /* also format italic bold strings */
                QString str = text.mid (index + 2, expMatch.capturedLength() - 4);
                int indx = 0;
                while ((indx = str.indexOf (italicExp, indx, &italicMatch)) > -1)
                {
                    setFormatWithoutOverwrite (index + 2 + indx, italicMatch.capturedLength(), boldItalicFormat, whiteSpaceFormat);
                    indx += italicMatch.capturedLength();
                }
            }
            else
            {
                setFormatWithoutOverwrite (index, expMatch.capturedLength(), italicFormat, whiteSpaceFormat);
                /* also format bold italic strings */
                QString str = text.mid (index + 1, expMatch.capturedLength() - 2);
                int indx = 0;
                while ((indx = str.indexOf (boldExp, indx, &boldcMatch)) > -1)
                {
                    setFormatWithoutOverwrite (index + 1 + indx, boldcMatch.capturedLength(), boldItalicFormat, whiteSpaceFormat);
                    indx += boldcMatch.capturedLength();
                }

            }
            index += expMatch.capturedLength();
        }
        else ++index;
    }
}
/*************************/
void Highlighter::highlightMarkdownBlock (const QString &text)
{
    bool rehighlightNextBlock = false;
    bool oldProperty = false;
    QString oldLabel;
    if (TextBlockData *oldData = static_cast<TextBlockData *>(currentBlockUserData()))
    {
        oldProperty = oldData->getProperty();
        oldLabel = oldData->labelInfo();
    }

    int index;
    TextBlockData *data = new TextBlockData;
    data->setLastState (currentBlockState());
    setCurrentBlockUserData (data);
    setCurrentBlockState (0);
    QTextCharFormat fi;

    rehighlightNextBlock |= multiLineQuote (text);
    rehighlightNextBlock |= multiLineComment (text, 0,
                                              commentStartExpression, commentEndExpression,
                                              commentState, commentFormat);

    int prevState = previousBlockState();
    QTextBlock prevBlock = currentBlock().previous();
    bool prevProperty = false;
    QString prevLabel;
    if (prevBlock.isValid())
    {
        if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
        {
            prevProperty = prevData->getProperty();
            prevLabel = prevData->labelInfo();
        }
    }
    int extraBlockIndentation = prevLabel.length();
    /* determine whether this line is inside a list */
    if (!prevLabel.isEmpty())
    {
        if (prevBlock.text().indexOf (QRegularExpression ("\\S")) > -1)
            data->insertInfo (prevLabel);
        else
        {
            QRegularExpressionMatch spacesMatch;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
            text.indexOf (QRegularExpression ("^\\s+"), 0, &spacesMatch);
#else
            (void)text.indexOf (QRegularExpression ("^\\s+"), 0, &spacesMatch);
#endif
            if (spacesMatch.capturedLength() == text.length()
                || spacesMatch.capturedLength() >= prevLabel.length())
            {
                data->insertInfo (prevLabel);
            }
        }
    }

    /* a block quote shouldn't be formatted inside a real comment */
    if (prevState != commentState)
    {
        markdownMultiLine (text, QString(),
                           QRegularExpression (QStringLiteral ("^\\s{0,")
                                               + QString::number (3 + extraBlockIndentation)
                                               + QStringLiteral ("}>.*")),
                           QRegularExpression ("^$"),
                           markdownBlockQuoteState, blockQuoteFormat);
    }
    /* the ``` code block shouldn't be formatted inside a comment or block quote */
    if (prevState != commentState && prevState != markdownBlockQuoteState)
    {
        static const QRegularExpression codeStartRegex ("^ {0,3}\\K(`{3,}(?!`)|~{3,}(?!~))");
        rehighlightNextBlock |= markdownMultiLine (text, oldLabel,
                                                   codeStartRegex,
                                                   QRegularExpression(),
                                                   codeBlockState, codeBlockFormat);
    }

    if (currentBlockState() != markdownBlockQuoteState && currentBlockState() != codeBlockState)
    {
        QRegularExpressionMatch match;

        /* list start */
        QTextCharFormat markdownFormat;
        markdownFormat.setFontWeight (QFont::Bold);
        markdownFormat.setForeground (DarkBlue);
        QRegularExpression listRegex (QStringLiteral ("^ {0,")
                                      + QString::number (3 + extraBlockIndentation)
                                      + QStringLiteral ("}((\\*\\s+){1,}|(\\+\\s+){1,}|(\\-\\s+){1,}|\\d+\\.\\s+|\\d+\\)\\s+)"));
        index = text.indexOf (listRegex, 0, &match);
        fi = format (index);
        while (index >= 0
               && (fi == blockQuoteFormat || fi == codeBlockFormat // the same as quoteFormat (for `...`)
                   || fi == commentFormat || fi == urlFormat))
        {
            index = text.indexOf (listRegex, index + match.capturedLength(), &match);
            fi = format (index);
        }
        if (index >= 0)
        {
            QString spaces;
            for (int i = 0; i < match.capturedLength(); ++i)
                spaces += " ";
            data->insertInfo (spaces);
            setFormat (index, match.capturedLength(), markdownFormat);
        }

        /* code block with indentation */
        static const QRegularExpression codeRegex ("^( {4,}|\\s*\\t+\\s*).*");
        index = text.indexOf (codeRegex, 0, &match);
        fi = format (index);
        while (index >= 0
               && (fi == blockQuoteFormat || fi == codeBlockFormat // the same as quoteFormat (for `...`)
                   || fi == commentFormat || fi == urlFormat))
        {
            index = text.indexOf (codeRegex, index + match.capturedLength(), &match);
            fi = format (index);
        }
        if (index >= 0)
        {
            /* when this is about a code block with indentation
                but the current line can't be code block */
            const QString prevTxt = prevBlock.text();
            if ((text.left (4 + extraBlockIndentation)).indexOf (QRegularExpression ("\\S")) > -1
                || (prevTxt.left (4 + prevLabel.length())).indexOf (QRegularExpression ("\\S")) > -1
                || (prevTxt.indexOf (QRegularExpression ("\\S")) > -1 && !prevProperty))
            {
                if (oldProperty)
                    rehighlightNextBlock = true;
            }
            else
            {
                setFormat (index, match.capturedLength(), codeBlockFormat);
                data->setProperty (true);
                if (!oldProperty)
                    rehighlightNextBlock = true;
            }
        }
    }

    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber())
    {
        data->setHighlighted(); // completely highlighted
        for (const HighlightingRule &rule : qAsConst (highlightingRules))
        {
            QRegularExpressionMatch match;
            index = text.indexOf (rule.pattern, 0, &match);
            if (rule.format != whiteSpaceFormat)
            {
                if (currentBlockState() == markdownBlockQuoteState || currentBlockState() == codeBlockState)
                    continue;
                fi = format (index);
                while (index >= 0
                       && (fi == blockQuoteFormat || fi == codeBlockFormat // the same as quoteFormat (for `...`)
                           || fi == commentFormat || fi == urlFormat))
                {
                    index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    fi = format (index);
                }
            }

            while (index >= 0)
            {
                setFormat (index, match.capturedLength(), rule.format);
                index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                if (rule.format != whiteSpaceFormat)
                {
                    fi = format (index);
                    while (index >= 0
                           && (fi == blockQuoteFormat || fi == codeBlockFormat
                               || fi == commentFormat || fi == urlFormat))
                    {
                        index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                        fi = format (index);
                    }
                }
            }
        }

        markdownFonts (text);
    }

    /* if this line isn't a code block with indentation
        but the next line was, rehighlight the next line */
    if (!rehighlightNextBlock && !data->getProperty())
    {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid())
        {
            if (TextBlockData *nextData = static_cast<TextBlockData *>(nextBlock.userData()))
            {
                if (nextData->getProperty())
                    rehighlightNextBlock = true;
            }
        }
    }
    if (!rehighlightNextBlock)
    {
        if (data->labelInfo().length() != oldLabel.length())
            rehighlightNextBlock = true;
        else if (!data->labelInfo().isEmpty())
        {
            QTextBlock nextBlock = currentBlock().next();
            if (nextBlock.isValid())
            {
                if (TextBlockData *nextData = static_cast<TextBlockData *>(nextBlock.userData()))
                {
                    if (nextData->labelInfo() != data->labelInfo())
                        rehighlightNextBlock = true;
                }
            }
        }
    }

    /* left parenthesis */
    index = text.indexOf ('(');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
        {
            index = text.indexOf (']', index + 1);
            fi = format (index);
        }
    }

    setCurrentBlockUserData (data);

    if (rehighlightNextBlock)
    {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid())
            QMetaObject::invokeMethod (this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG (QTextBlock, nextBlock));
    }
}

}
